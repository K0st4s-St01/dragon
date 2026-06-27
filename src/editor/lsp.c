#include "dragon_editor/lsp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

/* Helper: Extract JSON string value */
static char *json_extract_string(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *pos = strstr(json, search);
    if (!pos) return NULL;
    
    pos += strlen(search);
    const char *end = strchr(pos, '"');
    if (!end) return NULL;
    
    int len = end - pos;
    char *result = malloc(len + 1);
    memcpy(result, pos, len);
    result[len] = '\0';
    return result;
}

/* Helper: Extract JSON number value */
static int json_extract_number(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *pos = strstr(json, search);
    if (!pos) return -1;
    
    pos += strlen(search);
    return (int)strtol(pos, NULL, 10);
}

void lsp_manager_init(LSPManager *manager) {
    manager->clients = NULL;
    manager->client_count = 0;
    manager->client_capacity = 0;
}

void lsp_manager_free(LSPManager *manager) {
    for (int i = 0; i < manager->client_count; i++) {
        lsp_client_stop(&manager->clients[i]);
        free(manager->clients[i].language_id);
        free(manager->clients[i].config.path);
        if (manager->clients[i].config.args) {
            for (int j = 0; j < manager->clients[i].config.args_count; j++) {
                free(manager->clients[i].config.args[j]);
            }
            free(manager->clients[i].config.args);
        }
    }
    free(manager->clients);
    manager->clients = NULL;
    manager->client_count = 0;
    manager->client_capacity = 0;
}

void lsp_manager_add_server(LSPManager *manager, const char *language_id,
                            const char *server_path, const char **args, int args_count) {
    if (manager->client_count >= manager->client_capacity) {
        manager->client_capacity = (manager->client_capacity == 0) ? 8 : manager->client_capacity * 2;
        manager->clients = realloc(manager->clients, 
                                   manager->client_capacity * sizeof(LSPClient));
    }

    LSPClient *client = &manager->clients[manager->client_count];
    memset(client, 0, sizeof(*client));
    
    client->language_id = malloc(strlen(language_id) + 1);
    strcpy(client->language_id, language_id);
    
    client->config.path = malloc(strlen(server_path) + 1);
    strcpy(client->config.path, server_path);
    
    client->config.args_count = args_count;
    if (args_count > 0) {
        client->config.args = malloc(args_count * sizeof(char *));
        for (int i = 0; i < args_count; i++) {
            client->config.args[i] = malloc(strlen(args[i]) + 1);
            strcpy(client->config.args[i], args[i]);
        }
    }
    
    client->status = LSP_STATUS_DISCONNECTED;
    client->id = 1;
    client->pid = -1;
    client->stdin_fd = -1;
    client->stdout_fd = -1;
    client->initialized = false;
    
    manager->client_count++;
}

bool lsp_client_start(LSPClient *client) {
    if (client->status != LSP_STATUS_DISCONNECTED) {
        return client->status == LSP_STATUS_INITIALIZED;
    }
    
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
        client->status = LSP_STATUS_ERROR;
        return false;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        client->status = LSP_STATUS_ERROR;
        return false;
    }
    
    if (pid == 0) {
        /* Child process */
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        
        int argc = client->config.args_count + 2;
        char **argv = malloc(argc * sizeof(char *));
        argv[0] = client->config.path;
        for (int i = 0; i < client->config.args_count; i++) {
            argv[i + 1] = client->config.args[i];
        }
        argv[argc - 1] = NULL;
        
        execv(client->config.path, argv);
        exit(1);
    } else {
        /* Parent process */
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        
        client->pid = pid;
        client->stdin_fd = stdin_pipe[1];
        client->stdout_fd = stdout_pipe[0];
        
        fcntl(client->stdout_fd, F_SETFL, O_NONBLOCK);
        
        client->status = LSP_STATUS_CONNECTING;
        return true;
    }
}

