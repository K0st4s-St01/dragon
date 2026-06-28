#ifndef DE_DOCUMENT_H
#define DE_DOCUMENT_H

#include "buffer.h"
#include "cursor.h"
#include "history.h"
#include "syntax.h"
#include "lsp.h"
#include <stdbool.h>

/* Macros */
#define MACRO_MAX_KEYS 1024
#define MACRO_SLOTS 26

typedef struct {
    int keys[MACRO_MAX_KEYS];
    int len;
    bool recording;
} MacroSlot;

typedef struct {
    MacroSlot slots[MACRO_SLOTS];
    int active_slot;     /* -1 when not recording */
    int last_replayed;   /* slot index of last replayed macro, for @@ */
} MacroState;

typedef struct {
    char *uri;
    int line;
    int character;
} LSPGotoResult;

typedef struct {
    Buffer  buffer;
    Cursor  cursors[64];
    int     cursor_count;
    char   *filepath;
    bool    dirty;
    bool    syntax_dirty;
    bool    lsp_dirty;
    bool    lsp_opened;
    int     lsp_version;
    int     scroll_y;
    int     scroll_x;
    int     viewport_lines;
    int     viewport_cols;
    History history;
    char   *clipboard;
    size_t  clipboard_len;
    char   *search_query;
    size_t  search_len;
    int     jumplist[256][2];
    int     jumplist_len;
    int     jumplist_pos;
    char   *language_id;  /* Language identifier for LSP */
    SyntaxHighlighting syntax;  /* Syntax highlighting tokens from LSP */
    LSPGotoResult *goto_results;  /* Results from last LSP goto command */
    int goto_result_count;
    void *hover_result;  /* LSPHover* from LSP hover request */
    void *diagnostics;  /* LSPDiagnostics* from LSP publish diagnostics */
    bool  ts_parsed;    /* True if tree-sitter has parsed this document */
    bool  ts_attempted; /* True if tree-sitter was tried for current content */
    MacroState macros;  /* Macro recording/playback state */
    char  *alt_filepath; /* Alternate (previous) file path for ga */
} Document;

void document_init(Document *doc);
void document_free(Document *doc);
void document_mark_dirty(Document *doc);
void document_open(Document *doc, const char *path);
void document_save(Document *doc);
void document_save_as(Document *doc, const char *path);
void document_notify_lsp_open(Document *doc, void *lsp_manager);
void document_notify_lsp_change(Document *doc, void *lsp_manager);

void document_insert_char(Document *doc, char c);
void document_delete_char(Document *doc);
void document_delete_selection(Document *doc);
void document_newline(Document *doc);
void document_insert_text(Document *doc, const char *text);

void document_move_cursor(Document *doc, int dr, int dc);
void document_cursor_to(Document *doc, int row, int col);
void document_cursor_home(Document *doc);
void document_cursor_end(Document *doc);
void document_cursor_page_up(Document *doc);
void document_cursor_page_down(Document *doc);
void document_cursor_doc_start(Document *doc);
void document_cursor_doc_end(Document *doc);
void document_select_word(Document *doc);
void document_select_line(Document *doc);
void document_select_all(Document *doc);

void document_scroll_up(Document *doc);
void document_scroll_down(Document *doc);
void document_sync_viewport_to_cursor(Document *doc);

void document_undo(Document *doc);
void document_redo(Document *doc);

void document_cursor_word_forward(Document *doc);
void document_cursor_word_backward(Document *doc);
void document_cursor_word_end(Document *doc);
void document_cursor_WORD_forward(Document *doc);
void document_cursor_WORD_backward(Document *doc);
void document_cursor_WORD_end(Document *doc);
void document_increment_number(Document *doc);
void document_decrement_number(Document *doc);

void document_yank(Document *doc);
void document_paste(Document *doc);
void document_delete_line_at(Document *doc);

void document_add_cursor(Document *doc);
void document_remove_last_cursor(Document *doc);
void document_clear_cursors(Document *doc);
void document_insert_char_multi(Document *doc, char c);
void document_delete_char_multi(Document *doc);
void document_newline_multi(Document *doc);

void document_replace_char(Document *doc, char c);
void document_open_line_below(Document *doc);
void document_open_line_above(Document *doc);
void document_cursor_first_non_blank(Document *doc);
void document_join_lines(Document *doc);
void document_change_selection(Document *doc);
void document_substitute_char(Document *doc);
void document_delete_char_at_cursor(Document *doc);
void document_indent_line(Document *doc);
void document_dedent_line(Document *doc);
void document_yank_line(Document *doc);

