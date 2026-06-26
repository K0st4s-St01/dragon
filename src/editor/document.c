#include "dragon_editor/document.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_CURSORS 64

void document_init(Document *doc) {
    memset(doc, 0, sizeof(*doc));
    buffer_init(&doc->buffer);
    doc->cursor_count = 1;
    cursor_init(&doc->cursors[0]);
    doc->viewport_lines = 40;
    history_init(&doc->history);
}

void document_free(Document *doc) {
    buffer_free(&doc->buffer);
    history_free(&doc->history);
    free(doc->filepath);
    free(doc->clipboard);
    memset(doc, 0, sizeof(*doc));
}

void document_open(Document *doc, const char *path) {
    buffer_load(&doc->buffer, path);
    free(doc->filepath);
    doc->filepath = strdup(path);
    doc->dirty = false;
    doc->cursors[0] = (Cursor){0};
    doc->scroll_y = 0;
    history_clear(&doc->history);
}

void document_save(Document *doc) {
    if (!doc->filepath) return;
    buffer_save(&doc->buffer, doc->filepath);
    doc->dirty = false;
}

void document_save_as(Document *doc, const char *path) {
    free(doc->filepath);
    doc->filepath = strdup(path);
    buffer_save(&doc->buffer, path);
    doc->dirty = false;
}

void document_insert_char(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, &c, 1);
    history_push_insert(&doc->history, pos, &c, 1, cur->row, cur->col - 1);
    cur->col++;
    doc->dirty = true;
}

void document_delete_char(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (cur->row == 0 && cur->col == 0) return;
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos == 0) return;
    char deleted = doc->buffer.text[pos - 1];
    buffer_delete(&doc->buffer, pos - 1, 1);
    history_push_delete(&doc->history, pos - 1, &deleted, 1, cur->row, cur->col);
    if (cur->col > 0) cur->col--;
    else {
        cur->row--;
        int len = (int)buffer_line_len(&doc->buffer, cur->row);
        cur->col = len > 0 ? len - 1 : 0;
    }
    doc->dirty = true;
}

void document_delete_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    size_t len = end - start;
    char *deleted = malloc(len);
    memcpy(deleted, doc->buffer.text + start, len);
    buffer_delete(&doc->buffer, start, len);
    history_push_delete(&doc->history, start, deleted, len, sr, sc);
    free(deleted);
    cursor_move_to(cur, sr, sc);
    cursor_clear_selection(cur);
    doc->dirty = true;
}

void document_newline(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    char nl = '\n';
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, &nl, 1);
    history_push_insert(&doc->history, pos, &nl, 1, cur->row, cur->col);
    cur->row++;
    cur->col = 0;
    doc->dirty = true;

    /* auto-indent: copy leading whitespace from previous line */
    if (cur->row > 0) {
        const char *prev = buffer_line_ptr(&doc->buffer, cur->row - 1);
        int indent = 0;
        while (prev[indent] == ' ' || prev[indent] == '\t') indent++;
        if (indent > 0) {
            char *spaces = malloc(indent);
            memset(spaces, ' ', indent);
            size_t ipos = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
            buffer_insert(&doc->buffer, ipos, spaces, indent);
            history_push_insert(&doc->history, ipos, spaces, indent, cur->row, 0);
            cur->col = indent;
            free(spaces);
        }
    }
}

void document_insert_text(Document *doc, const char *text) {
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    size_t len = strlen(text);
    buffer_insert(&doc->buffer, pos, text, len);
    history_push_insert(&doc->history, pos, text, len, cur->row, cur->col);
    for (const char *p = text; *p; p++) {
        if (*p == '\n') { cur->row++; cur->col = 0; }
        else cur->col++;
    }
    doc->dirty = true;
}

void document_move_cursor(Document *doc, int dr, int dc) {
    Cursor *cur = &doc->cursors[0];
    cur->row += dr;
    cur->col += dc;
    if (cur->row < 0) cur->row = 0;
    if (cur->col < 0) {
        if (cur->row > 0) {
            cur->row--;
            cur->col = (int)buffer_line_len(&doc->buffer, cur->row) - 1;
            if (cur->col < 0) cur->col = 0;
        } else {
            cur->col = 0;
        }
    }
    size_t max_col = buffer_line_len(&doc->buffer, cur->row);
    if (max_col > 0 && cur->col >= (int)max_col)
        cur->col = (int)max_col - 1;
    if (cur->col < 0) cur->col = 0;
}

void document_cursor_to(Document *doc, int row, int max_row) {
    Cursor *cur = &doc->cursors[0];
    if (row < 0) row = 0;
    if (row > max_row) row = max_row;
    cur->row = row;
    size_t len = buffer_line_len(&doc->buffer, cur->row);
    if (cur->col >= (int)len) cur->col = (int)len - 1;
    if (cur->col < 0) cur->col = 0;
}

void document_cursor_home(Document *doc) {
    doc->cursors[0].col = 0;
}

void document_cursor_end(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    cur->col = len > 0 ? len - 1 : 0;
}

void document_cursor_page_up(Document *doc) {
    document_move_cursor(doc, -doc->viewport_lines, 0);
}

void document_cursor_page_down(Document *doc) {
    document_move_cursor(doc, doc->viewport_lines, 0);
}

void document_cursor_doc_start(Document *doc) {
    doc->cursors[0].row = 0;
    doc->cursors[0].col = 0;
}

void document_cursor_doc_end(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    size_t lines = buffer_line_count(&doc->buffer);
    cur->row = (int)lines - 1;
    if (cur->row < 0) cur->row = 0;
    document_cursor_end(doc);
}

void document_select_word(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cursor_select_start(cur);
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    while (cur->col < len && line[cur->col] != ' ' && line[cur->col] != '\n')
        cur->col++;
}

void document_select_line(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cursor_select_start(cur);
    cur->col = 0;
    cur->col = (int)buffer_line_len(&doc->buffer, cur->row);
    if (cur->col > 0) cur->col--;
}

void document_select_all(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cur->row = 0;
    cur->col = 0;
    cursor_select_start(cur);
    document_cursor_doc_end(doc);
}

void document_scroll_up(Document *doc) {
    if (doc->scroll_y > 0) doc->scroll_y--;
}

void document_scroll_down(Document *doc) {
    doc->scroll_y++;
}

void document_undo(Document *doc) {
    HistoryEntry *e = history_peek_undo(&doc->history);
    if (!e) return;
    Cursor *cur = &doc->cursors[0];
    if (e->type == OP_INSERT) {
        buffer_delete(&doc->buffer, e->pos, e->len);
    } else {
        buffer_insert(&doc->buffer, e->pos, e->text, e->len);
    }
    cursor_move_to(cur, e->cursor_row, e->cursor_col);
    history_regress(&doc->history);
    doc->dirty = true;
}

void document_redo(Document *doc) {
    HistoryEntry *e = history_peek_redo(&doc->history);
    if (!e) return;
    Cursor *cur = &doc->cursors[0];
    if (e->type == OP_INSERT) {
        buffer_insert(&doc->buffer, e->pos, e->text, e->len);
    } else {
        buffer_delete(&doc->buffer, e->pos, e->len);
    }
    size_t new_pos = e->pos;
    if (e->type == OP_INSERT) new_pos += e->len;
    int row, col;
    buffer_row_col_from_pos(&doc->buffer, new_pos, &row, &col);
    cursor_move_to(cur, row, col);
    history_advance(&doc->history);
    doc->dirty = true;
}

void document_cursor_word_forward(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    if (cur->col >= len) {
        if ((size_t)cur->row + 1 < buffer_line_count(&doc->buffer)) {
            cur->row++;
            cur->col = 0;
        }
        return;
    }
    int i = cur->col;
    while (i < len && line[i] != '\n' && (line[i] == ' ' || line[i] == '\t'))
        i++;
    if (i < len && (line[i] != ' ' && line[i] != '\t'))
        while (i < len && line[i] != '\n' && line[i] != ' ' && line[i] != '\t')
            i++;
    cur->col = i < len ? i : (len > 0 ? len - 1 : 0);
}

void document_cursor_word_backward(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int i = cur->col;
    if (i > 0) i--;
    while (i > 0 && (line[i] == ' ' || line[i] == '\t'))
        i--;
    while (i > 0 && line[i - 1] != ' ' && line[i - 1] != '\t' && line[i - 1] != '\n')
        i--;
    cur->col = i;
}

void document_cursor_word_end(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    int i = cur->col;
    if (i < len - 1) i++;
    while (i < len - 1 && (line[i] == ' ' || line[i] == '\t'))
        i++;
    while (i < len - 1 && line[i + 1] != ' ' && line[i + 1] != '\t' && line[i + 1] != '\n')
        i++;
    if (i < len && line[i] != '\n')
        cur->col = i;
}