void lsp_client_stop(LSPClient *client) {
    if (client->status == LSP_STATUS_DISCONNECTED) {
        return;
    }
    
    if (client->pid > 0) {
        kill(client->pid, SIGTERM);
        waitpid(client->pid, NULL, 0);
        client->pid = -1;
    }
    
    if (client->stdin_fd >= 0) {
        close(client->stdin_fd);
        client->stdin_fd = -1;
    }
    
    if (client->stdout_fd >= 0) {
        close(client->stdout_fd);
        client->stdout_fd = -1;
    }
    
    client->status = LSP_STATUS_DISCONNECTED;
    client->initialized = false;
}

bool lsp_client_initialize(LSPClient *client) {
    if (client->initialized) {
        return true;
    }
    
    if (client->status == LSP_STATUS_DISCONNECTED) {
        if (!lsp_client_start(client)) {
            return false;
        }
    }
    
    /* Send initialize request */
    const char *init_params = "{"
        "\"processId\":null,"
        "\"rootPath\":null,"
        "\"capabilities\":{"
            "\"textDocument\":{"
                "\"synchronization\":{\"didSave\":true},"
                "\"semanticTokens\":{\"requests\":{\"full\":true}}"
            "}"
        "}"
    "}";
    
    char buffer[4096];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"initialize\",\"params\":%s}",
             client->id++, init_params);
    
    char header[256];
    int content_len = strlen(buffer);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, buffer, content_len);
    
    /* Send initialized notification */
    const char *initialized_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}";
    content_len = strlen(initialized_msg);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, initialized_msg, content_len);
    
    client->status = LSP_STATUS_INITIALIZED;
    client->initialized = true;
    
    return true;
}

LSPStatus lsp_client_get_status(LSPClient *client) {
    return client->status;
}

void lsp_client_send_definition_request(LSPClient *client, const char *file_uri, int line, int character) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }
    
    char params[512];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d}}",
             file_uri, line, character);
    
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/definition\",\"params\":%s}",
             client->id++, params);
    
    char header[256];
    int content_len = strlen(buffer);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, buffer, content_len);
}

void lsp_client_send_hover_request(LSPClient *client, const char *file_uri, int line, int character) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }
    
    char params[512];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d}}",
             file_uri, line, character);
    
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/hover\",\"params\":%s}",
             client->id++, params);
    
    char header[256];
    int content_len = strlen(buffer);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, buffer, content_len);
}

void lsp_client_send_semantic_tokens_request(LSPClient *client, const char *file_uri) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }
    
    char params[512];
    snprintf(params, sizeof(params), "{\"textDocument\":{\"uri\":\"%s\"}}", file_uri);
    
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/semanticTokens/full\",\"params\":%s}",
             client->id++, params);
    
    char header[256];
    int content_len = strlen(buffer);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, buffer, content_len);
}

void lsp_client_send_rename_request(LSPClient *client, const char *file_uri, int line, int character, const char *new_name) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }
    
    char params[1024];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d},\"newName\":\"%s\"}",
             file_uri, line, character, new_name);
    
    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/rename\",\"params\":%s}",
             client->id++, params);
    
    char header[256];
    int content_len = strlen(buffer);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, buffer, content_len);
}

void lsp_client_send_code_action_request(LSPClient *client, const char *file_uri, int start_line, int start_character, int end_line, int end_character) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }
    
    char params[1024];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\"},\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}},\"context\":{\"diagnostics\":[]}}",
             file_uri, start_line, start_character, end_line, end_character);
    
    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/codeAction\",\"params\":%s}",
             client->id++, params);
    
    char header[256];
    int content_len = strlen(buffer);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, buffer, content_len);
}

void lsp_client_send_type_definition_request(LSPClient *client, const char *file_uri, int line, int character) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }
    
    char params[512];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d}}",
             file_uri, line, character);
    
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/typeDefinition\",\"params\":%s}",
             client->id++, params);
    
    char header[256];
    int content_len = strlen(buffer);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, buffer, content_len);
}

void lsp_client_send_references_request(LSPClient *client, const char *file_uri, int line, int character) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }
    
    char params[512];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d},\"context\":{\"includeDeclaration\":true}}",
             file_uri, line, character);
    
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/references\",\"params\":%s}",
             client->id++, params);
    
    char header[256];
    int content_len = strlen(buffer);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, buffer, content_len);
}