void document_replace_selection_char(Document *doc, char c);
void document_replace_selection_yanked(Document *doc);
void document_toggle_case(Document *doc);
void document_lowercase(Document *doc);
void document_uppercase(Document *doc);
void document_indent_selection(Document *doc);
void document_dedent_selection(Document *doc);
void document_collapse_selection(Document *doc);
void document_keep_primary_selection(Document *doc);
void document_flip_cursor_anchor(Document *doc);
void document_copy_selection_below(Document *doc);
void document_join_lines_selection(Document *doc);
void document_find_char_forward(Document *doc, char c);
void document_find_char_backward(Document *doc, char c);
void document_till_char_forward(Document *doc, char c);
void document_till_char_backward(Document *doc, char c);
void document_scroll_center(Document *doc);
void document_scroll_horizontal_center(Document *doc);
void document_scroll_top(Document *doc, int viewport_h);
void document_scroll_bottom(Document *doc, int viewport_h);
void document_search_next(Document *doc);
void document_search_prev(Document *doc);
void document_set_search(Document *doc, const char *query, size_t len);
void document_extend_to_line_bounds(Document *doc);
void document_shrink_to_line_bounds(Document *doc);
void document_remove_primary_selection(Document *doc);
void document_goto_line_start(Document *doc);
void document_goto_line_end(Document *doc);
void document_goto_view_top(Document *doc);
void document_goto_view_center(Document *doc);
void document_goto_view_bottom(Document *doc);
void document_paste_before(Document *doc);
void document_search_word(Document *doc, bool word_boundary);
void document_half_page_down(Document *doc, int viewport_h);
void document_half_page_up(Document *doc, int viewport_h);
void document_force_selection_forward(Document *doc);
void document_rotate_selections_backward(Document *doc);
void document_rotate_selections_forward(Document *doc);
void document_rotate_selection_contents_backward(Document *doc);
void document_rotate_selection_contents_forward(Document *doc);
void document_delete_word_forward(Document *doc);
void document_split_selection_newlines(Document *doc);
void document_merge_selections(Document *doc);
void document_merge_consecutive_selections(Document *doc);
void document_trim_whitespace(Document *doc);
void document_copy_selection_above(Document *doc);
void document_join_lines_with_space(Document *doc);
void document_comment_toggle(Document *doc);
void document_format_selection(Document *doc);
void document_move_line_up(Document *doc);
void document_move_line_down(Document *doc);
void document_page_down_extend(Document *doc);
void document_page_up_extend(Document *doc);
void document_half_page_down_extend(Document *doc, int viewport_h);
void document_half_page_up_extend(Document *doc, int viewport_h);
void document_select_regex(Document *doc, const char *pattern, size_t len);
void document_split_regex(Document *doc, const char *pattern, size_t len);
void document_new(Document *doc);
void document_sort_selection(Document *doc);
void document_surround(Document *doc, char c);
void document_delete_surround(Document *doc, char c);
void document_align_selections(Document *doc);
void document_jumplist_push(Document *doc, int row, int col);
void document_jumplist_backward(Document *doc);
void document_jumplist_forward(Document *doc);
void document_select_literal(Document *doc, const char *pattern, size_t len);
void document_select_all_matches(Document *doc, const char *pattern, size_t len);
void document_split_literal(Document *doc, const char *pattern, size_t len);
void document_split_all_matches(Document *doc, const char *pattern, size_t len);
void document_keep_matching(Document *doc, const char *pattern, size_t len);
void document_remove_matching(Document *doc, const char *pattern, size_t len);
void document_replace_surround(Document *doc, char from, char to);
void document_go_to_file(Document *doc);
void document_view_page_down(Document *doc);
void document_view_page_up(Document *doc);
void document_view_half_page_down(Document *doc);
void document_view_half_page_up(Document *doc);
void document_match_bracket(Document *doc);
void document_goto_last_modification(Document *doc);
void document_insert_file(Document *doc, const char *path);
void document_move_file(Document *doc, const char *path);
void document_pipe_selection(Document *doc, const char *cmd);
void document_pipe_to(Document *doc, const char *cmd);
void document_insert_output(Document *doc, const char *cmd);
void document_append_output(Document *doc, const char *cmd);
void document_filter_selection(Document *doc, const char *cmd);

/* Language detection and LSP */
void document_detect_language(Document *doc);
void document_lsp_goto_definition(Document *doc, void *lsp_manager);
void document_lsp_goto_type_definition(Document *doc, void *lsp_manager);
void document_lsp_goto_references(Document *doc, void *lsp_manager);
void document_lsp_goto_implementation(Document *doc, void *lsp_manager);
void document_lsp_hover(Document *doc, void *lsp_manager);
void document_lsp_select_references(Document *doc, void *lsp_manager);
void document_update_syntax_from_lsp(Document *doc, void *lsp_manager);
bool document_update_syntax_fallback(Document *doc);
void document_update_diagnostics_from_lsp(Document *doc, void *lsp_manager);

