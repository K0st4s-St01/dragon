#include "dragon_editor/lsp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

static const char *find_bytes(const char *haystack, size_t haystack_len,
                              const char *needle, size_t needle_len) {
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len) return NULL;
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack + i, needle, needle_len) == 0)
            return haystack + i;
    }
    return NULL;
}

static bool lsp_client_buffer_append(LSPClient *client, const char *data, size_t len) {
    if (!client || !data || len == 0) return true;
    if (client->read_len + len + 1 > client->read_capacity) {
        size_t next = client->read_capacity ? client->read_capacity : 16384;
        while (next < client->read_len + len + 1)
            next *= 2;
        char *buf = realloc(client->read_buffer, next);
        if (!buf) return false;
        client->read_buffer = buf;
        client->read_capacity = next;
    }
    memcpy(client->read_buffer + client->read_len, data, len);
    client->read_len += len;
    client->read_buffer[client->read_len] = '\0';
    return true;
}

static bool lsp_client_write_all(LSPClient *client, const char *data, size_t len) {
    if (!client || client->stdin_fd < 0 || !data) return false;
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(client->stdin_fd, data + written, len - written);
        if (n > 0) {
            written += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EPIPE || errno == EBADF)) {
            lsp_client_stop(client);
            client->status = LSP_STATUS_ERROR;
        }
        return false;
    }
    return true;
}

static bool lsp_client_write_message(LSPClient *client, const char *body, int content_len) {
    char header[256];
    snprintf(header, sizeof(header), "Content-Length: %d\r\n\r\n", content_len);
    return lsp_client_write_all(client, header, strlen(header)) &&
           lsp_client_write_all(client, body, (size_t)content_len);
}

/* Helper: Extract JSON string value */
static char *json_extract_string(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) return NULL;
    
    pos += strlen(search);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')) pos++;
    if (*pos != ':') return NULL;
    pos++;
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')) pos++;
    if (*pos != '"') return NULL;
    pos++;

    size_t cap = strlen(pos) + 1;
    char *result = malloc(cap);
    if (!result) return NULL;
    size_t len = 0;
    while (*pos && *pos != '"') {
        if (*pos == '\\' && pos[1]) {
            pos++;
            if (*pos == 'n') result[len++] = '\n';
            else if (*pos == 'r') result[len++] = '\r';
            else if (*pos == 't') result[len++] = '\t';
            else result[len++] = *pos;
            pos++;
        } else {
            result[len++] = *pos++;
        }
    }
    result[len] = '\0';
    return result;
}

/* Helper: Extract JSON number value */
static int json_extract_number(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) return -1;
    
    pos += strlen(search);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')) pos++;
    if (*pos != ':') return -1;
    pos++;
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')) pos++;
    return (int)strtol(pos, NULL, 10);
}

static const char *json_skip_ws_range(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    return p;
}

static const char *json_find_range(const char *start, const char *end, const char *needle) {
    if (!start || !end || end < start || !needle) return NULL;
    return find_bytes(start, (size_t)(end - start), needle, strlen(needle));
}

static const char *json_matching_delim(const char *open, const char *end,
                                       char open_ch, char close_ch) {
    if (!open || open >= end || *open != open_ch) return NULL;
    int depth = 0;
    bool in_string = false;
    bool escape = false;

    for (const char *p = open; p < end && *p; p++) {
        char c = *p;
        if (in_string) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }

        if (c == '"') {
            in_string = true;
        } else if (c == open_ch) {
            depth++;
        } else if (c == close_ch) {
            depth--;
            if (depth == 0)
                return p;
        }
    }
    return NULL;
}

static const char *json_value_after_key_range(const char *start, const char *end,
                                              const char *key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = json_find_range(start, end, search);
    if (!p) return NULL;

    p += strlen(search);
    p = json_skip_ws_range(p, end);
    if (p >= end || *p != ':') return NULL;
    p++;
    return json_skip_ws_range(p, end);
}

