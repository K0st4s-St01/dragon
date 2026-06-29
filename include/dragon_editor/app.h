#ifndef DE_APP_H
#define DE_APP_H

#include <stdbool.h>
#include "renderer.h"
#include "config.h"
#include "document.h"

typedef struct App App;

typedef enum {
    APP_LSP_GOTO_DEFINITION,
    APP_LSP_GOTO_TYPE_DEFINITION,
    APP_LSP_GOTO_REFERENCES,
    APP_LSP_GOTO_IMPLEMENTATION
} AppLSPGotoKind;

App  *app_create(int width, int height, const char *title);
void  app_destroy(App *app);
void  app_run(App *app);
void  app_quit(App *app);
void  app_open_file(App *app, const char *path);

int        app_get_width(App *app);
int        app_get_height(App *app);
double     app_get_dt(App *app);
void      *app_get_doc(App *app);
void      *app_get_doc_at(App *app, int index);
void      *app_get_mode(App *app);
Renderer  *app_get_renderer(App *app);

void app_set_clipboard(App *app, const char *text);
const char *app_get_clipboard(App *app);

/* Buffer management */
int  app_get_buffer_count(App *app);
int  app_get_current_buffer_index(App *app);
void app_switch_to_buffer(App *app, int index);
void app_next_buffer(App *app);
void app_prev_buffer(App *app);
bool app_close_buffer(App *app, int index);

/* Window/split management */
WindowManager *app_get_window_manager(App *app);
void app_split_vertical(App *app);
void app_split_horizontal(App *app);
void app_close_split(App *app);
void app_next_window(App *app);
void app_prev_window(App *app);
void app_goto_window_left(App *app);
void app_goto_window_right(App *app);
void app_goto_window_up(App *app);
void app_goto_window_down(App *app);
void app_swap_window_left(App *app);
void app_swap_window_right(App *app);
void app_swap_window_up(App *app);
void app_swap_window_down(App *app);
void app_maximize_window(App *app);
void app_equalize_windows(App *app);

/* Workspace management */
const char *app_get_workspace_root(App *app);
void app_set_workspace_root(App *app, const char *path);

/* LSP management */
void *app_get_lsp_manager(App *app);
void  app_lsp_request_goto(App *app, AppLSPGotoKind kind);
void  app_lsp_select_references(App *app);
void  app_format_document(App *app);

/* Tree-Sitter management */
void *app_get_treesitter_manager(App *app);

/* Config */
Config *app_get_config(App *app);
bool app_apply_theme(App *app, const char *name);
bool app_reload_config(App *app);
bool app_set_plugin_enabled(App *app, int index, bool enabled);

#endif