/* Diagnostic navigation */
void document_goto_next_diagnostic(Document *doc);
void document_goto_prev_diagnostic(Document *doc);
void document_goto_first_diagnostic(Document *doc);
void document_goto_last_diagnostic(Document *doc);

/* Treesitter integration */
bool document_parse_treesitter(Document *doc, void *ts_manager);
void document_select_treesitter_parent(Document *doc, void *ts_manager);
void document_select_treesitter_child(Document *doc, void *ts_manager);
void document_select_treesitter_sibling(Document *doc, void *ts_manager, int direction);
void document_select_treesitter_all_siblings(Document *doc, void *ts_manager);
void document_select_treesitter_all_children(Document *doc, void *ts_manager);
void document_move_to_treesitter_parent_edge(Document *doc, void *ts_manager, bool end_edge);

/* Text objects: select inside/around */
void document_select_inside_word(Document *doc);
void document_select_around_word(Document *doc);
void document_select_inside_paren(Document *doc);
void document_select_around_paren(Document *doc);
void document_select_inside_bracket(Document *doc);
void document_select_around_bracket(Document *doc);
void document_select_inside_curly(Document *doc);
void document_select_around_curly(Document *doc);
void document_select_inside_angle(Document *doc);
void document_select_around_angle(Document *doc);
void document_select_inside_quote(Document *doc);
void document_select_around_quote(Document *doc);
void document_select_inside_backtick(Document *doc);
void document_select_around_backtick(Document *doc);
void document_select_inside_paragraph(Document *doc);
void document_select_around_paragraph(Document *doc);

/* Macros */
void macro_init(MacroState *ms);
void macro_free(MacroState *ms);
bool macro_start_record(MacroState *ms, int slot);
void macro_stop_record(MacroState *ms);
bool macro_is_recording(const MacroState *ms);
void macro_record_key(MacroState *ms, int key);
bool macro_replay(MacroState *ms, int slot);

/* Reflow / indent style */
void document_reflow(Document *doc, int text_width);
void document_indent_style_to_tabs(Document *doc, int tab_width);
void document_indent_style_to_spaces(Document *doc, int space_width);

/* Block comment */
void document_comment_toggle_block(Document *doc, const char *open, const char *close);
void document_comment_toggle_line(Document *doc, const char *prefix);

/* System clipboard */
bool document_yank_to_system_clipboard(Document *doc);
bool document_paste_from_system_clipboard(Document *doc);
bool document_paste_before_from_system_clipboard(Document *doc);
bool document_replace_selection_from_system_clipboard(Document *doc);
bool document_yank_main_to_system_clipboard(Document *doc);

/* Alternate file */
void document_set_alternate(Document *doc, const char *path);
const char *document_get_alternate(const Document *doc);
void document_goto_alternate(Document *doc);

/* Window / split management */
#define MAX_WINDOWS 16

typedef struct {
    int doc_index;          /* Index into document array */
    int x, y, width, height; /* Viewport geometry */
    int scroll_y, scroll_x;
    bool visible;
    int parent;             /* Index of parent split (-1 for root) */
    bool is_horizontal;     /* true = split horizontally (side by side) */
    int active_child;       /* Active child window index */
} Window;

typedef struct {
    Window windows[MAX_WINDOWS];
    int count;
    int active;             /* Index of active window */
} WindowManager;

void window_manager_init(WindowManager *wm);
int window_split_vertical(WindowManager *wm, int doc_index);
int window_split_horizontal(WindowManager *wm, int doc_index);
void window_close(WindowManager *wm);
void window_next(WindowManager *wm);
void window_prev(WindowManager *wm);
void window_goto_left(WindowManager *wm);
void window_goto_right(WindowManager *wm);
void window_goto_up(WindowManager *wm);
void window_goto_down(WindowManager *wm);
void window_swap_left(WindowManager *wm);
void window_swap_right(WindowManager *wm);
void window_swap_up(WindowManager *wm);
void window_swap_down(WindowManager *wm);
void window_maximize(WindowManager *wm);
void window_equalize(WindowManager *wm);

/* Per-language settings */
typedef struct {
    const char *language;
    int tab_width;
    bool use_tabs;          /* true = tabs, false = spaces */
    const char *comment_open;   /* e.g. slash-star for C */
    const char *comment_close;  /* e.g. star-slash for C */
    const char *line_comment;   /* e.g. "//" for C */
    bool auto_format;
} LanguageSettings;

const LanguageSettings *language_settings_get(const char *language_id);
void language_settings_detect(Document *doc, LanguageSettings *out);

/* LSP text editing */
void document_apply_text_edit(Document *doc, const LSPTextEdit *edit);
void document_apply_workspace_edit(Document *doc, const LSPWorkspaceEdit *edit);
bool document_format_with_lsp(Document *doc, void *lsp_manager, int tab_size, bool insert_spaces);

#endif