static bool json_object_after_key_range(const char *start, const char *end,
                                        const char *key,
                                        const char **obj_start,
                                        const char **obj_end) {
    const char *value = json_value_after_key_range(start, end, key);
    if (!value || value >= end || *value != '{') return false;

    const char *close = json_matching_delim(value, end, '{', '}');
    if (!close) return false;

    if (obj_start) *obj_start = value;
    if (obj_end) *obj_end = close + 1;
    return true;
}

static int json_number_in_range(const char *start, const char *end,
                                const char *key, int default_value) {
    const char *value = json_value_after_key_range(start, end, key);
    if (!value || value >= end) return default_value;
    char *num_end = NULL;
    long parsed = strtol(value, &num_end, 10);
    if (num_end == value || num_end > end) return default_value;
    return (int)parsed;
}

static char *json_string_in_range(const char *start, const char *end, const char *key) {
    const char *value = json_value_after_key_range(start, end, key);
    if (!value || value >= end || *value != '"') return strdup("");
    value++;

    size_t cap = (size_t)(end - value) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;

    size_t len = 0;
    bool escape = false;
    for (const char *p = value; p < end && *p; p++) {
        char c = *p;
        if (escape) {
            if (c == 'n') out[len++] = '\n';
            else if (c == 'r') out[len++] = '\r';
            else if (c == 't') out[len++] = '\t';
            else out[len++] = c;
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            continue;
        }
        if (c == '"')
            break;
        out[len++] = c;
    }
    out[len] = '\0';
    return out;
}

static void lsp_diagnostics_append(LSPDiagnostics *diag, LSPDiagnostic item) {
    if (!diag) return;
    LSPDiagnostic *items = realloc(diag->items, sizeof(LSPDiagnostic) * (size_t)(diag->count + 1));
    if (!items) {
        free(item.message);
        free(item.code);
        return;
    }
    diag->items = items;
    diag->items[diag->count++] = item;
}

static void lsp_parse_diagnostic_object(const char *obj_start, const char *obj_end,
                                        LSPDiagnostic *out) {
    memset(out, 0, sizeof(*out));
    out->severity = LSP_DIAG_ERROR;

    const char *range_start = NULL;
    const char *range_end = NULL;
    if (json_object_after_key_range(obj_start, obj_end, "range", &range_start, &range_end)) {
        const char *start_start = NULL;
        const char *start_end = NULL;
        if (json_object_after_key_range(range_start, range_end, "start", &start_start, &start_end)) {
            out->start_line = json_number_in_range(start_start, start_end, "line", 0);
            out->start_col = json_number_in_range(start_start, start_end, "character", 0);
        }

        const char *end_start = NULL;
        const char *end_end = NULL;
        if (json_object_after_key_range(range_start, range_end, "end", &end_start, &end_end)) {
            out->end_line = json_number_in_range(end_start, end_end, "line", out->start_line);
            out->end_col = json_number_in_range(end_start, end_end, "character", out->start_col + 1);
        }
    }

    if (out->end_line < out->start_line ||
        (out->end_line == out->start_line && out->end_col <= out->start_col)) {
        out->end_line = out->start_line;
        out->end_col = out->start_col + 1;
    }

    int severity = json_number_in_range(obj_start, obj_end, "severity", LSP_DIAG_ERROR);
    if (severity < LSP_DIAG_ERROR || severity > LSP_DIAG_HINT)
        severity = LSP_DIAG_ERROR;
    out->severity = (LSPDiagnosticSeverity)severity;

    out->message = json_string_in_range(obj_start, obj_end, "message");
    out->code = json_string_in_range(obj_start, obj_end, "code");
}

