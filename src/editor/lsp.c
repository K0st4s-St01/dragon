#include "dragon_editor/lsp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
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

static bool lsp_client_write_jsonf(LSPClient *client, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (len < 0) return false;

    char *body = malloc((size_t)len + 1);
    if (!body) return false;

    va_start(args, fmt);
    vsnprintf(body, (size_t)len + 1, fmt, args);
    va_end(args);

    bool ok = lsp_client_write_message(client, body, len);
    free(body);
    return ok;
}

static bool json_escape_append(char **out, size_t *len, size_t *cap, const char *s, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t next = *cap ? *cap : 256;
        while (next < *len + n + 1)
            next *= 2;
        char *buf = realloc(*out, next);
        if (!buf) return false;
        *out = buf;
        *cap = next;
    }
    memcpy(*out + *len, s, n);
    *len += n;
    (*out)[*len] = '\0';
    return true;
}

static char *json_escape_string(const char *text) {
    if (!text) text = "";
    char *out = NULL;
    size_t len = 0, cap = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        char buf[8];
        switch (*p) {
        case '"':
            if (!json_escape_append(&out, &len, &cap, "\\\"", 2)) goto fail;
            break;
        case '\\':
            if (!json_escape_append(&out, &len, &cap, "\\\\", 2)) goto fail;
            break;
        case '\b':
            if (!json_escape_append(&out, &len, &cap, "\\b", 2)) goto fail;
            break;
        case '\f':
            if (!json_escape_append(&out, &len, &cap, "\\f", 2)) goto fail;
            break;
        case '\n':
            if (!json_escape_append(&out, &len, &cap, "\\n", 2)) goto fail;
            break;
        case '\r':
            if (!json_escape_append(&out, &len, &cap, "\\r", 2)) goto fail;
            break;
        case '\t':
            if (!json_escape_append(&out, &len, &cap, "\\t", 2)) goto fail;
            break;
        default:
            if (*p < 0x20) {
                snprintf(buf, sizeof(buf), "\\u%04x", *p);
                if (!json_escape_append(&out, &len, &cap, buf, 6)) goto fail;
            } else if (!json_escape_append(&out, &len, &cap, (const char *)p, 1)) {
                goto fail;
            }
            break;
        }
    }
    if (!out && !json_escape_append(&out, &len, &cap, "", 0)) goto fail;
    return out;

fail:
    free(out);
    return NULL;
}

static char *lsp_absolute_path(const char *path) {
    if (!path) return NULL;

    char *resolved = realpath(path, NULL);
    if (resolved)
        return resolved;

    if (path[0] == '/')
        return strdup(path);

    char *cwd = getcwd(NULL, 0);
    if (!cwd) return strdup(path);

    size_t len = strlen(cwd) + 1 + strlen(path) + 1;
    char *absolute = malloc(len);
    if (absolute)
        snprintf(absolute, len, "%s/%s", cwd, path);
    free(cwd);
    return absolute ? absolute : strdup(path);
}

static char *lsp_file_uri_from_path(const char *path) {
    char *absolute = lsp_absolute_path(path);
    if (!absolute) return NULL;

    size_t cap = strlen(absolute) * 3 + strlen("file://") + 1;
    char *uri = malloc(cap);
    if (!uri) {
        free(absolute);
        return NULL;
    }

    strcpy(uri, "file://");
    size_t out = strlen(uri);
    for (const unsigned char *p = (const unsigned char *)absolute; *p && out + 4 < cap; p++) {
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') || *p == '/' || *p == '-' ||
            *p == '_' || *p == '.' || *p == '~') {
            uri[out++] = (char)*p;
        } else {
            snprintf(uri + out, cap - out, "%%%02X", *p);
            out += 3;
        }
    }
    uri[out] = '\0';
    free(absolute);
    return uri;
}