void document_yank(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    free(doc->clipboard);
    doc->clipboard = NULL;
    doc->clipboard_len = 0;

    if (cur->has_selection) {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
        size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
        doc->clipboard_len = end - start;
        doc->clipboard = malloc(doc->clipboard_len);
        memcpy(doc->clipboard, doc->buffer.text + start, doc->clipboard_len);
        cursor_clear_selection(cur);
    } else {
        const char *line = buffer_line_ptr(&doc->buffer, cur->row);
        int len = (int)buffer_line_len(&doc->buffer, cur->row);
        doc->clipboard_len = len;
        doc->clipboard = malloc(len + 1);
        memcpy(doc->clipboard, line, len);
        if (len > 0 && doc->clipboard[len - 1] == '\n') {
            doc->clipboard_len = len;
        } else {
            doc->clipboard[len] = '\n';
            doc->clipboard_len = len + 1;
        }
    }
}

void document_paste(Document *doc) {
    if (!doc->clipboard || doc->clipboard_len == 0) return;
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, doc->clipboard, doc->clipboard_len);
    history_push_insert(&doc->history, pos, doc->clipboard, doc->clipboard_len,
                        cur->row, cur->col);
    for (size_t i = 0; i < doc->clipboard_len; i++) {
        if (doc->clipboard[i] == '\n') { cur->row++; cur->col = 0; }
        else cur->col++;
    }
    doc->dirty = true;
}

void document_delete_line_at(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    size_t start = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
    size_t del_len = len;
    if (del_len > 0 && line[del_len - 1] == '\n') {
        /* Include the newline in the deletion */
    } else if ((size_t)cur->row + 1 < buffer_line_count(&doc->buffer)) {
        del_len++;
    }
    char *deleted = malloc(del_len);
    memcpy(deleted, doc->buffer.text + start, del_len);
    buffer_delete(&doc->buffer, start, del_len);
    history_push_delete(&doc->history, start, deleted, del_len, cur->row, 0);
    free(deleted);
    if (cur->row >= (int)buffer_line_count(&doc->buffer))
        cur->row = (int)buffer_line_count(&doc->buffer) - 1;
    if (cur->row < 0) cur->row = 0;
    cur->col = 0;
    doc->dirty = true;
}

static void get_word_at_cursor(Cursor *cur, Buffer *buf, const char **word, int *word_len) {
    const char *line = buffer_line_ptr(buf, cur->row);
    int len = (int)buffer_line_len(buf, cur->row);
    int start = cur->col;
    int end = cur->col;
    while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t' &&
           line[start - 1] != '\n' && line[start - 1] != '(' && line[start - 1] != ')' &&
           line[start - 1] != '{' && line[start - 1] != '}' && line[start - 1] != ';' &&
           line[start - 1] != ',' && line[start - 1] != '=' && line[start - 1] != '+')
        start--;
    while (end < len && line[end] != ' ' && line[end] != '\t' &&
           line[end] != '\n' && line[end] != '(' && line[end] != ')' &&
           line[end] != '{' && line[end] != '}' && line[end] != ';' &&
           line[end] != ',' && line[end] != '=' && line[end] != '+')
        end++;
    *word = line + start;
    *word_len = end - start;
}

void document_add_cursor(Document *doc) {
    if (doc->cursor_count >= 64) return;

    Cursor *primary = &doc->cursors[0];
    const char *word;
    int word_len;
    get_word_at_cursor(primary, &doc->buffer, &word, &word_len);
    if (word_len == 0) return;

    /* Search forward from the end of current word for next occurrence */
    size_t search_start = buffer_pos_from_row_col(&doc->buffer, primary->row, primary->col) + word_len;
    const char *text = doc->buffer.text;
    size_t total = doc->buffer.len;

    while (search_start < total) {
        const char *found = memmem(text + search_start, total - search_start, word, word_len);
        if (!found) break;
        size_t pos = (size_t)(found - text);

        /* Check that it's a whole word match */
        if (pos > 0 && text[pos - 1] != ' ' && text[pos - 1] != '\t' &&
            text[pos - 1] != '\n' && text[pos - 1] != '(' && text[pos - 1] != ')' &&
            text[pos - 1] != '{' && text[pos - 1] != '}' && text[pos - 1] != ';' &&
            text[pos - 1] != ',' && text[pos - 1] != '=' && text[pos - 1] != '+') {
            search_start = pos + 1;
            continue;
        }
        if (pos + (size_t)word_len < total) {
            char after = text[pos + word_len];
            if (after != ' ' && after != '\t' && after != '\n' &&
                after != '(' && after != ')' && after != '{' && after != '}' &&
                after != ';' && after != ',' && after != '=' && after != '+') {
                search_start = pos + 1;
                continue;
            }
        }

        int row, col;
        buffer_row_col_from_pos(&doc->buffer, pos, &row, &col);
        Cursor *new_cur = &doc->cursors[doc->cursor_count];
        cursor_init(new_cur);
        cursor_move_to(new_cur, row, col);
        doc->cursor_count++;
        return;
    }
}

void document_remove_last_cursor(Document *doc) {
    if (doc->cursor_count > 1)
        doc->cursor_count--;
}

void document_clear_cursors(Document *doc) {
    doc->cursor_count = 1;
    cursor_init(&doc->cursors[0]);
}

void document_insert_char_multi(Document *doc, char c) {
    /* Sort cursor indices by buffer position ascending, then iterate descending */
    int order[64];
    for (int i = 0; i < doc->cursor_count; i++) order[i] = i;
    for (int i = 1; i < doc->cursor_count; i++) {
        for (int j = i; j > 0; j--) {
            size_t pa = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j]].row, doc->cursors[order[j]].col);
            size_t pb = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j-1]].row, doc->cursors[order[j-1]].col);
            if (pa < pb) { int tmp = order[j]; order[j] = order[j-1]; order[j-1] = tmp; }
            else break;
        }
    }
    /* Insert from highest position to lowest so earlier positions aren't invalidated */
    for (int i = doc->cursor_count - 1; i >= 0; i--) {
        Cursor *cur = &doc->cursors[order[i]];
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
        buffer_insert(&doc->buffer, pos, &c, 1);
        cur->col++;
    }
    doc->dirty = true;
}

void document_delete_char_multi(Document *doc) {
    int order[64];
    for (int i = 0; i < doc->cursor_count; i++) order[i] = i;
    for (int i = 1; i < doc->cursor_count; i++) {
        for (int j = i; j > 0; j--) {
            size_t pa = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j]].row, doc->cursors[order[j]].col);
            size_t pb = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j-1]].row, doc->cursors[order[j-1]].col);
            if (pa < pb) { int tmp = order[j]; order[j] = order[j-1]; order[j-1] = tmp; }
            else break;
        }
    }
    for (int i = doc->cursor_count - 1; i >= 0; i--) {
        Cursor *cur = &doc->cursors[order[i]];
        if (cur->row == 0 && cur->col == 0) continue;
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
        if (pos == 0) continue;
        buffer_delete(&doc->buffer, pos - 1, 1);
        if (cur->col > 0) cur->col--;
        else {
            cur->row--;
            int len = (int)buffer_line_len(&doc->buffer, cur->row);
            cur->col = len > 0 ? len - 1 : 0;
        }
    }
    doc->dirty = true;
}

void document_newline_multi(Document *doc) {
    int order[64];
    for (int i = 0; i < doc->cursor_count; i++) order[i] = i;
    for (int i = 1; i < doc->cursor_count; i++) {
        for (int j = i; j > 0; j--) {
            size_t pa = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j]].row, doc->cursors[order[j]].col);
            size_t pb = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j-1]].row, doc->cursors[order[j-1]].col);
            if (pa < pb) { int tmp = order[j]; order[j] = order[j-1]; order[j-1] = tmp; }
            else break;
        }
    }
    for (int i = doc->cursor_count - 1; i >= 0; i--) {
        Cursor *cur = &doc->cursors[order[i]];
        char nl = '\n';
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
        buffer_insert(&doc->buffer, pos, &nl, 1);
        cur->row++;
        cur->col = 0;
    }
    doc->dirty = true;
}

void document_replace_char(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= doc->buffer.len) return;
    history_push_delete(&doc->history, pos, &doc->buffer.text[pos], 1, cur->row, cur->col);
    doc->buffer.text[pos] = c;
    history_push_insert(&doc->history, pos, &c, 1, cur->row, cur->col);
    doc->dirty = true;
}