void lsp_client_send_implementation_request(LSPClient *client, const char *file_uri, int line, int character) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }
    
    char params[512];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d}}",
             file_uri, line, character);
    
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/implementation\",\"params\":%s}",
             client->id++, params);
    
    char header[256];
    int content_len = strlen(buffer);
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    
    write(client->stdin_fd, header, strlen(header));
    write(client->stdin_fd, buffer, content_len);
}

char *lsp_client_read_response(LSPClient *client) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return NULL;
    }
    
    char buffer[8192];
    int bytes_read = read(client->stdout_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        return NULL;
    }
    
    buffer[bytes_read] = '\0';
    
    /* Parse Content-Length header */
    int content_len = 0;
    if (sscanf(buffer, "Content-Length: %d", &content_len) != 1) {
        return NULL;
    }
    
    if (content_len <= 0 || content_len > sizeof(buffer)) {
        return NULL;
    }
    
    /* Find body start */
    const char *body_start = strstr(buffer, "\r\n\r\n");
    if (!body_start) {
        return NULL;
    }
    
    body_start += 4;
    
    /* Allocate and copy response */
    char *response = malloc(content_len + 1);
    int header_len = body_start - buffer;
    int already_read = bytes_read - header_len;
    
    if (already_read > 0) {
        memcpy(response, body_start, already_read);
    }
    
    /* Read remaining if needed */
    int offset = already_read;
    while (offset < content_len) {
        bytes_read = read(client->stdout_fd, response + offset, content_len - offset);
        if (bytes_read <= 0) break;
        offset += bytes_read;
    }
    
    response[offset] = '\0';
    return response;
}

LSPLocation *lsp_parse_definition_response(const char *response, int *count) {
    if (!response || !count) return NULL;
    
    *count = 0;
    
    /* Simple parser for Location or Location[] response */
    /* {"jsonrpc":"2.0","id":N,"result":{"uri":"file://...","range":{"start":{"line":N,"character":N},"end":...}}} */
    
    const char *result = strstr(response, "\"result\":");
    if (!result) return NULL;
    
    result += strlen("\"result\":");
    
    /* Check if it's an array or single location */
    int location_count = 1;
    if (*result == '[') {
        /* Count elements */
        const char *p = result;
        while (*p && *p != ']') {
            if (*p == '{') location_count++;
            p++;
        }
    } else if (*result != '{') {
        return NULL;
    }
    
    LSPLocation *locations = malloc(location_count * sizeof(LSPLocation));
    memset(locations, 0, location_count * sizeof(LSPLocation));
    
    /* Parse locations */
    const char *pos = result;
    int idx = 0;
    
    while (*pos && idx < location_count) {
        pos = strchr(pos, '{');
        if (!pos) break;
        
        pos++;
        
        /* Extract uri */
        char *uri = json_extract_string(pos, "uri");
        if (uri) {
            locations[idx].uri = uri;
        }
        
        /* Extract range start line */
        const char *range_start = strstr(pos, "\"start\":");
        if (range_start) {
            locations[idx].range.start.line = json_extract_number(range_start, "line");
            locations[idx].range.start.character = json_extract_number(range_start, "character");
        }
        
        /* Extract range end line */
        const char *range_end = strstr(pos, "\"end\":");
        if (range_end) {
            locations[idx].range.end.line = json_extract_number(range_end, "line");
            locations[idx].range.end.character = json_extract_number(range_end, "character");
        }
        
        idx++;
    }
    
    *count = idx;
    return idx > 0 ? locations : NULL;
}

void lsp_free_locations(LSPLocation *locations, int count) {
    if (!locations) return;
    for (int i = 0; i < count; i++) {
        free(locations[i].uri);
    }
    free(locations);
}

