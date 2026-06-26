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

#endif