void document_open_line_below(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cur->col = (int)buffer_line_len(&doc->buffer, cur->row);
    char nl = '\n';
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, &nl, 1);
    history_push_insert(&doc->history, pos, &nl, 1, cur->row, cur->col);
    cur->row++;
    cur->col = 0;

    if (cur->row > 0) {
        const char *prev = buffer_line_ptr(&doc->buffer, cur->row - 1);
        int indent = 0;
        while (prev[indent] == ' ' || prev[indent] == '\t') indent++;
        if (indent > 0) {
            char *spaces = malloc(indent);
            memset(spaces, ' ', indent);
            size_t ipos = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
            buffer_insert(&doc->buffer, ipos, spaces, indent);
            history_push_insert(&doc->history, ipos, spaces, indent, cur->row, 0);
            cur->col = indent;
            free(spaces);
        }
    }
    doc->dirty = true;
}

void document_open_line_above(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cur->col = 0;
    char nl = '\n';
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
    buffer_insert(&doc->buffer, pos, &nl, 1);
    history_push_insert(&doc->history, pos, &nl, 1, cur->row, 0);

    if (cur->row > 0) {
        const char *prev = buffer_line_ptr(&doc->buffer, cur->row - 1);
        int indent = 0;
        while (prev[indent] == ' ' || prev[indent] == '\t') indent++;
        if (indent > 0) {
            char *spaces = malloc(indent);
            memset(spaces, ' ', indent);
            buffer_insert(&doc->buffer, pos, spaces, indent);
            history_push_insert(&doc->history, pos, spaces, indent, cur->row, 0);
            cur->col = indent;
            free(spaces);
        }
    }
    doc->dirty = true;
}

void document_cursor_first_non_blank(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    int i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t'))
        i++;
    cur->col = i < len ? i : (len > 0 ? len - 1 : 0);
}

void document_join_lines(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    size_t lines = buffer_line_count(&doc->buffer);
    if ((size_t)cur->row + 1 >= lines) return;

    int cur_len = (int)buffer_line_len(&doc->buffer, cur->row);
    size_t join_pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur_len);

    if (join_pos > 0 && doc->buffer.text[join_pos - 1] == '\n') {
        buffer_delete(&doc->buffer, join_pos - 1, 1);
        const char *next = buffer_line_ptr(&doc->buffer, cur->row + 1);
        int next_indent = 0;
        while (next[next_indent] == ' ' || next[next_indent] == '\t') next_indent++;
        if (next_indent > 0) {
            buffer_delete(&doc->buffer, join_pos - 1, next_indent);
        }
    }
    cur->col = cur_len > 0 ? cur_len - 1 : 0;
    doc->dirty = true;
}

void document_change_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        document_delete_char_at_cursor(doc);
        return;
    }
    document_delete_selection(doc);
}

void document_substitute_char(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    document_delete_char_at_cursor(doc);
    (void)cur;
}

void document_delete_char_at_cursor(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= doc->buffer.len) return;
    char deleted = doc->buffer.text[pos];
    buffer_delete(&doc->buffer, pos, 1);
    history_push_delete(&doc->history, pos, &deleted, 1, cur->row, cur->col);
    size_t max_col = buffer_line_len(&doc->buffer, cur->row);
    if (max_col > 0 && cur->col >= (int)max_col)
        cur->col = (int)max_col - 1;
    if (cur->col < 0) cur->col = 0;
    doc->dirty = true;
}

void document_indent_line(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    char tab = '\t';
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
    buffer_insert(&doc->buffer, pos, &tab, 1);
    history_push_insert(&doc->history, pos, &tab, 1, cur->row, 0);
    cur->col++;
    doc->dirty = true;
}

void document_dedent_line(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    if (line[0] == '\t') {
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
        buffer_delete(&doc->buffer, pos, 1);
        history_push_delete(&doc->history, pos, "\t", 1, cur->row, 0);
        if (cur->col > 0) cur->col--;
        doc->dirty = true;
    } else if (line[0] == ' ') {
        int spaces = 0;
        while (line[spaces] == ' ' && spaces < 4) spaces++;
        if (spaces > 0) {
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
            buffer_delete(&doc->buffer, pos, spaces);
            history_push_delete(&doc->history, pos, line, spaces, cur->row, 0);
            cur->col -= spaces;
            if (cur->col < 0) cur->col = 0;
            doc->dirty = true;
        }
    }
}

void document_yank_line(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    free(doc->clipboard);
    doc->clipboard = NULL;
    doc->clipboard_len = 0;

    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    doc->clipboard = malloc(len + 1);
    memcpy(doc->clipboard, line, len);
    doc->clipboard[len] = '\n';
    doc->clipboard_len = len + 1;
}

void document_replace_selection_char(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        document_replace_char(doc, c);
        return;
    }
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    size_t len = end - start;
    char *deleted = malloc(len);
    memcpy(deleted, doc->buffer.text + start, len);
    buffer_delete(&doc->buffer, start, len);
    history_push_delete(&doc->history, start, deleted, len, sr, sc);
    free(deleted);

    char *replaced = malloc(len);
    memset(replaced, c, len);
    buffer_insert(&doc->buffer, start, replaced, len);
    history_push_insert(&doc->history, start, replaced, len, sr, sc);
    free(replaced);

    cursor_move_to(cur, sr, sc);
    cursor_clear_selection(cur);
    doc->dirty = true;
}

void document_replace_selection_yanked(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection || !doc->clipboard || doc->clipboard_len == 0) return;

    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    size_t old_len = end - start;

    char *deleted = malloc(old_len);
    memcpy(deleted, doc->buffer.text + start, old_len);
    buffer_delete(&doc->buffer, start, old_len);
    history_push_delete(&doc->history, start, deleted, old_len, sr, sc);
    free(deleted);

    buffer_insert(&doc->buffer, start, doc->clipboard, doc->clipboard_len);
    history_push_insert(&doc->history, start, doc->clipboard, doc->clipboard_len, sr, sc);

    cursor_move_to(cur, sr, sc);
    cursor_clear_selection(cur);
    doc->dirty = true;
}

static void do_toggle_case(Document *doc, int sr, int sc, int er, int ec) {
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    for (size_t i = start; i < end && i < doc->buffer.len; i++) {
        char c = doc->buffer.text[i];
        if (c >= 'a' && c <= 'z') doc->buffer.text[i] = c - 32;
        else if (c >= 'A' && c <= 'Z') doc->buffer.text[i] = c + 32;
    }
}

static void do_set_case(Document *doc, int sr, int sc, int er, int ec, char offset) {
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    for (size_t i = start; i < end && i < doc->buffer.len; i++) {
        char c = doc->buffer.text[i];
        if (c >= 'a' && c <= 'z') doc->buffer.text[i] = c + offset;
        else if (c >= 'A' && c <= 'Z') doc->buffer.text[i] = c + offset;
    }
}

void document_toggle_case(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
        if (pos < doc->buffer.len) {
            char c = doc->buffer.text[pos];
            if (c >= 'a' && c <= 'z') doc->buffer.text[pos] = c - 32;
            else if (c >= 'A' && c <= 'Z') doc->buffer.text[pos] = c + 32;
        }
        doc->dirty = true;
        return;
    }
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    do_toggle_case(doc, sr, sc, er, ec);
    doc->dirty = true;
}

void document_lowercase(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    do_set_case(doc, sr, sc, er, ec, 32);
    doc->dirty = true;
}

void document_uppercase(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    do_set_case(doc, sr, sc, er, ec, -32);
    doc->dirty = true;
}

void document_indent_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        document_indent_line(doc);
        return;
    }
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    for (int r = sr; r <= er; r++) {
        char tab = '\t';
        size_t pos = buffer_pos_from_row_col(&doc->buffer, r, 0);
        buffer_insert(&doc->buffer, pos, &tab, 1);
        history_push_insert(&doc->history, pos, &tab, 1, r, 0);
    }
    doc->dirty = true;
}

void document_dedent_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        document_dedent_line(doc);
        return;
    }
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    for (int r = sr; r <= er; r++) {
        const char *line = buffer_line_ptr(&doc->buffer, r);
        if (line[0] == '\t') {
            size_t pos = buffer_pos_from_row_col(&doc->buffer, r, 0);
            buffer_delete(&doc->buffer, pos, 1);
            history_push_delete(&doc->history, pos, "\t", 1, r, 0);
        } else if (line[0] == ' ') {
            int spaces = 0;
            while (line[spaces] == ' ' && spaces < 4) spaces++;
            if (spaces > 0) {
                size_t pos = buffer_pos_from_row_col(&doc->buffer, r, 0);
                buffer_delete(&doc->buffer, pos, spaces);
            }
        }
    }
    doc->dirty = true;
}

void document_collapse_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    cursor_move_to(cur, er, ec);
    cursor_clear_selection(cur);
}

void document_keep_primary_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    cursor_move_to(cur, er, ec);
    cursor_clear_selection(cur);
}

void document_flip_cursor_anchor(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        cursor_select_start(cur);
        return;
    }
    int tmp_r = cur->row, tmp_c = cur->col;
    cursor_move_to(cur, cur->anchor_row, cur->anchor_col);
    cur->anchor_row = tmp_r;
    cur->anchor_col = tmp_c;
}