static void lsp_client_send_position_request(LSPClient *client, const char *method,
                                             const char *file_uri, int line,
                                             int character, const char *extra_params) {
    if (client->status != LSP_STATUS_INITIALIZED) return;
    char *uri = json_escape_string(file_uri);
    if (!uri) return;
    lsp_client_write_jsonf(client,
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"%s\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\"},"
        "\"position\":{\"line\":%d,\"character\":%d}%s}}",
        client->id++, method, uri, line, character,
        extra_params ? extra_params : "");
    free(uri);
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

static const char *json_key_in_range(const char *start, const char *end, const char *key) {
    if (!start || !end || end < start || !key) return NULL;

    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    size_t search_len = strlen(search);
    bool in_string = false;
    bool escape = false;

    for (const char *p = start; p < end && *p; p++) {
        char c = *p;
        if (in_string) {
            if (escape)
                escape = false;
            else if (c == '\\')
                escape = true;
            else if (c == '"')
                in_string = false;
            continue;
        }

        if (c == '"') {
            if ((size_t)(end - p) >= search_len && memcmp(p, search, search_len) == 0) {
                const char *after = json_skip_ws_range(p + search_len, end);
                if (after < end && *after == ':')
                    return p;
            }
            in_string = true;
        }
    }

    return NULL;
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
    const char *p = json_key_in_range(start, end, key);
    if (!p) return NULL;

    p++;
    while (p < end && *p && *p != '"') p++;
    if (p >= end || *p != '"') return NULL;
    p++;
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

static char *json_scalar_to_string(const char *value, const char *end) {
    if (!value || value >= end) return strdup("");

    const char *p = value;
    while (p < end && *p &&
           *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        p++;
    }

    size_t len = (size_t)(p - value);
    if (len == 0 || len > 256) return strdup("");
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, value, len);
    out[len] = '\0';
    return out;
}

static char *json_code_in_range(const char *start, const char *end) {
    const char *value = json_value_after_key_range(start, end, "code");
    if (!value || value >= end) return strdup("");

    if (*value == '"')
        return json_string_in_range(start, end, "code");

    if (*value == '{') {
        const char *obj_end = json_matching_delim(value, end, '{', '}');
        if (!obj_end) return strdup("");
        const char *inner = json_value_after_key_range(value, obj_end + 1, "value");
        if (!inner || inner > obj_end) return strdup("");
        if (*inner == '"')
            return json_string_in_range(value, obj_end + 1, "value");
        return json_scalar_to_string(inner, obj_end + 1);
    }

    return json_scalar_to_string(value, end);
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
    out->code = json_code_in_range(obj_start, obj_end);
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
    
    char *root_absolute = lsp_absolute_path(workspace_root ? workspace_root : ".");
    char *root_uri = lsp_file_uri_from_path(root_absolute ? root_absolute : ".");
    char *root_path = json_escape_string(root_absolute ? root_absolute : ".");
    char *root_uri_json = json_escape_string(root_uri ? root_uri : "file:///");
    if (!root_path || !root_uri_json) {
        free(root_absolute);
        free(root_uri);
        free(root_path);
        free(root_uri_json);
        return false;
    }

    int init_id = client->id++;
    if (!lsp_client_write_jsonf(client,
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"initialize\","
        "\"params\":{\"processId\":null,\"rootPath\":\"%s\",\"rootUri\":\"%s\","
        "\"capabilities\":{\"textDocument\":{\"synchronization\":{\"didOpen\":true,"
        "\"didChange\":true,\"didSave\":true},\"semanticTokens\":{\"requests\":{\"full\":true}},"
        "\"publishDiagnostics\":{\"relatedInformation\":true}}}}}",
        init_id, root_path, root_uri_json)) {
        free(root_absolute);
        free(root_uri);
        free(root_path);
        free(root_uri_json);
        return false;
    }
    free(root_absolute);
    free(root_uri);
    free(root_path);
    free(root_uri_json);

    char id_pattern[64];
    snprintf(id_pattern, sizeof(id_pattern), "\"id\":%d", init_id);
    char *deferred[32] = {0};
    int deferred_count = 0;
    bool initialized = false;
    time_t start = time(NULL);
    while (difftime(time(NULL), start) < 2.0) {
        char *response = lsp_client_read_response(client);
        if (!response) {
            usleep(10000);
            continue;
        }
        if (strstr(response, id_pattern) && strstr(response, "\"result\"")) {
            initialized = true;
            free(response);
            break;
        }
        if (deferred_count < (int)(sizeof(deferred) / sizeof(deferred[0]))) {
            deferred[deferred_count++] = response;
        } else {
            free(response);
        }
    }

    for (int i = deferred_count - 1; i >= 0; i--) {
        lsp_client_unread_response(client, deferred[i]);
        free(deferred[i]);
    }

    if (!initialized) {
        client->status = LSP_STATUS_ERROR;
        return false;
    }
    
    /* Send initialized notification */
    const char *initialized_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}";
    int content_len = strlen(initialized_msg);
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
    lsp_client_send_position_request(client, "textDocument/definition", file_uri, line, character, NULL);
}

void lsp_client_send_hover_request(LSPClient *client, const char *file_uri, int line, int character) {
    lsp_client_send_position_request(client, "textDocument/hover", file_uri, line, character, NULL);
}

void lsp_client_send_completion_request(LSPClient *client, const char *file_uri, int line, int character) {
    lsp_client_send_position_request(client, "textDocument/completion", file_uri, line, character,
                                     ",\"context\":{\"triggerKind\":1}");
}

