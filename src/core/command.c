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
    command_register("open-workspace", "Open workspace",      "Workspace", cmd_open_workspace);
    command_register("cwd",            "Change working dir",  "Workspace", cmd_change_dir);
    command_register("tree-sitter-subtree", "Tree-sitter subtree", "LSP", cmd_tree_sitter_inspect);
    command_register("ts-subtree",     "Tree-sitter subtree", "LSP", cmd_tree_sitter_inspect);
    command_register("tree-sitter-highlight-name", "Tree-sitter highlight", "LSP", cmd_tree_sitter_inspect);
    command_register("lsp-stop",       "LSP stop",            "LSP", cmd_lsp_stop);
    command_register("lsp-restart",    "LSP restart",         "LSP", cmd_lsp_restart);
    command_register("workspace-symbols", "Workspace symbols", "LSP", cmd_workspace_symbols);
    command_register("workspace-diagnostics", "Workspace diagnostics", "LSP", cmd_workspace_diagnostics);
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
    extern void panel_file_browser_open_save_as(App *);
    panel_file_browser_open_save_as(app);
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
    extern void panel_find_open_replace(App *, Document *);
    Document *doc = (Document *)app_get_doc(app);
    panel_find_open_replace(app, doc);
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

void cmd_copy(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_yank(doc);
}
void cmd_paste(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_paste(doc);
}
void cmd_cut(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_delete_selection(doc);
}

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

void cmd_move_line_up(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_move_line_up(doc);
}
void cmd_move_line_down(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_move_line_down(doc);
}
void cmd_indent(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_indent_selection(doc);
}
void cmd_unindent(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_dedent_selection(doc);
}

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

void cmd_settings(App *app) {
    extern void panel_settings_open(App *);
    panel_settings_open(app);
}

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

void cmd_open_workspace(App *app) {
    extern void panel_file_browser_open_workspace(App *);
    panel_file_browser_open_workspace(app);
}

void cmd_change_dir(App *app) {
    extern void panel_file_browser_open_change_dir(App *);
    panel_file_browser_open_change_dir(app);
}

void cmd_tree_sitter_inspect(App *app) {
    extern void panel_treesitter_inspector_open(App *);
    panel_treesitter_inspector_open(app);
}

void cmd_lsp_stop(App *app) {
    lsp_manager_stop_all((LSPManager *)app_get_lsp_manager(app));
}

void cmd_lsp_restart(App *app) {
    lsp_manager_restart_all((LSPManager *)app_get_lsp_manager(app));
}

void cmd_workspace_symbols(App *app) {
    extern void panel_workspace_symbols_open(App *);
    panel_workspace_symbols_open(app);
}

void cmd_workspace_diagnostics(App *app) {
    extern void panel_workspace_diagnostics_open(App *);
    panel_workspace_diagnostics_open(app);
}

/* New commands */

void cmd_goto_alternate(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_goto_alternate(doc);
}

void cmd_goto_last_mod(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_goto_last_modification(doc);
}

void cmd_jumplist_backward(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_jumplist_backward(doc);
}

void cmd_jumplist_forward(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_jumplist_forward(doc);
}

void cmd_select_iw(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    document_select_inside_word(doc);
}

void cmd_select_aw(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    document_select_around_word(doc);
}

void cmd_select_iparen(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    document_select_inside_paren(doc);
}

void cmd_select_icurly(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    document_select_inside_curly(doc);
}

void cmd_comment_toggle(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_comment_toggle(doc);
}

void cmd_comment_block(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    const LanguageSettings *ls = language_settings_get(doc->language_id);
    if (ls && ls->comment_open && ls->comment_open[0])
        document_comment_toggle_block(doc, ls->comment_open, ls->comment_close);
    else
        document_comment_toggle_block(doc, "/*", "*/");
}

void cmd_reflow(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_reflow(doc, 80);
}

void cmd_retab(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    const LanguageSettings *ls = language_settings_get(doc->language_id);
    int tw = ls ? ls->tab_width : 4;
    document_indent_style_to_tabs(doc, tw);
}

void cmd_expandtab(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    const LanguageSettings *ls = language_settings_get(doc->language_id);
    int sw = ls ? ls->tab_width : 4;
    document_indent_style_to_spaces(doc, sw);
}

void cmd_sort(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_sort_selection(doc);
}

void cmd_format(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_format_selection(doc);
}

void cmd_yank_clipboard(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_yank_to_system_clipboard(doc);
}

void cmd_yank_primary_clipboard(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_yank_main_to_system_clipboard(doc);
}

void cmd_paste_clipboard(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_paste_from_system_clipboard(doc);
}

void cmd_paste_before_clipboard(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_paste_before_from_system_clipboard(doc);
}

void cmd_replace_clipboard(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    document_replace_selection_from_system_clipboard(doc);
}

void cmd_macro_record(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    if (macro_is_recording(&doc->macros))
        macro_stop_record(&doc->macros);
    else
        macro_start_record(&doc->macros, 0);
}

void cmd_macro_replay(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    macro_replay(&doc->macros, 0);
}

void cmd_split_v(App *app) { app_split_vertical(app); }
void cmd_split_h(App *app) { app_split_horizontal(app); }
void cmd_close_split(App *app) { app_close_split(app); }
void cmd_win_left(App *app) { app_goto_window_left(app); }
void cmd_win_right(App *app) { app_goto_window_right(app); }
void cmd_win_up(App *app) { app_goto_window_up(app); }
void cmd_win_down(App *app) { app_goto_window_down(app); }
void cmd_win_maximize(App *app) { app_maximize_window(app); }
void cmd_win_equalize(App *app) { app_equalize_windows(app); }
void cmd_win_next(App *app) { app_next_window(app); }
void cmd_win_prev(App *app) { app_prev_window(app); }