void document_copy_selection_below(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        document_add_cursor(doc);
        return;
    }
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);

    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    size_t len = end - start;
    if (len == 0) return;

    char *text = malloc(len);
    memcpy(text, doc->buffer.text + start, len);

    size_t insert_pos = buffer_pos_from_row_col(&doc->buffer, er + 1, 0);
    buffer_insert(&doc->buffer, insert_pos, text, len);
    history_push_insert(&doc->history, insert_pos, text, len, er + 1, 0);
    free(text);
    doc->dirty = true;
}

void document_join_lines_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        document_join_lines(doc);
        return;
    }
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);

    for (int r = er; r > sr; r--) {
        const char *line = buffer_line_ptr(&doc->buffer, r);
        int len = (int)buffer_line_len(&doc->buffer, r);
        int prev_len = (int)buffer_line_len(&doc->buffer, r - 1);

        size_t join_pos = buffer_pos_from_row_col(&doc->buffer, r - 1, prev_len);
        if (join_pos > 0 && doc->buffer.text[join_pos - 1] == '\n') {
            buffer_delete(&doc->buffer, join_pos - 1, 1);
            int indent = 0;
            while (line[indent] == ' ' || line[indent] == '\t') indent++;
            if (indent > 0 && len > 0) {
                buffer_delete(&doc->buffer, join_pos - 1, indent);
            }
        }
    }
    doc->dirty = true;
}

void document_find_char_forward(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    for (size_t i = pos + 1; i < doc->buffer.len; i++) {
        if (doc->buffer.text[i] == c) {
            int row, col;
            buffer_row_col_from_pos(&doc->buffer, i, &row, &col);
            cursor_move_to(cur, row, col);
            return;
        }
    }
}

void document_find_char_backward(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos == 0) return;
    for (size_t i = pos - 1; ; i--) {
        if (doc->buffer.text[i] == c) {
            int row, col;
            buffer_row_col_from_pos(&doc->buffer, i, &row, &col);
            cursor_move_to(cur, row, col);
            return;
        }
        if (i == 0) break;
    }
}

void document_till_char_forward(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    for (size_t i = pos + 1; i < doc->buffer.len; i++) {
        if (doc->buffer.text[i] == c) {
            size_t target = i > 0 ? i - 1 : 0;
            int row, col;
            buffer_row_col_from_pos(&doc->buffer, target, &row, &col);
            cursor_move_to(cur, row, col);
            return;
        }
    }
}

void document_till_char_backward(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos == 0) return;
    for (size_t i = pos - 1; ; i--) {
        if (doc->buffer.text[i] == c) {
            size_t target = i + 1 < doc->buffer.len ? i + 1 : i;
            int row, col;
            buffer_row_col_from_pos(&doc->buffer, target, &row, &col);
            cursor_move_to(cur, row, col);
            return;
        }
        if (i == 0) break;
    }
}

void document_scroll_center(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    doc->scroll_y = cur->row - doc->viewport_lines / 2;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
}

void document_scroll_horizontal_center(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    /* Center horizontally - assume average viewport width of 80 chars */
    int viewport_width = 80;
    doc->scroll_x = cur->col - viewport_width / 2;
    if (doc->scroll_x < 0) doc->scroll_x = 0;
}

void document_scroll_top(Document *doc, int viewport_h) {
    (void)viewport_h;
    Cursor *cur = &doc->cursors[0];
    doc->scroll_y = cur->row;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
}

void document_scroll_bottom(Document *doc, int viewport_h) {
    Cursor *cur = &doc->cursors[0];
    doc->scroll_y = cur->row - viewport_h + 2;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
}

void document_set_search(Document *doc, const char *query, size_t len) {
    free(doc->search_query);
    doc->search_query = malloc(len + 1);
    memcpy(doc->search_query, query, len);
    doc->search_query[len] = '\0';
    doc->search_len = len;
}

void document_search_next(Document *doc) {
    if (!doc->search_query || doc->search_len == 0) return;
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos + 1 < doc->buffer.len) {
        const char *found = memmem(doc->buffer.text + pos + 1,
                                   doc->buffer.len - pos - 1,
                                   doc->search_query, doc->search_len);
        if (found) {
            size_t fpos = (size_t)(found - doc->buffer.text);
            int row, col;
            buffer_row_col_from_pos(&doc->buffer, fpos, &row, &col);
            cursor_move_to(cur, row, col);
            return;
        }
    }
    const char *found = memmem(doc->buffer.text, doc->buffer.len,
                               doc->search_query, doc->search_len);
    if (found) {
        size_t fpos = (size_t)(found - doc->buffer.text);
        int row, col;
        buffer_row_col_from_pos(&doc->buffer, fpos, &row, &col);
        cursor_move_to(cur, row, col);
    }
}

void document_search_prev(Document *doc) {
    if (!doc->search_query || doc->search_len == 0) return;
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    size_t best = 0;
    bool found = false;
    for (size_t i = 0; i < pos; i++) {
        if (i + doc->search_len <= pos &&
            memcmp(doc->buffer.text + i, doc->search_query, doc->search_len) == 0) {
            best = i;
            found = true;
        }
    }
    if (!found) {
        for (size_t i = (pos > 0 ? pos - 1 : 0); i < doc->buffer.len; i++) {
            if (i + doc->search_len <= doc->buffer.len &&
                memcmp(doc->buffer.text + i, doc->search_query, doc->search_len) == 0) {
                best = i;
                found = true;
                break;
            }
        }
    }
    if (found) {
        int row, col;
        buffer_row_col_from_pos(&doc->buffer, best, &row, &col);
        cursor_move_to(cur, row, col);
    }
}

void document_extend_to_line_bounds(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    cur->col = len > 0 ? len - 1 : 0;
}

void document_shrink_to_line_bounds(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    cursor_move_to(cur, er, ec);
    cursor_clear_selection(cur);
}

void document_remove_primary_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cursor_clear_selection(cur);
}

void document_goto_line_start(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cursor_move_to(cur, cur->row, 0);
}

void document_goto_line_end(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    cursor_move_to(cur, cur->row, len > 0 ? len - 1 : 0);
}

void document_goto_view_top(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cursor_move_to(cur, doc->scroll_y, cur->col);
}

void document_goto_view_center(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cursor_move_to(cur, doc->scroll_y + doc->viewport_lines / 2, cur->col);
}

void document_goto_view_bottom(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    int bottom = doc->scroll_y + doc->viewport_lines - 1;
    if (bottom < 0) bottom = 0;
    cursor_move_to(cur, bottom, cur->col);
}

void document_paste_before(Document *doc) {
    if (!doc->clipboard || doc->clipboard_len == 0) return;
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos > 0 && doc->clipboard_len == 1 && doc->clipboard[0] != '\n') pos--;
    buffer_insert(&doc->buffer, pos, doc->clipboard, doc->clipboard_len);
    history_push_insert(&doc->history, pos, doc->clipboard, doc->clipboard_len,
                        cur->row, cur->col);
    if (doc->clipboard_len == 1 && doc->clipboard[0] != '\n') {
        cur->col++;
    } else {
        for (size_t i = 0; i < doc->clipboard_len; i++) {
            if (doc->clipboard[i] == '\n') { cur->row++; cur->col = 0; }
            else cur->col++;
        }
    }
    doc->dirty = true;
}

void document_search_word(Document *doc, bool word_boundary) {
    Cursor *cur = &doc->cursors[0];
    if (cur->has_selection) {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
        size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
        size_t len = end - start;
        if (len > 0 && len < 256) {
            char pat[256];
            memcpy(pat, doc->buffer.text + start, len);
            pat[len] = '\0';
            document_set_search(doc, pat, len);
        }
    } else {
        const char *line = buffer_line_ptr(&doc->buffer, cur->row);
        int col = cur->col;
        int len = (int)buffer_line_len(&doc->buffer, cur->row);
        int start = col;
        int end = col;
        if (word_boundary) {
            while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t'
                   && line[start - 1] != '\n' && line[start - 1] != '('
                   && line[start - 1] != ')' && line[start - 1] != '{'
                   && line[start - 1] != '}' && line[start - 1] != '['
                   && line[start - 1] != ']' && line[start - 1] != ','
                   && line[start - 1] != ';')
                start--;
            while (end < len && line[end] != ' ' && line[end] != '\t'
                   && line[end] != '\n' && line[end] != '('
                   && line[end] != ')' && line[end] != '{'
                   && line[end] != '}' && line[end] != '['
                   && line[end] != ']' && line[end] != ','
                   && line[end] != ';')
                end++;
        } else {
            while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t'
                   && line[start - 1] != '\n')
                start--;
            while (end < len && line[end] != ' ' && line[end] != '\t'
                   && line[end] != '\n')
                end++;
        }
        int slen = end - start;
        if (slen > 0) {
            document_set_search(doc, line + start, slen);
            cursor_move_to(cur, cur->row, start);
        }
    }
    document_search_next(doc);
}