void lsp_client_send_semantic_tokens_request(LSPClient *client, const char *file_uri) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }
    char *uri = json_escape_string(file_uri);
    if (!uri) return;
    lsp_client_write_jsonf(client,
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/semanticTokens/full\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\"}}}",
        client->id++, uri);
    free(uri);
}

void lsp_client_send_rename_request(LSPClient *client, const char *file_uri, int line, int character, const char *new_name) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }

    char *uri = json_escape_string(file_uri);
    char *name = json_escape_string(new_name);
    if (!uri || !name) {
        free(uri);
        free(name);
        return;
    }
    lsp_client_write_jsonf(client,
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/rename\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\"},"
        "\"position\":{\"line\":%d,\"character\":%d},\"newName\":\"%s\"}}",
        client->id++, uri, line, character, name);
    free(uri);
    free(name);
}

void lsp_client_send_code_action_request(LSPClient *client, const char *file_uri, int start_line, int start_character, int end_line, int end_character) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }

    char *uri = json_escape_string(file_uri);
    if (!uri) return;
    lsp_client_write_jsonf(client,
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/codeAction\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\"},"
        "\"range\":{\"start\":{\"line\":%d,\"character\":%d},"
        "\"end\":{\"line\":%d,\"character\":%d}},\"context\":{\"diagnostics\":[]}}}",
        client->id++, uri, start_line, start_character, end_line, end_character);
    free(uri);
}

void lsp_client_send_formatting_request(LSPClient *client, const char *file_uri, int tab_size, bool insert_spaces) {
    if (client->status != LSP_STATUS_INITIALIZED) {
        return;
    }

    char *uri = json_escape_string(file_uri);
    if (!uri) return;
    if (tab_size <= 0) tab_size = 4;
    lsp_client_write_jsonf(client,
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/formatting\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\"},"
        "\"options\":{\"tabSize\":%d,\"insertSpaces\":%s}}}",
        client->id++, uri, tab_size, insert_spaces ? "true" : "false");
    free(uri);
}

void lsp_client_send_type_definition_request(LSPClient *client, const char *file_uri, int line, int character) {
    lsp_client_send_position_request(client, "textDocument/typeDefinition", file_uri, line, character, NULL);
}

void lsp_client_send_references_request(LSPClient *client, const char *file_uri, int line, int character) {
    lsp_client_send_position_request(client, "textDocument/references", file_uri, line, character,
                                     ",\"context\":{\"includeDeclaration\":true}");
}

void lsp_client_send_implementation_request(LSPClient *client, const char *file_uri, int line, int character) {
    lsp_client_send_position_request(client, "textDocument/implementation", file_uri, line, character, NULL);
}

void lsp_client_send_didOpen(LSPClient *client, const char *file_uri, const char *language_id, const char *text) {
    if (client->status != LSP_STATUS_INITIALIZED) return;

    char *uri = json_escape_string(file_uri);
    char *lang = json_escape_string(language_id);
    char *escaped_text = json_escape_string(text);
    if (!uri || !lang || !escaped_text) {
        free(uri);
        free(lang);
        free(escaped_text);
        return;
    }

    lsp_client_write_jsonf(client,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\",\"languageId\":\"%s\","
        "\"version\":1,\"text\":\"%s\"}}}",
        uri, lang, escaped_text);

    free(uri);
    free(lang);
    free(escaped_text);
}

void lsp_client_send_didChange(LSPClient *client, const char *file_uri, int version, const char *text) {
    if (client->status != LSP_STATUS_INITIALIZED) return;

    char *uri = json_escape_string(file_uri);
    char *escaped_text = json_escape_string(text);
    if (!uri || !escaped_text) {
        free(uri);
        free(escaped_text);
        return;
    }

    lsp_client_write_jsonf(client,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\",\"version\":%d},"
        "\"contentChanges\":[{\"text\":\"%s\"}]}}",
        uri, version, escaped_text);

    free(uri);
    free(escaped_text);
}