static LSPDiagnostics *lsp_parse_diagnostics_array(const char *array_start) {
    LSPDiagnostics *diag = malloc(sizeof(LSPDiagnostics));
    if (!diag) return NULL;
    memset(diag, 0, sizeof(*diag));
    if (!array_start || *array_start != '[') return diag;

    const char *array_end = json_matching_delim(array_start, array_start + strlen(array_start), '[', ']');
    if (!array_end) return diag;

    int depth = 0;
    bool in_string = false;
    bool escape = false;
    const char *obj_start = NULL;

    for (const char *p = array_start + 1; p < array_end; p++) {
        char c = *p;
        if (in_string) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }

        if (c == '"') {
            in_string = true;
        } else if (c == '{') {
            if (depth == 0)
                obj_start = p;
            depth++;
        } else if (c == '}') {
            if (depth > 0)
                depth--;
            if (depth == 0 && obj_start) {
                LSPDiagnostic item;
                lsp_parse_diagnostic_object(obj_start, p + 1, &item);
                lsp_diagnostics_append(diag, item);
                obj_start = NULL;
            }
        }
    }

    return diag;
}

void lsp_manager_init(LSPManager *manager) {
    signal(SIGPIPE, SIG_IGN);
    manager->clients = NULL;
    manager->client_count = 0;
    manager->client_capacity = 0;
    manager->workspace_root = NULL;
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
        free(manager->clients[i].read_buffer);
    }
    free(manager->clients);
    free(manager->workspace_root);
    manager->clients = NULL;
    manager->client_count = 0;
    manager->client_capacity = 0;
    manager->workspace_root = NULL;
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
    client->read_buffer = NULL;
    client->read_len = 0;
    client->read_capacity = 0;
    
    manager->client_count++;
}

void lsp_manager_stop_all(LSPManager *manager) {
    if (!manager) return;
    for (int i = 0; i < manager->client_count; i++)
        lsp_client_stop(&manager->clients[i]);
}

void lsp_manager_restart_all(LSPManager *manager) {
    if (!manager) return;
    for (int i = 0; i < manager->client_count; i++) {
        lsp_client_stop(&manager->clients[i]);
        lsp_client_initialize(&manager->clients[i], manager->workspace_root);
    }
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
        
        execvp(client->config.path, argv);
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
    client->read_len = 0;
}

bool lsp_client_initialize(LSPClient *client, const char *workspace_root) {
    if (client->initialized) {
        return true;
    }
    
    if (client->status == LSP_STATUS_DISCONNECTED) {
        if (!lsp_client_start(client)) {
            return false;
        }
    }
    
    /* Send initialize request */
    char init_params[1024];
    snprintf(init_params, sizeof(init_params), "{"
        "\"processId\":null,"
        "\"rootPath\":\"%s\","
        "\"rootUri\":\"file://%s\","
        "\"capabilities\":{"
            "\"textDocument\":{"
                "\"synchronization\":{\"didOpen\":true,\"didChange\":true,\"didSave\":true},"
                "\"semanticTokens\":{\"requests\":{\"full\":true}}"
            "}"
        "}"
    "}", workspace_root ? workspace_root : ".", workspace_root ? workspace_root : ".");
    
    char buffer[4096];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"initialize\",\"params\":%s}",
             client->id++, init_params);
    
    int content_len = strlen(buffer);
    if (!lsp_client_write_message(client, buffer, content_len))
        return false;
    
    /* Send initialized notification */
    const char *initialized_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}";
    content_len = strlen(initialized_msg);
    if (!lsp_client_write_message(client, initialized_msg, content_len))
        return false;
    
    client->status = LSP_STATUS_INITIALIZED;
    client->initialized = true;
    
    return true;
}

LSPStatus lsp_client_get_status(LSPClient *client) {
    return client->status;
}

void lsp_manager_status_counts(LSPManager *manager, int *initialized, int *connecting, int *errors) {
    if (initialized) *initialized = 0;
    if (connecting) *connecting = 0;
    if (errors) *errors = 0;
    if (!manager) return;
    for (int i = 0; i < manager->client_count; i++) {
        switch (manager->clients[i].status) {
        case LSP_STATUS_INITIALIZED:
            if (initialized) (*initialized)++;
            break;
        case LSP_STATUS_CONNECTING:
            if (connecting) (*connecting)++;
            break;
        case LSP_STATUS_ERROR:
            if (errors) (*errors)++;
            break;
        default:
            break;
        }
    }
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
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
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
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
}