void document_half_page_down(Document *doc, int viewport_h) {
    int half = viewport_h / 2;
    doc->scroll_y += half;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
    document_move_cursor(doc, half, 0);
}

void document_half_page_up(Document *doc, int viewport_h) {
    int half = viewport_h / 2;
    doc->scroll_y -= half;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
    document_move_cursor(doc, -half, 0);
}

void document_force_selection_forward(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (cur->has_selection) {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        cursor_move_to(cur, sr, sc);
        cursor_select_start(cur);
        cursor_move_to(cur, er, ec);
    }
}

void document_rotate_selections_backward(Document *doc) {
    (void)doc;
}

void document_rotate_selections_forward(Document *doc) {
    (void)doc;
}

void document_rotate_selection_contents_backward(Document *doc) {
    if (doc->cursor_count < 2) return;
    
    /* Extract text from each selection */
    char *contents[64];
    int count = 0;
    
    for (int i = 0; i < doc->cursor_count && i < 64; i++) {
        Cursor *cur = &doc->cursors[i];
        if (!cur->has_selection) continue;
        
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        
        /* Calculate total length needed */
        size_t total_len = 0;
        for (int r = sr; r <= er; r++) {
            size_t line_len = buffer_line_len(&doc->buffer, r);
            int start = (r == sr) ? sc : 0;
            int end = (r == er) ? ec : (int)line_len;
            if (end > start) total_len += (end - start);
            if (r < er) total_len++; /* newline */
        }
        
        /* Extract text */
        char *text = malloc(total_len + 1);
        size_t pos = 0;
        for (int r = sr; r <= er; r++) {
            const char *line = buffer_line_ptr(&doc->buffer, r);
            size_t line_len = buffer_line_len(&doc->buffer, r);
            int start = (r == sr) ? sc : 0;
            int end = (r == er) ? ec : (int)line_len;
            if (end > start) {
                memcpy(text + pos, line + start, end - start);
                pos += (end - start);
            }
            if (r < er) {
                text[pos++] = '\n';
            }
        }
        text[pos] = '\0';
        
        contents[count++] = text;
    }
    
    if (count < 2) {
        for (int i = 0; i < count; i++) free(contents[i]);
        return;
    }
    
    /* Rotate contents backward (move first to end) */
    char *first = contents[0];
    for (int i = 0; i < count - 1; i++) {
        contents[i] = contents[i + 1];
    }
    contents[count - 1] = first;
    
    /* Replace selections with rotated content */
    int idx = 0;
    for (int i = 0; i < doc->cursor_count && idx < count; i++) {
        Cursor *cur = &doc->cursors[i];
        if (!cur->has_selection) continue;
        
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        
        /* Delete selection */
        size_t start_pos = buffer_pos_from_row_col(&doc->buffer, sr, sc);
        size_t end_pos = buffer_pos_from_row_col(&doc->buffer, er, ec);
        size_t del_len = end_pos - start_pos;
        
        buffer_delete(&doc->buffer, start_pos, del_len);
        buffer_insert(&doc->buffer, start_pos, contents[idx], strlen(contents[idx]));
        
        /* Update cursor position */
        buffer_row_col_from_pos(&doc->buffer, start_pos + strlen(contents[idx]), &cur->row, &cur->col);
        cur->has_selection = false;
        
        idx++;
    }
    
    for (int i = 0; i < count; i++) free(contents[i]);
    doc->dirty = true;
}

void document_rotate_selection_contents_forward(Document *doc) {
    if (doc->cursor_count < 2) return;
    
    /* Extract text from each selection */
    char *contents[64];
    int count = 0;
    
    for (int i = 0; i < doc->cursor_count && i < 64; i++) {
        Cursor *cur = &doc->cursors[i];
        if (!cur->has_selection) continue;
        
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        
        /* Calculate total length needed */
        size_t total_len = 0;
        for (int r = sr; r <= er; r++) {
            size_t line_len = buffer_line_len(&doc->buffer, r);
            int start = (r == sr) ? sc : 0;
            int end = (r == er) ? ec : (int)line_len;
            if (end > start) total_len += (end - start);
            if (r < er) total_len++; /* newline */
        }
        
        /* Extract text */
        char *text = malloc(total_len + 1);
        size_t pos = 0;
        for (int r = sr; r <= er; r++) {
            const char *line = buffer_line_ptr(&doc->buffer, r);
            size_t line_len = buffer_line_len(&doc->buffer, r);
            int start = (r == sr) ? sc : 0;
            int end = (r == er) ? ec : (int)line_len;
            if (end > start) {
                memcpy(text + pos, line + start, end - start);
                pos += (end - start);
            }
            if (r < er) {
                text[pos++] = '\n';
            }
        }
        text[pos] = '\0';
        
        contents[count++] = text;
    }
    
    if (count < 2) {
        for (int i = 0; i < count; i++) free(contents[i]);
        return;
    }
    
    /* Rotate contents forward (move last to beginning) */
    char *last = contents[count - 1];
    for (int i = count - 1; i > 0; i--) {
        contents[i] = contents[i - 1];
    }
    contents[0] = last;
    
    /* Replace selections with rotated content */
    int idx = 0;
    for (int i = 0; i < doc->cursor_count && idx < count; i++) {
        Cursor *cur = &doc->cursors[i];
        if (!cur->has_selection) continue;
        
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        
        /* Delete selection */
        size_t start_pos = buffer_pos_from_row_col(&doc->buffer, sr, sc);
        size_t end_pos = buffer_pos_from_row_col(&doc->buffer, er, ec);
        size_t del_len = end_pos - start_pos;
        
        buffer_delete(&doc->buffer, start_pos, del_len);
        buffer_insert(&doc->buffer, start_pos, contents[idx], strlen(contents[idx]));
        
        /* Update cursor position */
        buffer_row_col_from_pos(&doc->buffer, start_pos + strlen(contents[idx]), &cur->row, &cur->col);
        cur->has_selection = false;
        
        idx++;
    }
    
    for (int i = 0; i < count; i++) free(contents[i]);
    doc->dirty = true;
}

void document_delete_word_forward(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int col = cur->col;
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    if (col < len) {
        int end = col + 1;
        while (end < len && line[end] != ' ' && line[end] != '\t'
               && line[end] != '\n')
            end++;
        int del = end - col;
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, col);
        buffer_delete(&doc->buffer, pos, del);
        doc->dirty = true;
    }
}

void document_split_selection_newlines(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    for (size_t i = start; i < end; i++) {
        if (doc->buffer.text[i] == '\n') {
            int row, col;
            buffer_row_col_from_pos(&doc->buffer, i + 1, &row, &col);
            cursor_move_to(cur, row, col);
            return;
        }
    }
}

void document_merge_selections(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cursor_clear_selection(cur);
}

void document_merge_consecutive_selections(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    cursor_clear_selection(cur);
}

void document_trim_whitespace(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        const char *line = buffer_line_ptr(&doc->buffer, cur->row);
        int len = (int)buffer_line_len(&doc->buffer, cur->row);
        int end = len;
        while (end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\t'))
            end--;
        if (end < len) {
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, end);
            buffer_delete(&doc->buffer, pos, len - end);
            doc->dirty = true;
        }
    } else {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        for (int r = sr; r <= er; r++) {
            const char *line = buffer_line_ptr(&doc->buffer, r);
            int len = (int)buffer_line_len(&doc->buffer, r);
            int end = len;
            while (end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\t'))
                end--;
            if (end < len) {
                size_t pos = buffer_pos_from_row_col(&doc->buffer, r, end);
                buffer_delete(&doc->buffer, pos, len - end);
            }
        }
        cursor_clear_selection(cur);
        doc->dirty = true;
    }
}

void document_copy_selection_above(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    size_t len = end - start;
    if (len == 0) return;
    char *text = malloc(len + 1);
    memcpy(text, doc->buffer.text + start, len);
    text[len] = '\0';
    size_t insert_pos = buffer_pos_from_row_col(&doc->buffer, sr, 0);
    buffer_insert(&doc->buffer, insert_pos, text, len);
    int new_row = sr + 1;
    cursor_move_to(cur, new_row, sc);
    cur->anchor_row = new_row;
    cur->anchor_col = ec;
    cur->has_selection = true;
    free(text);
    doc->dirty = true;
}