LSPHover *lsp_parse_hover_response(const char *response) {
    if (!response) return NULL;
    
    /* Parse: {"result": {"contents": "..."}} or {"result": null} */
    LSPHover *hover = malloc(sizeof(LSPHover));
    memset(hover, 0, sizeof(LSPHover));
    
    /* Check if result is null */
    const char *result_start = strstr(response, "\"result\"");
    if (!result_start) {
        return hover;
    }
    
    /* Look for null result */
    if (strstr(result_start, "\"result\":null") || strstr(result_start, "\"result\": null")) {
        return hover;
    }
    
    /* Find contents field */
    const char *contents_start = strstr(result_start, "\"contents\"");
    if (!contents_start) {
        return hover;
    }
    
    /* Find the string value */
    const char *value_start = strchr(contents_start, '"');
    if (!value_start || value_start[1] == '"') {
        return hover;
    }
    value_start++; /* Skip opening quote */
    
    /* Find closing quote (simple approach - doesn't handle escaped quotes) */
    const char *value_end = strchr(value_start, '"');
    if (!value_end) {
        return hover;
    }
    
    size_t len = value_end - value_start;
    if (len > 0 && len < 8192) {
        hover->contents = malloc(len + 1);
        memcpy(hover->contents, value_start, len);
        hover->contents[len] = '\0';
    }
    
    return hover;
}

void lsp_free_hover(LSPHover *hover) {
    if (!hover) return;
    free(hover->contents);
    free(hover->language);
    free(hover);
}

void lsp_free_workspace_edit(LSPWorkspaceEdit *edit) {
    if (!edit) return;
    if (edit->changes) {
        for (int i = 0; i < edit->count; i++) {
            free(edit->changes[i].new_text);
        }
        free(edit->changes);
    }
    free(edit);
}

const char *lsp_hover_get_contents(LSPHover *hover) {
    if (!hover) return NULL;
    return hover->contents;
}

LSPClient *lsp_manager_get_client(LSPManager *manager, const char *language_id) {
    for (int i = 0; i < manager->client_count; i++) {
        if (strcmp(manager->clients[i].language_id, language_id) == 0) {
            if (manager->clients[i].status != LSP_STATUS_INITIALIZED) {
                if (!lsp_client_initialize(&manager->clients[i])) {
                    continue;
                }
            }
            return &manager->clients[i];
        }
    }
    return NULL;
}

LSPDiagnostics *lsp_parse_diagnostics_response(const char *response) {
    if (!response) return NULL;
    
    LSPDiagnostics *diag = malloc(sizeof(LSPDiagnostics));
    memset(diag, 0, sizeof(LSPDiagnostics));
    
    /* Parse: {"result": [{...}, {...}]} */
    const char *result = strstr(response, "\"result\"");
    if (!result) return diag;
    
    /* Count diagnostic objects */
    int count = 0;
    const char *p = strchr(result, '[');
    if (!p) return diag;
    
    while (p) {
        p = strchr(p + 1, '{');
        if (!p || p[1] == ']') break;
        count++;
    }
    
    if (count == 0) return diag;
    
    diag->items = malloc(count * sizeof(LSPDiagnostic));
    memset(diag->items, 0, count * sizeof(LSPDiagnostic));
    diag->count = 0;
    
    /* Parse each diagnostic */
    p = strchr(result, '[');
    if (!p) return diag;
    
    for (int i = 0; i < count && i < 100; i++) {
        /* Find start of this diagnostic */
        p = strchr(p, '{');
        if (!p) break;
        
        LSPDiagnostic *d = &diag->items[i];
        
        /* Parse range.start.line */
        const char *line_str = strstr(p, "\"line\"");
        if (line_str) {
            sscanf(line_str + 6, "%d", &d->start_line);
        }
        
        /* Parse range.start.character */
        const char *char_str = strstr(p, "\"character\"");
        if (char_str) {
            sscanf(char_str + 11, "%d", &d->start_col);
        }
        
        /* Parse severity (1=error, 2=warning, 3=info, 4=hint) */
        const char *sev_str = strstr(p, "\"severity\"");
        if (sev_str) {
            int sev = 1;
            sscanf(sev_str + 10, "%d", &sev);
            d->severity = sev;
        } else {
            d->severity = LSP_DIAG_ERROR;
        }
        
        /* Parse message */
        const char *msg_start = strstr(p, "\"message\"");
        if (msg_start) {
            const char *quote = strchr(msg_start + 9, '"');
            if (quote) {
                quote++;
                const char *quote_end = strchr(quote, '"');
                if (quote_end && quote_end - quote < 256) {
                    d->message = malloc(quote_end - quote + 1);
                    memcpy(d->message, quote, quote_end - quote);
                    d->message[quote_end - quote] = '\0';
                }
            }
        }
        
        diag->count++;
        p++;
    }
    
    return diag;
}