void lsp_client_send_completion_request(LSPClient *client, const char *file_uri, int line, int character) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }

    char params[640];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\"},\"position\":{\"line\":%d,\"character\":%d},\"context\":{\"triggerKind\":1}}",
             file_uri, line, character);

    char buffer[1200];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/completion\",\"params\":%s}",
             client->id++, params);

    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
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
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
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
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
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
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
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
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
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
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
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
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
}

void lsp_client_send_didOpen(LSPClient *client, const char *file_uri, const char *language_id, const char *text) {
    if (client->status != LSP_STATUS_INITIALIZED) return;
    
    /* Escape text for JSON */
    size_t text_len = strlen(text);
    size_t escaped_len = text_len * 2 + 1;
    char *escaped = malloc(escaped_len);
    size_t j = 0;
    for (size_t i = 0; i < text_len; i++) {
        char c = text[i];
        if (c == '"') { escaped[j++] = '\\'; escaped[j++] = '"'; }
        else if (c == '\\') { escaped[j++] = '\\'; escaped[j++] = '\\'; }
        else if (c == '\n') { escaped[j++] = '\\'; escaped[j++] = 'n'; }
        else if (c == '\r') { escaped[j++] = '\\'; escaped[j++] = 'r'; }
        else if (c == '\t') { escaped[j++] = '\\'; escaped[j++] = 't'; }
        else { escaped[j++] = c; }
    }
    escaped[j] = '\0';
    
    char params[4096];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\",\"languageId\":\"%s\",\"version\":1,\"text\":\"%s\"}}",
             file_uri, language_id, escaped);
    free(escaped);
    
    char buffer[8192];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":%s}",
             params);
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
}

void lsp_client_send_didChange(LSPClient *client, const char *file_uri, const char *text) {
    if (client->status != LSP_STATUS_INITIALIZED) return;
    
    /* Escape text for JSON */
    size_t text_len = strlen(text);
    size_t escaped_len = text_len * 2 + 1;
    char *escaped = malloc(escaped_len);
    size_t j = 0;
    for (size_t i = 0; i < text_len; i++) {
        char c = text[i];
        if (c == '"') { escaped[j++] = '\\'; escaped[j++] = '"'; }
        else if (c == '\\') { escaped[j++] = '\\'; escaped[j++] = '\\'; }
        else if (c == '\n') { escaped[j++] = '\\'; escaped[j++] = 'n'; }
        else if (c == '\r') { escaped[j++] = '\\'; escaped[j++] = 'r'; }
        else if (c == '\t') { escaped[j++] = '\\'; escaped[j++] = 't'; }
        else { escaped[j++] = c; }
    }
    escaped[j] = '\0';
    
    char params[4096];
    snprintf(params, sizeof(params),
             "{\"textDocument\":{\"uri\":\"%s\",\"version\":2},\"contentChanges\":[{\"text\":\"%s\"}]}",
             file_uri, escaped);
    free(escaped);
    
    char buffer[8192];
    snprintf(buffer, sizeof(buffer),
             "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":%s}",
             params);
    
    int content_len = strlen(buffer);
    lsp_client_write_message(client, buffer, content_len);
}