void document_join_lines_with_space(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        size_t len = buffer_line_len(&doc->buffer, cur->row);
        size_t join_pos = buffer_pos_from_row_col(&doc->buffer, cur->row, len);
        if (join_pos < doc->buffer.len && doc->buffer.text[join_pos] == '\n') {
            doc->buffer.text[join_pos] = ' ';
            doc->dirty = true;
        }
    } else {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        for (int r = er; r > sr; r--) {
            size_t len = buffer_line_len(&doc->buffer, r - 1);
            size_t join_pos = buffer_pos_from_row_col(&doc->buffer, r - 1, len);
            if (join_pos < doc->buffer.len && doc->buffer.text[join_pos] == '\n') {
                doc->buffer.text[join_pos] = ' ';
            }
        }
        int new_end_col = (int)buffer_line_len(&doc->buffer, sr);
        cursor_move_to(cur, sr, sc);
        cur->anchor_col = new_end_col;
        cur->has_selection = true;
        doc->dirty = true;
    }
}

void document_comment_toggle(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    int first_nonblank = 0;
    while (first_nonblank < len && (line[first_nonblank] == ' ' || line[first_nonblank] == '\t'))
        first_nonblank++;
    if (first_nonblank + 1 < len && line[first_nonblank] == '/' && line[first_nonblank + 1] == '/') {
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, first_nonblank);
        buffer_delete(&doc->buffer, pos, 2);
        cur->col = first_nonblank;
    } else {
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, first_nonblank);
        buffer_insert(&doc->buffer, pos, "//", 2);
        cur->col = first_nonblank + 2;
    }
    doc->dirty = true;
}

void document_format_selection(Document *doc) {
    (void)doc;
}

void document_page_down_extend(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    document_move_cursor(doc, doc->viewport_lines, 0);
}

void document_page_up_extend(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    document_move_cursor(doc, -doc->viewport_lines, 0);
}

void document_half_page_down_extend(Document *doc, int viewport_h) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    int half = viewport_h / 2;
    doc->scroll_y += half;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
    document_move_cursor(doc, half, 0);
}

void document_half_page_up_extend(Document *doc, int viewport_h) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    int half = viewport_h / 2;
    doc->scroll_y -= half;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
    document_move_cursor(doc, -half, 0);
}

void document_select_regex(Document *doc, const char *pattern, size_t len) {
    (void)doc; (void)pattern; (void)len;
}

void document_split_regex(Document *doc, const char *pattern, size_t len) {
    (void)doc; (void)pattern; (void)len;
}

void document_new(Document *doc) {
    document_free(doc);
    buffer_init(&doc->buffer);
    cursor_init(&doc->cursors[0]);
    doc->cursor_count = 1;
    doc->scroll_y = 0;
    doc->dirty = false;
}

void document_sort_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, 0);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, (int)buffer_line_len(&doc->buffer, er));
    if (end > start) {
        qsort(doc->buffer.text + start, end - start, 1,
              (int(*)(const void*, const void*))strcmp);
    }
    doc->dirty = true;
}

void document_cursor_WORD_forward(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    if (cur->col >= len) {
        if ((size_t)cur->row + 1 < buffer_line_count(&doc->buffer)) {
            cur->row++;
            cur->col = 0;
        }
        return;
    }
    int i = cur->col;
    while (i < len && (line[i] == ' ' || line[i] == '\t'))
        i++;
    while (i < len && line[i] != ' ' && line[i] != '\t')
        i++;
    cur->col = i < len ? i : (len > 0 ? len - 1 : 0);
}

void document_cursor_WORD_backward(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int i = cur->col;
    if (i > 0) i--;
    while (i > 0 && (line[i] == ' ' || line[i] == '\t'))
        i--;
    while (i > 0 && line[i - 1] != ' ' && line[i - 1] != '\t')
        i--;
    cur->col = i;
}

void document_cursor_WORD_end(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    int i = cur->col;
    if (i < len - 1) i++;
    while (i < len - 1 && (line[i] == ' ' || line[i] == '\t'))
        i++;
    while (i < len - 1 && line[i + 1] != ' ' && line[i + 1] != '\t')
        i++;
    if (i < len && line[i] != '\n')
        cur->col = i;
}

void document_increment_number(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    int col = cur->col;
    int start = col;
    int end = col;
    if (start > 0 && (line[start - 1] == '-' || line[start - 1] == '+'))
        start--;
    else if (col < len && (line[col] == '-' || line[col] == '+'))
        col++;
    while (start > 0 && line[start - 1] >= '0' && line[start - 1] <= '9')
        start--;
    end = col;
    while (end < len && line[end] >= '0' && line[end] <= '9')
        end++;
    if (end > start) {
        char num_str[32];
        int num_len = end - start;
        if (num_len < 31) {
            memcpy(num_str, line + start, num_len);
            num_str[num_len] = '\0';
            long val = strtol(num_str, NULL, 10);
            val++;
            char new_str[32];
            int new_len = snprintf(new_str, sizeof(new_str), "%ld", val);
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, start);
            buffer_delete(&doc->buffer, pos, num_len);
            buffer_insert(&doc->buffer, pos, new_str, new_len);
            cur->col = start + new_len;
            doc->dirty = true;
        }
    }
}

void document_decrement_number(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    int col = cur->col;
    int start = col;
    int end = col;
    if (start > 0 && (line[start - 1] == '-' || line[start - 1] == '+'))
        start--;
    else if (col < len && (line[col] == '-' || line[col] == '+'))
        col++;
    while (start > 0 && line[start - 1] >= '0' && line[start - 1] <= '9')
        start--;
    end = col;
    while (end < len && line[end] >= '0' && line[end] <= '9')
        end++;
    if (end > start) {
        char num_str[32];
        int num_len = end - start;
        if (num_len < 31) {
            memcpy(num_str, line + start, num_len);
            num_str[num_len] = '\0';
            long val = strtol(num_str, NULL, 10);
            val--;
            char new_str[32];
            int new_len = snprintf(new_str, sizeof(new_str), "%ld", val);
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, start);
            buffer_delete(&doc->buffer, pos, num_len);
            buffer_insert(&doc->buffer, pos, new_str, new_len);
            cur->col = start + new_len;
            doc->dirty = true;
        }
    }
}

void document_surround(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    char open_c = c, close_c = c;
    if (c == '(') { open_c = '('; close_c = ')'; }
    else if (c == '{') { open_c = '{'; close_c = '}'; }
    else if (c == '[') { open_c = '['; close_c = ']'; }
    else if (c == '<') { open_c = '<'; close_c = '>'; }
    size_t end_pos = buffer_pos_from_row_col(&doc->buffer, er, ec);
    buffer_insert(&doc->buffer, end_pos, &close_c, 1);
    size_t start_pos = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    buffer_insert(&doc->buffer, start_pos, &open_c, 1);
    cursor_move_to(cur, sr, sc + 1);
    cur->anchor_row = er;
    cur->anchor_col = ec + 1;
    cur->has_selection = true;
    doc->dirty = true;
}

void document_delete_surround(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    char open_c = c, close_c = c;
    if (c == '(') { open_c = '('; close_c = ')'; }
    else if (c == '{') { open_c = '{'; close_c = '}'; }
    else if (c == '[') { open_c = '['; close_c = ']'; }
    else if (c == '<') { open_c = '<'; close_c = '>'; }
    size_t end_pos = buffer_pos_from_row_col(&doc->buffer, er, ec);
    if (end_pos < doc->buffer.len && doc->buffer.text[end_pos] == close_c)
        buffer_delete(&doc->buffer, end_pos, 1);
    size_t start_pos = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    if (start_pos > 0 && doc->buffer.text[start_pos - 1] == open_c) {
        buffer_delete(&doc->buffer, start_pos - 1, 1);
        cursor_move_to(cur, sr, sc - 1);
    }
    doc->dirty = true;
}

void document_align_selections(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    int max_width = 0;
    for (int r = sr; r <= er; r++) {
        const char *line = buffer_line_ptr(&doc->buffer, r);
        int len = (int)buffer_line_len(&doc->buffer, r);
        int ws = 0;
        while (ws < len && (line[ws] == ' ' || line[ws] == '\t'))
            ws++;
        int word_len = len - ws;
        if (word_len > 0 && ws + word_len > max_width)
            max_width = ws + word_len;
    }
    for (int r = sr; r <= er; r++) {
        const char *line = buffer_line_ptr(&doc->buffer, r);
        int len = (int)buffer_line_len(&doc->buffer, r);
        int ws = 0;
        while (ws < len && (line[ws] == ' ' || line[ws] == '\t'))
            ws++;
        int word_len = len - ws;
        if (word_len > 0 && ws < max_width) {
            int padding = max_width - ws - word_len;
            if (padding > 0) {
                char *pad = malloc(padding);
                memset(pad, ' ', padding);
                size_t pos = buffer_pos_from_row_col(&doc->buffer, r, ws + word_len);
                buffer_insert(&doc->buffer, pos, pad, padding);
                free(pad);
            }
        }
    }
    doc->dirty = true;
}

