#include "dragon_editor/command.h"
#include "dragon_editor/document.h"
#include <string.h>

static Command commands[MAX_COMMANDS];
static int command_count = 0;

void command_registry_init(void) {
    command_count = 0;
    command_register("open",           "Open file",           "File",    cmd_open);
    command_register("save",           "Save",                "File",    cmd_save);
    command_register("save-as",        "Save as",             "File",    cmd_save_as);
    command_register("quit",           "Quit",                "File",    cmd_quit);
    command_register("find",           "Find",                "Edit",    cmd_find);
    command_register("replace",        "Replace",             "Edit",    cmd_replace);
    command_register("goto-line",      "Go to line",          "Navigation", cmd_goto_line);
    command_register("undo",           "Undo",                "Edit",    cmd_undo);
    command_register("redo",           "Redo",                "Edit",    cmd_redo);
    command_register("select-all",     "Select all",          "Selection", cmd_select_all);
    command_register("copy",           "Copy",                "Edit",    cmd_copy);
    command_register("paste",          "Paste",               "Edit",    cmd_paste);
    command_register("cut",            "Cut",                 "Edit",    cmd_cut);
    command_register("delete-line",    "Delete line",         "Edit",    cmd_delete_line);
    command_register("duplicate-line", "Duplicate line",      "Edit",    cmd_duplicate_line);
    command_register("move-line-up",   "Move line up",        "Edit",    cmd_move_line_up);
    command_register("move-line-down", "Move line down",      "Edit",    cmd_move_line_down);
    command_register("indent",         "Indent",              "Edit",    cmd_indent);
    command_register("unindent",       "Unindent",            "Edit",    cmd_unindent);
    command_register("goto-top",       "Go to top",           "Navigation", cmd_goto_top);
    command_register("goto-bottom",    "Go to bottom",        "Navigation", cmd_goto_bottom);
    command_register("goto-start",     "Go to start of line", "Navigation", cmd_goto_start);
    command_register("goto-end",       "Go to end of line",   "Navigation", cmd_goto_end);
    command_register("about",          "About",               "Help",    cmd_about);
    command_register("settings",       "Settings",            "Help",    cmd_settings);
    command_register("bn",             "Buffer next",         "Buffer",  cmd_buffer_next);
    command_register("bp",             "Buffer previous",     "Buffer",  cmd_buffer_prev);
    command_register("bc",             "Buffer close",        "Buffer",  cmd_buffer_close);
}

void command_register(const char *name, const char *label,
                      const char *category, CommandFn fn) {
    if (command_count >= MAX_COMMANDS) return;
    Command *cmd = &commands[command_count++];
    cmd->name = name;
    cmd->label = label;
    cmd->category = category;
    cmd->fn = fn;
}

Command *command_find(const char *name) {
    for (int i = 0; i < command_count; i++)
        if (strcmp(commands[i].name, name) == 0)
            return &commands[i];
    return NULL;
}

Command *command_get_all(int *count) {
    *count = command_count;
    return commands;
}

int command_search(const char *query, Command *results[], int max) {
    int found = 0;
    for (int i = 0; i < command_count && found < max; i++) {
        if (strstr(commands[i].name, query) ||
            strstr(commands[i].label, query)) {
            results[found++] = &commands[i];
        }
    }
    return found;
}

/* --- Command implementations --- */

void cmd_open(App *app) {
    (void)app;
    extern void panel_file_browser_open(App *);
    panel_file_browser_open(app);
}

void cmd_save(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_save(doc);
}

void cmd_save_as(App *app) {
    (void)app;
}

void cmd_quit(App *app) {
    app_quit(app);
}

void cmd_find(App *app) {
    extern void panel_find_open(App *, Document *);
    Document *doc = (Document *)app_get_doc(app);
    panel_find_open(app, doc);
}

void cmd_replace(App *app) {
    (void)app;
}

void cmd_goto_line(App *app) {
    extern void panel_goto_open(App *, Document *);
    Document *doc = (Document *)app_get_doc(app);
    panel_goto_open(app, doc);
}

void cmd_undo(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_undo(doc);
}

void cmd_redo(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_redo(doc);
}

void cmd_select_all(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_select_all(doc);
}

void cmd_copy(App *app) { (void)app; }
void cmd_paste(App *app) { (void)app; }
void cmd_cut(App *app) { (void)app; }

void cmd_delete_line(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_delete_line_at(doc);
}

void cmd_duplicate_line(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    document_insert_char(doc, '\n');
    for (int i = 0; i < len; i++)
        document_insert_char(doc, line[i]);
}

void cmd_move_line_up(App *app) { (void)app; }
void cmd_move_line_down(App *app) { (void)app; }
void cmd_indent(App *app) { (void)app; }
void cmd_unindent(App *app) { (void)app; }

void cmd_goto_top(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_cursor_doc_start(doc);
}

void cmd_goto_bottom(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_cursor_doc_end(doc);
}

void cmd_goto_start(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_cursor_home(doc);
}

void cmd_goto_end(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_cursor_end(doc);
}

void cmd_about(App *app) {
    extern void panel_about_open(App *);
    panel_about_open(app);
}

void cmd_settings(App *app) { (void)app; }

void cmd_buffer_next(App *app) {
    app_next_buffer(app);
}

void cmd_buffer_prev(App *app) {
    app_prev_buffer(app);
}

void cmd_buffer_close(App *app) {
    int current = app_get_current_buffer_index(app);
    app_close_buffer(app, current);
}