char *lsp_client_read_response(LSPClient *client) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return NULL;
    }

    char chunk[8192];
    for (;;) {
        ssize_t bytes_read = read(client->stdout_fd, chunk, sizeof(chunk));
        if (bytes_read > 0) {
            if (!lsp_client_buffer_append(client, chunk, (size_t)bytes_read))
                return NULL;
            continue;
        }
        if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        if (bytes_read <= 0)
            break;
    }

    const char *header_start = find_bytes(client->read_buffer, client->read_len,
                                          "Content-Length:", strlen("Content-Length:"));
    if (!header_start) {
        if (client->read_len > 4096)
            client->read_len = 0;
        return NULL;
    }

    size_t prefix_len = (size_t)(header_start - client->read_buffer);
    if (prefix_len > 0) {
        memmove(client->read_buffer, header_start, client->read_len - prefix_len);
        client->read_len -= prefix_len;
        client->read_buffer[client->read_len] = '\0';
    }

    const char *body_start = find_bytes(client->read_buffer, client->read_len, "\r\n\r\n", 4);
    if (!body_start)
        return NULL;

    size_t header_len = (size_t)(body_start - client->read_buffer);
    char header[512];
    size_t header_copy = header_len < sizeof(header) - 1 ? header_len : sizeof(header) - 1;
    memcpy(header, client->read_buffer, header_copy);
    header[header_copy] = '\0';

    int content_len = 0;
    if (sscanf(header, "Content-Length: %d", &content_len) != 1) {
        return NULL;
    }

    if (content_len <= 0 || content_len > 1024 * 1024) {
        client->read_len = 0;
        if (client->read_buffer)
            client->read_buffer[0] = '\0';
        return NULL;
    }

    size_t frame_len = header_len + 4 + (size_t)content_len;
    if (client->read_len < frame_len)
        return NULL;

    char *response = malloc((size_t)content_len + 1);
    if (!response) return NULL;
    memcpy(response, client->read_buffer + header_len + 4, (size_t)content_len);
    response[content_len] = '\0';

    size_t remaining = client->read_len - frame_len;
    if (remaining > 0)
        memmove(client->read_buffer, client->read_buffer + frame_len, remaining);
    client->read_len = remaining;
    if (client->read_buffer)
        client->read_buffer[client->read_len] = '\0';
    return response;
}