char *lsp_client_read_response(LSPClient *client) {
    if (!client || client->status == LSP_STATUS_DISCONNECTED || client->status == LSP_STATUS_ERROR) {
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
            it->insert_text = json_extract_string(p, "insertText");
            if (!it->insert_text || !it->insert_text[0]) {
                free(it->insert_text);
                it->insert_text = json_extract_string(p, "newText");
            }
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
        free(items->items[i].insert_text);
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
        /* No diagnostics array found - not a valid diagnostics notification */
        return NULL;
    }

    const char *array = strchr(diagnostics_start, '[');
    if (!array) return NULL;
    
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

static void workspace_edit_append(LSPWorkspaceEdit *edit, LSPTextEdit item) {
    if (!edit) return;
    LSPTextEdit *changes = realloc(edit->changes, sizeof(LSPTextEdit) * (size_t)(edit->count + 1));
    if (!changes) {
        free(item.new_text);
        return;
    }
    edit->changes = changes;
    edit->changes[edit->count++] = item;
}

static bool parse_text_edit_at_range(const char *range_key, const char *limit, LSPTextEdit *out) {
    memset(out, 0, sizeof(*out));
    const char *range_start = strchr(range_key, '{');
    if (!range_start || range_start >= limit) return false;
    const char *range_end = json_matching_delim(range_start, limit, '{', '}');
    if (!range_end) return false;
    range_end++;

    const char *start_start = NULL, *start_end = NULL;
    const char *end_start = NULL, *end_end = NULL;
    if (json_object_after_key_range(range_start, range_end, "start", &start_start, &start_end)) {
        out->range.start.line = json_number_in_range(start_start, start_end, "line", 0);
        out->range.start.character = json_number_in_range(start_start, start_end, "character", 0);
    }
    if (json_object_after_key_range(range_start, range_end, "end", &end_start, &end_end)) {
        out->range.end.line = json_number_in_range(end_start, end_end, "line", out->range.start.line);
        out->range.end.character = json_number_in_range(end_start, end_end, "character", out->range.start.character);
    }

    const char *next_range = json_find_range(range_end, limit, "\"range\"");
    const char *text_limit = next_range ? next_range : limit;
    if (!json_value_after_key_range(range_end, text_limit, "newText"))
        return false;
    out->new_text = json_string_in_range(range_end, text_limit, "newText");
    if (!out->new_text) out->new_text = strdup("");
    return true;
}

static LSPWorkspaceEdit *parse_workspace_edits_in_range(const char *start, const char *end) {
    LSPWorkspaceEdit *edit = calloc(1, sizeof(LSPWorkspaceEdit));
    if (!edit || !start || !end || start >= end) return edit;

    const char *p = start;
    while ((p = json_find_range(p, end, "\"range\"")) && edit->count < 200) {
        LSPTextEdit item;
        if (parse_text_edit_at_range(p, end, &item))
            workspace_edit_append(edit, item);
        p += 7;
    }
    return edit;
}

static LSPWorkspaceEdit *parse_result_workspace_edit(const char *response) {
    LSPWorkspaceEdit *edit = calloc(1, sizeof(LSPWorkspaceEdit));
    if (!response) return edit;
    const char *result = strstr(response, "\"result\"");
    if (!result || strstr(result, "\"result\":null") || strstr(result, "\"result\": null"))
        return edit;

    const char *value = json_value_after_key_range(result, response + strlen(response), "result");
    if (!value) return edit;

    const char *end = response + strlen(response);
    if (*value == '[') {
        const char *array_end = json_matching_delim(value, end, '[', ']');
        if (array_end) {
            lsp_free_workspace_edit(edit);
            return parse_workspace_edits_in_range(value, array_end + 1);
        }
    } else if (*value == '{') {
        const char *obj_end = json_matching_delim(value, end, '{', '}');
        if (obj_end) {
            lsp_free_workspace_edit(edit);
            return parse_workspace_edits_in_range(value, obj_end + 1);
        }
    }
    return edit;
}

LSPWorkspaceEdit *lsp_parse_rename_response(const char *response) {
    return parse_result_workspace_edit(response);
}

LSPWorkspaceEdit *lsp_parse_formatting_response(const char *response) {
    return parse_result_workspace_edit(response);
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
    
    const char *p = array_start;
    const char *array_end = json_matching_delim(array_start, response + strlen(response), '[', ']');
    if (!array_end) array_end = response + strlen(response);
    while ((p = json_find_range(p, array_end, "{")) && actions->count < 50) {
        const char *obj_end = json_matching_delim(p, array_end, '{', '}');
        if (!obj_end) break;
        obj_end++;
        char *title = json_string_in_range(p, obj_end, "title");
        if (title && title[0]) {
            LSPCodeAction *action = &actions->actions[actions->count++];
            action->title = title;
            action->kind = json_string_in_range(p, obj_end, "kind");
            const char *edit_start = NULL, *edit_end = NULL;
            if (json_object_after_key_range(p, obj_end, "edit", &edit_start, &edit_end))
                action->edit = parse_workspace_edits_in_range(edit_start, edit_end);
        }
        else {
            free(title);
        }
        p = obj_end;
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