void lsp_free_diagnostics(LSPDiagnostics *diag) {
    if (!diag) return;
    if (diag->items) {
        for (int i = 0; i < diag->count; i++) {
            free(diag->items[i].message);
            free(diag->items[i].code);
        }
        free(diag->items);
    }
    free(diag);
}

LSPDiagnostics *lsp_parse_publish_diagnostics_notification(const char *response) {
    if (!response) return NULL;
    
    /* Check if this is a publishDiagnostics notification */
    if (!strstr(response, "textDocument/publishDiagnostics")) {
        return NULL;
    }
    
    LSPDiagnostics *diag = malloc(sizeof(LSPDiagnostics));
    memset(diag, 0, sizeof(LSPDiagnostics));
    
    /* Parse: {"method":"textDocument/publishDiagnostics","params":{"uri":"...","diagnostics":[...]}} */
    const char *diagnostics_start = strstr(response, "\"diagnostics\"");
    if (!diagnostics_start) return diag;
    
    /* Count diagnostic objects */
    int count = 0;
    const char *p = strchr(diagnostics_start, '[');
    if (!p) return diag;
    
    while (p) {
        p = strchr(p + 1, '{');
        if (!p || p[1] == ']') break;
        count++;
    }
    
    if (count == 0) return diag;
    
    diag->items = malloc(count * sizeof(LSPDiagnostic));
    memset(diag->items, 0, count * sizeof(LSPDiagnostic));
    diag->count = 0;
    
    /* Parse each diagnostic */
    p = strchr(diagnostics_start, '[');
    if (!p) return diag;
    
    for (int i = 0; i < count && i < 100; i++) {
        /* Find start of this diagnostic */
        p = strchr(p, '{');
        if (!p) break;
        
        LSPDiagnostic *d = &diag->items[i];
        
        /* Parse range.start.line */
        const char *range_str = strstr(p, "\"range\"");
        if (range_str) {
            const char *line_str = strstr(range_str, "\"line\"");
            if (line_str) {
                sscanf(line_str + 6, "%d", &d->start_line);
            }
            
            /* Parse range.start.character */
            const char *char_str = strstr(range_str, "\"character\"");
            if (char_str) {
                sscanf(char_str + 11, "%d", &d->start_col);
            }
        }
        
        /* Parse severity (1=error, 2=warning, 3=info, 4=hint) */
        const char *sev_str = strstr(p, "\"severity\"");
        if (sev_str) {
            int sev = 1;
            sscanf(sev_str + 10, "%d", &sev);
            d->severity = sev;
        } else {
            d->severity = LSP_DIAG_ERROR;
        }
        
        /* Parse message */
        const char *msg_start = strstr(p, "\"message\"");
        if (msg_start) {
            const char *quote = strchr(msg_start + 9, '"');
            if (quote) {
                quote++;
                const char *quote_end = strchr(quote, '"');
                if (quote_end && quote_end - quote < 256) {
                    d->message = malloc(quote_end - quote + 1);
                    memcpy(d->message, quote, quote_end - quote);
                    d->message[quote_end - quote] = '\0';
                }
            }
        }
        
        diag->count++;
        p++;
    }
    
    return diag;
}

