#ifndef DE_DOCUMENT_H
#define DE_DOCUMENT_H

#include "buffer.h"
#include "cursor.h"
#include "history.h"
#include <stdbool.h>

typedef struct {
    Buffer  buffer;
    Cursor  cursors[64];
    int     cursor_count;
    char   *filepath;
    bool    dirty;
    int     scroll_y;
    int     viewport_lines;
    History history;
    char   *clipboard;
    size_t  clipboard_len;
    char   *search_query;
    size_t  search_len;
    int     jumplist[256][2];
    int     jumplist_len;
    int     jumplist_pos;
} Document;

void document_init(Document *doc);
void document_free(Document *doc);
void document_open(Document *doc, const char *path);
void document_save(Document *doc);
void document_save_as(Document *doc, const char *path);

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

#endif
