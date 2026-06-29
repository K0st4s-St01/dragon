#ifndef DE_COMMAND_H
#define DE_COMMAND_H

#include "app.h"
#include "document.h"

typedef void (*CommandFn)(App *app);

typedef struct {
    const char *name;
    const char *label;
    const char *category;
    CommandFn   fn;
} Command;

#define MAX_COMMANDS 128

void     command_registry_init(void);
void     command_register(const char *name, const char *label,
                          const char *category, CommandFn fn);
Command *command_find(const char *name);
Command *command_get_all(int *count);
int      command_search(const char *query, Command *results[], int max_results);

void     cmd_open(App *app);
void     cmd_save(App *app);
void     cmd_save_as(App *app);
void     cmd_quit(App *app);
void     cmd_find(App *app);
void     cmd_replace(App *app);
void     cmd_goto_line(App *app);
void     cmd_undo(App *app);
void     cmd_redo(App *app);
void     cmd_select_all(App *app);
void     cmd_copy(App *app);
void     cmd_paste(App *app);
void     cmd_cut(App *app);
void     cmd_delete_line(App *app);
void     cmd_duplicate_line(App *app);
void     cmd_move_line_up(App *app);
void     cmd_move_line_down(App *app);
void     cmd_indent(App *app);
void     cmd_unindent(App *app);
void     cmd_goto_top(App *app);
void     cmd_goto_bottom(App *app);
void     cmd_goto_start(App *app);
void     cmd_goto_end(App *app);
void     cmd_about(App *app);
void     cmd_settings(App *app);
void     cmd_buffer_next(App *app);
void     cmd_buffer_prev(App *app);
void     cmd_buffer_close(App *app);
void     cmd_open_workspace(App *app);
void     cmd_change_dir(App *app);
void     cmd_tree_sitter_inspect(App *app);
void     cmd_lsp_stop(App *app);
void     cmd_lsp_restart(App *app);
void     cmd_workspace_symbols(App *app);
void     cmd_workspace_diagnostics(App *app);
void     cmd_plugins(App *app);

/* New commands */
void     cmd_goto_alternate(App *app);
void     cmd_goto_last_mod(App *app);
void     cmd_jumplist_backward(App *app);
void     cmd_jumplist_forward(App *app);
void     cmd_select_iw(App *app);
void     cmd_select_aw(App *app);
void     cmd_select_iparen(App *app);
void     cmd_select_icurly(App *app);
void     cmd_comment_toggle(App *app);
void     cmd_comment_block(App *app);
void     cmd_reflow(App *app);
void     cmd_retab(App *app);
void     cmd_expandtab(App *app);
void     cmd_sort(App *app);
void     cmd_format(App *app);
void     cmd_yank_clipboard(App *app);
void     cmd_yank_primary_clipboard(App *app);
void     cmd_paste_clipboard(App *app);
void     cmd_paste_before_clipboard(App *app);
void     cmd_replace_clipboard(App *app);
void     cmd_macro_record(App *app);
void     cmd_macro_replay(App *app);
void     cmd_split_v(App *app);
void     cmd_split_h(App *app);
void     cmd_close_split(App *app);
void     cmd_win_left(App *app);
void     cmd_win_right(App *app);
void     cmd_win_up(App *app);
void     cmd_win_down(App *app);
void     cmd_win_maximize(App *app);
void     cmd_win_equalize(App *app);
void     cmd_win_next(App *app);
void     cmd_win_prev(App *app);

#endif