void document_jumplist_push(Document *doc, int row, int col) {
    if (doc->jumplist_len < 256) {
        doc->jumplist[doc->jumplist_len][0] = row;
        doc->jumplist[doc->jumplist_len][1] = col;
        doc->jumplist_len++;
    }
    doc->jumplist_pos = doc->jumplist_len;
}

void document_jumplist_backward(Document *doc) {
    if (doc->jumplist_pos > 0) {
        doc->jumplist_pos--;
        Cursor *cur = &doc->cursors[0];
        cursor_move_to(cur, doc->jumplist[doc->jumplist_pos][0],
                       doc->jumplist[doc->jumplist_pos][1]);
    }
}

void document_jumplist_forward(Document *doc) {
    if (doc->jumplist_pos < doc->jumplist_len - 1) {
        doc->jumplist_pos++;
        Cursor *cur = &doc->cursors[0];
        cursor_move_to(cur, doc->jumplist[doc->jumplist_pos][0],
                       doc->jumplist[doc->jumplist_pos][1]);
    }
}

void document_select_literal(Document *doc, const char *pattern, size_t len) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection || len == 0) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t sel_start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t sel_end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    const char *found = memmem(doc->buffer.text + sel_start, sel_end - sel_start, pattern, len);
    if (found) {
        size_t fpos = (size_t)(found - doc->buffer.text);
        int row, col;
        buffer_row_col_from_pos(&doc->buffer, fpos, &row, &col);
        cursor_move_to(cur, row, col);
        cur->anchor_row = row;
        cur->anchor_col = col + (int)len;
        cur->has_selection = true;
    }
}

void document_select_all_matches(Document *doc, const char *pattern, size_t len) {
    if (len == 0) return;
    Cursor *cur = &doc->cursors[0];
    int sr = 0, sc = 0, er = (int)buffer_line_count(&doc->buffer) - 1, ec = 0;
    if (cur->has_selection) {
        cursor_normalize(cur, &sr, &sc, &er, &ec);
    }
    size_t sel_start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t sel_end = buffer_pos_from_row_col(&doc->buffer, er, ec);

    /* Clear existing selections, keep only first cursor */
    cur->has_selection = false;
    doc->cursor_count = 1;

    /* Find all matches within selection range and create cursors */
    const char *base = doc->buffer.text + sel_start;
    size_t range = sel_end - sel_start;
    size_t offset = 0;
    int first = 1;
    while (offset < range) {
        const char *found = memmem(base + offset, range - offset, pattern, len);
        if (!found) break;
        size_t fpos = sel_start + offset + (size_t)(found - (base + offset));
        int row, col;
        buffer_row_col_from_pos(&doc->buffer, fpos, &row, &col);
        if (first) {
            cursor_move_to(cur, row, col);
            cur->anchor_row = row;
            cur->anchor_col = col + (int)len;
            cur->has_selection = true;
            first = 0;
        } else if (doc->cursor_count < MAX_CURSORS) {
            Cursor *nc = &doc->cursors[doc->cursor_count++];
            cursor_init(nc);
            cursor_move_to(nc, row, col);
            nc->anchor_row = row;
            nc->anchor_col = col + (int)len;
            nc->has_selection = true;
        }
        offset = fpos - sel_start + len;
    }
}

void document_split_literal(Document *doc, const char *pattern, size_t len) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection || len == 0) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t sel_start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t sel_end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    const char *found = memmem(doc->buffer.text + sel_start, sel_end - sel_start, pattern, len);
    if (found) {
        size_t fpos = (size_t)(found - doc->buffer.text);
        int row, col;
        buffer_row_col_from_pos(&doc->buffer, fpos + len, &row, &col);
        cursor_move_to(cur, row, col);
        cur->anchor_row = row;
        cur->anchor_col = col;
        cur->has_selection = false;
    }
}

void document_split_all_matches(Document *doc, const char *pattern, size_t len) {
    if (len == 0) return;
    Cursor *cur = &doc->cursors[0];
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t sel_start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t sel_end = buffer_pos_from_row_col(&doc->buffer, er, ec);

    /* Split: keep text between matches as separate selections */
    doc->cursor_count = 1;
    cur->has_selection = false;
    size_t offset = sel_start;
    int first = 1;
    while (offset < sel_end) {
        const char *found = memmem(doc->buffer.text + offset, sel_end - offset, pattern, len);
        size_t match_end = found ? (size_t)(found - doc->buffer.text) + len : sel_end;
        if (match_end > offset) {
            int row1, col1, row2, col2;
            buffer_row_col_from_pos(&doc->buffer, offset, &row1, &col1);
            buffer_row_col_from_pos(&doc->buffer, match_end - (found ? len : 0), &row2, &col2);
            if (first) {
                cursor_move_to(cur, row1, col1);
                cur->anchor_row = row2;
                cur->anchor_col = col2;
                cur->has_selection = (offset < match_end - (found ? len : 0));
                first = 0;
            } else if (doc->cursor_count < MAX_CURSORS) {
                Cursor *nc = &doc->cursors[doc->cursor_count++];
                cursor_init(nc);
                cursor_move_to(nc, row1, col1);
                nc->anchor_row = row2;
                nc->anchor_col = col2;
                nc->has_selection = (offset < match_end - (found ? len : 0));
            }
        }
        if (!found) break;
        offset = (size_t)(found - doc->buffer.text) + len;
    }
}

void document_keep_matching(Document *doc, const char *pattern, size_t len) {
    if (len == 0) return;
    Cursor *cur = &doc->cursors[0];
    int sr = 0, sc = 0, er = (int)buffer_line_count(&doc->buffer) - 1, ec = 0;
    if (cur->has_selection) {
        cursor_normalize(cur, &sr, &sc, &er, &ec);
    }
    size_t sel_start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t sel_end = buffer_pos_from_row_col(&doc->buffer, er, ec);

    doc->cursor_count = 1;
    cur->has_selection = false;
    const char *base = doc->buffer.text + sel_start;
    size_t range = sel_end - sel_start;
    size_t offset = 0;
    int first = 1;
    while (offset < range) {
        const char *found = memmem(base + offset, range - offset, pattern, len);
        if (!found) break;
        size_t fpos = sel_start + offset + (size_t)(found - (base + offset));
        int row, col;
        buffer_row_col_from_pos(&doc->buffer, fpos, &row, &col);
        if (first) {
            cursor_move_to(cur, row, col);
            cur->anchor_row = row;
            cur->anchor_col = col + (int)len;
            cur->has_selection = true;
            first = 0;
        } else if (doc->cursor_count < MAX_CURSORS) {
            Cursor *nc = &doc->cursors[doc->cursor_count++];
            cursor_init(nc);
            cursor_move_to(nc, row, col);
            nc->anchor_row = row;
            nc->anchor_col = col + (int)len;
            nc->has_selection = true;
        }
        offset = fpos - sel_start + len;
    }
}

void document_remove_matching(Document *doc, const char *pattern, size_t len) {
    (void)doc; (void)pattern; (void)len;
}

void document_replace_surround(Document *doc, char from, char to) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t end_pos = buffer_pos_from_row_col(&doc->buffer, er, ec);
    if (end_pos < doc->buffer.len && doc->buffer.text[end_pos] == from)
        doc->buffer.text[end_pos] = to;
    size_t start_pos = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    if (start_pos > 0 && doc->buffer.text[start_pos - 1] == from)
        doc->buffer.text[start_pos - 1] = to;
    doc->dirty = true;
}

void document_go_to_file(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    int col = cur->col;
    int start = col;
    int end = col;
    while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t'
           && line[start - 1] != '"' && line[start - 1] != '\'')
        start--;
    while (end < len && line[end] != ' ' && line[end] != '\t'
           && line[end] != '"' && line[end] != '\'' && line[end] != '\n')
        end++;
    if (end > start) {
        int path_len = end - start;
        char *path = malloc(path_len + 1);
        memcpy(path, line + start, path_len);
        path[path_len] = '\0';
        document_open(doc, path);
        free(path);
    }
}

void document_view_page_down(Document *doc) {
    doc->scroll_y += doc->viewport_lines;
    int total = (int)buffer_line_count(&doc->buffer);
    if (doc->scroll_y > total - doc->viewport_lines)
        doc->scroll_y = total - doc->viewport_lines;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
}

void document_view_page_up(Document *doc) {
    doc->scroll_y -= doc->viewport_lines;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
}

void document_view_half_page_down(Document *doc) {
    doc->scroll_y += doc->viewport_lines / 2;
    int total = (int)buffer_line_count(&doc->buffer);
    if (doc->scroll_y > total - doc->viewport_lines)
        doc->scroll_y = total - doc->viewport_lines;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
}

void document_view_half_page_up(Document *doc) {
    doc->scroll_y -= doc->viewport_lines / 2;
    if (doc->scroll_y < 0) doc->scroll_y = 0;
}