bool lsp_client_unread_response(LSPClient *client, const char *response) {
    if (!client || !response) return false;

    size_t body_len = strlen(response);
    char header[256];
    int header_len = snprintf(header, sizeof(header), "Content-Length: %zu\r\n\r\n", body_len);
    if (header_len <= 0 || header_len >= (int)sizeof(header))
        return false;

    size_t frame_len = (size_t)header_len + body_len;
    if (client->read_len + frame_len + 1 > client->read_capacity) {
        size_t next = client->read_capacity ? client->read_capacity : 16384;
        while (next < client->read_len + frame_len + 1)
            next *= 2;
        char *buf = realloc(client->read_buffer, next);
        if (!buf) return false;
        client->read_buffer = buf;
        client->read_capacity = next;
    }

    memmove(client->read_buffer + frame_len, client->read_buffer, client->read_len);
    memcpy(client->read_buffer, header, (size_t)header_len);
    memcpy(client->read_buffer + header_len, response, body_len);
    client->read_len += frame_len;
    client->read_buffer[client->read_len] = '\0';
    return true;
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

LSPCompletionItems *lsp_parse_completion_response(const char *response) {
    if (!response) return NULL;
    const char *result = strstr(response, "\"result\"");
    if (!result || strstr(result, "\"result\":null") || strstr(result, "\"result\": null")) return NULL;

    LSPCompletionItems *items = calloc(1, sizeof(LSPCompletionItems));
    items->items = calloc(128, sizeof(LSPCompletionItem));

    const char *p = result;
    while ((p = strstr(p, "\"label\"")) && items->count < 128) {
        char *label = json_extract_string(p, "label");
        if (label && label[0]) {
            LSPCompletionItem *it = &items->items[items->count++];
            it->label = label;
            it->detail = json_extract_string(p, "detail");
            it->documentation = json_extract_string(p, "documentation");
        } else {
            free(label);
        }
        p += 7;
    }

    if (items->count == 0) {
        free(items->items);
        free(items);
        return NULL;
    }
    return items;
}

void lsp_free_completion_items(LSPCompletionItems *items) {
    if (!items) return;
    for (int i = 0; i < items->count; i++) {
        free(items->items[i].label);
        free(items->items[i].detail);
        free(items->items[i].documentation);
    }
    free(items->items);
    free(items);
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
            /* Don't retry failed servers too frequently */
            if (manager->clients[i].status == LSP_STATUS_ERROR) {
                return NULL;
            }
            if (manager->clients[i].status != LSP_STATUS_INITIALIZED) {
                if (!lsp_client_initialize(&manager->clients[i], manager->workspace_root)) {
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

    /* Parse: {"result": [{...}, {...}]} */
    const char *result = strstr(response, "\"result\"");
    if (!result) {
        LSPDiagnostics *diag = malloc(sizeof(LSPDiagnostics));
        if (diag) memset(diag, 0, sizeof(*diag));
        return diag;
    }

    const char *array = strchr(result, '[');
    return lsp_parse_diagnostics_array(array);
}

void lsp_free_diagnostics(LSPDiagnostics *diag) {
    if (!diag) return;
    free(diag->uri);
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
    
    /* Parse: {"method":"textDocument/publishDiagnostics","params":{"uri":"...","diagnostics":[...]}} */
    const char *diagnostics_start = strstr(response, "\"diagnostics\"");
    if (!diagnostics_start) {
        LSPDiagnostics *diag = malloc(sizeof(LSPDiagnostics));
        if (diag) {
            memset(diag, 0, sizeof(*diag));
            const char *params_start = NULL;
            const char *params_end = NULL;
            if (json_object_after_key_range(response, response + strlen(response),
                                            "params", &params_start, &params_end)) {
                diag->uri = json_string_in_range(params_start, params_end, "uri");
            }
        }
        return diag;
    }

    const char *array = strchr(diagnostics_start, '[');
    LSPDiagnostics *diag = lsp_parse_diagnostics_array(array);
    if (diag) {
        const char *params_start = NULL;
        const char *params_end = NULL;
        if (json_object_after_key_range(response, response + strlen(response),
                                        "params", &params_start, &params_end)) {
            diag->uri = json_string_in_range(params_start, params_end, "uri");
        }
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
    
    /* Parser: extract range and newText pairs */
    const char *p = changes_start;
    while (edit->count < 100) {
        /* Find next range object */
        const char *range_start = strstr(p, "\"range\"");
        if (!range_start) break;
        
        const char *range_brace = strchr(range_start, '{');
        if (!range_brace) break;
        
        /* Parse start position: "start":{"line":X,"character":Y} */
        const char *start_pos = strstr(range_brace, "\"start\"");
        if (!start_pos) break;
        
        int start_line = 0, start_char = 0;
        sscanf(start_pos, "\"start\":{\"line\":%d,\"character\":%d", &start_line, &start_char);
        
        /* Parse end position: "end":{"line":X,"character":Y} */
        const char *end_pos = strstr(range_brace, "\"end\"");
        if (!end_pos) break;
        
        int end_line = 0, end_char = 0;
        sscanf(end_pos, "\"end\":{\"line\":%d,\"character\":%d", &end_line, &end_char);
        
        /* Find newText after this range */
        const char *newtext_ptr = strstr(range_brace, "\"newText\"");
        if (!newtext_ptr) break;
        
        const char *colon = strchr(newtext_ptr, ':');
        if (!colon) break;
        
        const char *quote_start = strchr(colon, '"');
        if (!quote_start) break;
        quote_start++;
        
        /* Find closing quote (handle escapes) */
        const char *quote_end = quote_start;
        while (*quote_end && *quote_end != '"') {
            if (*quote_end == '\\') quote_end += 2;
            else quote_end++;
        }
        
        if (*quote_end != '"') break;
        
        size_t text_len = quote_end - quote_start;
        if (text_len > 0 && text_len < 2048) {
            edit->changes[edit->count].new_text = malloc(text_len + 1);
            memcpy(edit->changes[edit->count].new_text, quote_start, text_len);
            edit->changes[edit->count].new_text[text_len] = '\0';
            
            /* Store range */
            edit->changes[edit->count].range.start.line = start_line;
            edit->changes[edit->count].range.start.character = start_char;
            edit->changes[edit->count].range.end.line = end_line;
            edit->changes[edit->count].range.end.character = end_char;
            
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