LSPWorkspaceEdit *lsp_parse_rename_response(const char *response) {
    if (!response) return NULL;
    
    /* Parse: {"result": {"changes": {"file://...": [{"range": {...}, "newText": "..."}]}}} */
    LSPWorkspaceEdit *edit = malloc(sizeof(LSPWorkspaceEdit));
    memset(edit, 0, sizeof(LSPWorkspaceEdit));
    
    /* Check if result exists and is not null */
    const char *result_start = strstr(response, "\"result\"");
    if (!result_start) return edit;
    
    if (strstr(result_start, "\"result\":null") || strstr(result_start, "\"result\": null")) {
        return edit;
    }
    
    /* Find changes array - allocate reasonable capacity for edits */
    const char *changes_start = strstr(result_start, "\"changes\"");
    if (!changes_start) return edit;
    
    /* Pre-allocate space for edits (max 100 edits per file) */
    edit->changes = malloc(sizeof(LSPTextEdit) * 100);
    edit->count = 0;
    
    /* Simple parser: find all "newText" values and corresponding ranges */
    const char *p = changes_start;
    while ((p = strstr(p, "\"newText\"")) && edit->count < 100) {
        /* Find the quoted string value */
        const char *colon = strchr(p, ':');
        if (!colon) break;
        
        const char *quote_start = strchr(colon, '"');
        if (!quote_start) break;
        quote_start++;
        
        const char *quote_end = quote_start;
        /* Simple unescaping - find closing quote */
        while (*quote_end && *quote_end != '"') {
            if (*quote_end == '\\') quote_end += 2;
            else quote_end++;
        }
        
        if (*quote_end != '"') break;
        
        size_t text_len = quote_end - quote_start;
        if (text_len > 0 && text_len < 512) {
            edit->changes[edit->count].new_text = malloc(text_len + 1);
            memcpy(edit->changes[edit->count].new_text, quote_start, text_len);
            edit->changes[edit->count].new_text[text_len] = '\0';
            
            /* For simplicity, mark range as [0,0] to [0,0] - document will handle application */
            edit->changes[edit->count].range.start.line = 0;
            edit->changes[edit->count].range.start.character = 0;
            edit->changes[edit->count].range.end.line = 0;
            edit->changes[edit->count].range.end.character = 0;
            
            edit->count++;
        }
        
        p = quote_end + 1;
    }
    
    if (edit->count == 0) {
        free(edit->changes);
        edit->changes = NULL;
    }
    
    return edit;
}

LSPCodeActions *lsp_parse_code_actions_response(const char *response) {
    if (!response) return NULL;
    
    /* Parse: {"result": [{"title": "...", "kind": "...", "edit": {...}}]} */
    LSPCodeActions *actions = malloc(sizeof(LSPCodeActions));
    memset(actions, 0, sizeof(LSPCodeActions));
    
    /* Check if result exists and is not null */
    const char *result_start = strstr(response, "\"result\"");
    if (!result_start) return actions;
    
    if (strstr(result_start, "\"result\":null") || strstr(result_start, "\"result\": null")) {
        return actions;
    }
    
    /* Find array start */
    const char *array_start = strchr(result_start, '[');
    if (!array_start) return actions;
    
    /* Pre-allocate space for code actions (max 50) */
    actions->actions = malloc(sizeof(LSPCodeAction) * 50);
    actions->count = 0;
    
    /* Simple parser: find all "title" fields */
    const char *p = array_start;
    while ((p = strstr(p, "\"title\"")) && actions->count < 50) {
        /* Find the quoted string value */
        const char *colon = strchr(p, ':');
        if (!colon) break;
        
        const char *quote_start = strchr(colon, '"');
        if (!quote_start) break;
        quote_start++;
        
        const char *quote_end = quote_start;
        while (*quote_end && *quote_end != '"') {
            if (*quote_end == '\\') quote_end += 2;
            else quote_end++;
        }
        
        if (*quote_end != '"') break;
        
        size_t title_len = quote_end - quote_start;
        if (title_len > 0 && title_len < 256) {
            actions->actions[actions->count].title = malloc(title_len + 1);
            memcpy(actions->actions[actions->count].title, quote_start, title_len);
            actions->actions[actions->count].title[title_len] = '\0';
            
            actions->actions[actions->count].kind = NULL;  /* Could parse kind too */
            actions->actions[actions->count].edit = NULL;  /* Could parse edit too */
            
            actions->count++;
        }
        
        p = quote_end + 1;
    }
    
    if (actions->count == 0) {
        free(actions->actions);
        actions->actions = NULL;
    }
    
    return actions;
}

void lsp_free_code_actions(LSPCodeActions *actions) {
    if (!actions) return;
    if (actions->actions) {
        for (int i = 0; i < actions->count; i++) {
            free(actions->actions[i].title);
            free(actions->actions[i].kind);
            if (actions->actions[i].edit) {
                lsp_free_workspace_edit(actions->actions[i].edit);
            }
        }
        free(actions->actions);
    }
    free(actions);
}
