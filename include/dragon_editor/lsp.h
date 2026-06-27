#ifndef DE_LSP_H
#define DE_LSP_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LSP_STATUS_DISCONNECTED,
    LSP_STATUS_CONNECTING,
    LSP_STATUS_INITIALIZED,
    LSP_STATUS_ERROR,
} LSPStatus;

typedef struct {
    int line;
    int character;
} LSPPosition;

typedef struct {
    LSPPosition start;
    LSPPosition end;
} LSPRange;

typedef struct {
    char *uri;
    LSPRange range;
} LSPLocation;

typedef struct {
    char *new_text;
    LSPRange range;
} LSPTextEdit;

typedef struct {
    LSPTextEdit *changes;
    int count;
} LSPWorkspaceEdit;

typedef struct {
    char *title;
    char *kind;
    LSPWorkspaceEdit *edit;
} LSPCodeAction;

typedef struct {
    LSPCodeAction *actions;
    int count;
} LSPCodeActions;

typedef struct {
    char *contents;  /* Hover documentation/markdown */
    char *language;  /* Optional: language hint for code blocks */
} LSPHover;

typedef enum {
    LSP_DIAG_ERROR = 1,
    LSP_DIAG_WARNING = 2,
    LSP_DIAG_INFORMATION = 3,
    LSP_DIAG_HINT = 4,
} LSPDiagnosticSeverity;

typedef struct {
    int start_line;
    int start_col;
    int end_line;
    int end_col;
    LSPDiagnosticSeverity severity;
    char *message;
    char *code;  /* Optional: error code/rule name */
} LSPDiagnostic;

typedef struct {
    LSPDiagnostic *items;
    int count;
} LSPDiagnostics;

typedef struct {
    char *path;              /* Server executable path */
    char **args;             /* Command arguments */
    int   args_count;
    char *language_id;       /* Language identifier (e.g., "c", "rust") */
} LSPServerConfig;

typedef struct {
    int    id;               /* Message ID for request/response tracking */
    int    pid;              /* Process ID of language server */
    int    stdin_fd;         /* stdin pipe fd to server */
    int    stdout_fd;        /* stdout pipe fd from server */
    char  *language_id;      /* Language this server handles */
    LSPStatus status;
    LSPServerConfig config;
    bool   initialized;
} LSPClient;

typedef struct {
    LSPClient *clients;
    int        client_count;
    int        client_capacity;
} LSPManager;

/* Initialization and management */
void lsp_manager_init(LSPManager *manager);
void lsp_manager_free(LSPManager *manager);
void lsp_manager_add_server(LSPManager *manager, const char *language_id, 
                            const char *server_path, const char **args, int args_count);

/* Server lifecycle */
bool lsp_client_start(LSPClient *client);
void lsp_client_stop(LSPClient *client);
bool lsp_client_initialize(LSPClient *client);
LSPStatus lsp_client_get_status(LSPClient *client);

/* Communication - text position based on row/col */
void lsp_client_send_definition_request(LSPClient *client, const char *file_uri, int line, int character);
void lsp_client_send_type_definition_request(LSPClient *client, const char *file_uri, int line, int character);
void lsp_client_send_references_request(LSPClient *client, const char *file_uri, int line, int character);
void lsp_client_send_implementation_request(LSPClient *client, const char *file_uri, int line, int character);
void lsp_client_send_hover_request(LSPClient *client, const char *file_uri, int line, int character);
void lsp_client_send_rename_request(LSPClient *client, const char *file_uri, int line, int character, const char *new_name);
void lsp_client_send_code_action_request(LSPClient *client, const char *file_uri, int start_line, int start_character, int end_line, int end_character);
void lsp_client_send_semantic_tokens_request(LSPClient *client, const char *file_uri);

/* Response parsing */
char *lsp_client_read_response(LSPClient *client);
LSPLocation *lsp_parse_definition_response(const char *response, int *count);
LSPHover *lsp_parse_hover_response(const char *response);
LSPDiagnostics *lsp_parse_diagnostics_response(const char *response);
LSPDiagnostics *lsp_parse_publish_diagnostics_notification(const char *response);
LSPWorkspaceEdit *lsp_parse_rename_response(const char *response);
LSPCodeActions *lsp_parse_code_actions_response(const char *response);
void lsp_free_locations(LSPLocation *locations, int count);
void lsp_free_hover(LSPHover *hover);
void lsp_free_diagnostics(LSPDiagnostics *diag);
void lsp_free_workspace_edit(LSPWorkspaceEdit *edit);
void lsp_free_code_actions(LSPCodeActions *actions);
const char *lsp_hover_get_contents(LSPHover *hover);

/* Language server discovery */
LSPClient *lsp_manager_get_client(LSPManager *manager, const char *language_id);

#endif