static int de_is_open_bracket(char c)  { return c == '(' || c == '[' || c == '{'; }
static int de_is_close_bracket(char c) { return c == ')' || c == ']' || c == '}'; }
static char de_matching_bracket(char c) {
    switch (c) {
    case '(': return ')';
    case ')': return '(';
    case '[': return ']';
    case ']': return '[';
    case '{': return '}';
    case '}': return '{';
    default:  return 0;
    }
}

void document_match_bracket(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    size_t total = doc->buffer.len;
    if (total == 0) return;
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= total) pos = total - 1;
    char c = doc->buffer.text[pos];

    /* If not on a bracket, scan forward on the current line for one. */
    if (!de_is_open_bracket(c) && !de_is_close_bracket(c)) {
        size_t p = pos;
        while (p < total && doc->buffer.text[p] != '\n') {
            char cc = doc->buffer.text[p];
            if (de_is_open_bracket(cc) || de_is_close_bracket(cc)) {
                pos = p;
                c = cc;
                break;
            }
            p++;
        }
        if (!de_is_open_bracket(c) && !de_is_close_bracket(c)) return;
    }

    char match = de_matching_bracket(c);
    if (!match) return;

    int depth = 0;
    if (de_is_open_bracket(c)) {
        for (size_t p = pos; p < total; p++) {
            char cc = doc->buffer.text[p];
            if (cc == c) depth++;
            else if (cc == match) {
                depth--;
                if (depth == 0) {
                    int r, cl;
                    buffer_row_col_from_pos(&doc->buffer, p, &r, &cl);
                    cursor_move_to(cur, r, cl);
                    return;
                }
            }
        }
    } else {
        size_t p = pos + 1;
        while (p-- > 0) {
            char cc = doc->buffer.text[p];
            if (cc == c) depth++;
            else if (cc == match) {
                depth--;
                if (depth == 0) {
                    int r, cl;
                    buffer_row_col_from_pos(&doc->buffer, p, &r, &cl);
                    cursor_move_to(cur, r, cl);
                    return;
                }
            }
        }
    }
}

void document_goto_last_modification(Document *doc) {
    History *h = &doc->history;
    if (h->current <= 0) return;
    HistoryEntry *e = &h->entries[h->current - 1];
    size_t pos = e->pos;
    if (pos > doc->buffer.len) pos = doc->buffer.len;
    int r, c;
    buffer_row_col_from_pos(&doc->buffer, pos, &r, &c);
    cursor_move_to(&doc->cursors[0], r, c);
}

void document_insert_file(Document *doc, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) { fclose(f); return; }
    char *data = malloc((size_t)size);
    if (!data) { fclose(f); return; }
    size_t got = fread(data, 1, (size_t)size, f);
    fclose(f);

    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, data, got);
    history_push_insert(&doc->history, pos, data, got, cur->row, cur->col);
    for (size_t i = 0; i < got; i++) {
        if (data[i] == '\n') { cur->row++; cur->col = 0; }
        else cur->col++;
    }
    free(data);
    doc->dirty = true;
}

void document_move_file(Document *doc, const char *path) {
    if (doc->filepath) {
        rename(doc->filepath, path);
        free(doc->filepath);
    }
    doc->filepath = strdup(path);
    buffer_save(&doc->buffer, path);
    doc->dirty = false;
}

/* Shell command helpers */
static char *get_selection_text(Document *doc, size_t *len_out) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return NULL;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    size_t len = end - start;
    char *text = malloc(len + 1);
    if (!text) return NULL;
    memcpy(text, doc->buffer.text + start, len);
    text[len] = '\0';
    *len_out = len;
    return text;
}

static char *run_shell_command(const char *cmd, const char *input, size_t input_len, size_t *output_len) {
    char *result = NULL;
    FILE *pin = NULL, *pout = NULL;
    
    /* Create temp file for input if needed */
    if (input && input_len > 0) {
        pin = tmpfile();
        if (!pin) return NULL;
        fwrite(input, 1, input_len, pin);
        rewind(pin);
    }
    
    /* Run command with popen */
    char cmd_buf[4096];
    if (input && input_len > 0) {
        snprintf(cmd_buf, sizeof(cmd_buf), "%s", cmd);
    } else {
        snprintf(cmd_buf, sizeof(cmd_buf), "%s", cmd);
    }
    
    pout = popen(cmd_buf, input ? "r" : "r");
    if (!pout) {
        if (pin) fclose(pin);
        return NULL;
    }
    
    /* Read output */
    size_t cap = 4096;
    size_t len = 0;
    result = malloc(cap);
    if (!result) {
        pclose(pout);
        if (pin) fclose(pin);
        return NULL;
    }
    
    size_t n;
    while ((n = fread(result + len, 1, cap - len, pout)) > 0) {
        len += n;
        if (len >= cap) {
            cap *= 2;
            char *tmp = realloc(result, cap);
            if (!tmp) {
                free(result);
                pclose(pout);
                if (pin) fclose(pin);
                return NULL;
            }
            result = tmp;
        }
    }
    
    pclose(pout);
    if (pin) fclose(pin);
    
    *output_len = len;
    return result;
}

void document_pipe_selection(Document *doc, const char *cmd) {
    size_t sel_len = 0;
    char *sel_text = get_selection_text(doc, &sel_len);
    if (!sel_text) return;
    
    size_t out_len = 0;
    char *output = run_shell_command(cmd, sel_text, sel_len, &out_len);
    free(sel_text);
    if (!output) return;
    
    /* Replace selection with output */
    Cursor *cur = &doc->cursors[0];
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    size_t del_len = end - start;
    
    char *deleted = malloc(del_len);
    memcpy(deleted, doc->buffer.text + start, del_len);
    buffer_delete(&doc->buffer, start, del_len);
    history_push_delete(&doc->history, start, deleted, del_len, sr, sc);
    free(deleted);
    
    buffer_insert(&doc->buffer, start, output, out_len);
    history_push_insert(&doc->history, start, output, out_len, sr, sc);
    
    /* Update cursor position */
    int row = sr, col = sc;
    for (size_t i = 0; i < out_len; i++) {
        if (output[i] == '\n') { row++; col = 0; }
        else col++;
    }
    cursor_move_to(cur, row, col);
    cursor_clear_selection(cur);
    
    free(output);
    doc->dirty = true;
}

void document_pipe_to(Document *doc, const char *cmd) {
    size_t sel_len = 0;
    char *sel_text = get_selection_text(doc, &sel_len);
    if (!sel_text) return;
    
    size_t out_len = 0;
    char *output = run_shell_command(cmd, sel_text, sel_len, &out_len);
    free(sel_text);
    free(output); /* Ignore output, just run command */
    
    cursor_clear_selection(&doc->cursors[0]);
}

void document_insert_output(Document *doc, const char *cmd) {
    size_t out_len = 0;
    char *output = run_shell_command(cmd, NULL, 0, &out_len);
    if (!output) return;
    
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, output, out_len);
    history_push_insert(&doc->history, pos, output, out_len, cur->row, cur->col);
    
    /* Move cursor to end of inserted text */
    for (size_t i = 0; i < out_len; i++) {
        if (output[i] == '\n') { cur->row++; cur->col = 0; }
        else cur->col++;
    }
    
    free(output);
    doc->dirty = true;
}

void document_append_output(Document *doc, const char *cmd) {
    size_t out_len = 0;
    char *output = run_shell_command(cmd, NULL, 0, &out_len);
    if (!output) return;
    
    Cursor *cur = &doc->cursors[0];
    
    /* Move to end of line */
    int row = cur->row;
    int col = (int)buffer_line_len(&doc->buffer, row);
    
    size_t pos = buffer_pos_from_row_col(&doc->buffer, row, col);
    buffer_insert(&doc->buffer, pos, output, out_len);
    history_push_insert(&doc->history, pos, output, out_len, row, col);
    
    /* Move cursor to end of inserted text */
    cur->row = row;
    cur->col = col;
    for (size_t i = 0; i < out_len; i++) {
        if (output[i] == '\n') { cur->row++; cur->col = 0; }
        else cur->col++;
    }
    
    free(output);
    doc->dirty = true;
}

void document_filter_selection(Document *doc, const char *cmd) {
    size_t sel_len = 0;
    char *sel_text = get_selection_text(doc, &sel_len);
    if (!sel_text) return;
    
    size_t out_len = 0;
    char *output = run_shell_command(cmd, sel_text, sel_len, &out_len);
    free(sel_text);
    if (!output) return;
    
    /* Keep only lines that match (have output) */
    Cursor *cur = &doc->cursors[0];
    if (out_len > 0) {
        /* Selection remains */
        cursor_clear_selection(cur);
    } else {
        /* Delete selection */
        document_delete_selection(doc);
    }
    
    free(output);
}
