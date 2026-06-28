#include "dragon_editor/document.h"
#include "dragon_editor/lsp.h"
#include "dragon_editor/treesitter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <regex.h>

#define MAX_CURSORS 64

static char *filepath_to_uri(const char *filepath);

static bool language_is_c_family(const char *lang) {
    return lang && (strcmp(lang, "c") == 0 || strcmp(lang, "cpp") == 0 ||
                    strcmp(lang, "objc") == 0 || strcmp(lang, "objcpp") == 0 ||
                    strcmp(lang, "cuda") == 0 || strcmp(lang, "java") == 0);
}

static bool language_uses_slash_comments(const char *lang) {
    return language_is_c_family(lang) ||
           (lang && (strcmp(lang, "javascript") == 0 || strcmp(lang, "typescript") == 0 ||
                     strcmp(lang, "rust") == 0 || strcmp(lang, "go") == 0));
}

static bool language_uses_hash_comments(const char *lang) {
    return lang && strcmp(lang, "python") == 0;
}

static bool language_is_script(const char *lang) {
    return lang && (strcmp(lang, "python") == 0 || strcmp(lang, "javascript") == 0 ||
                    strcmp(lang, "typescript") == 0 || strcmp(lang, "go") == 0 ||
                    strcmp(lang, "rust") == 0);
}

static bool fallback_keyword(const char *word, int len, const char *lang, SyntaxType *type) {
    static const char *keywords[] = {
        "if", "else", "for", "while", "do", "switch", "case", "default",
        "break", "continue", "return", "goto", "struct", "union", "enum",
        "class", "interface", "typedef", "sizeof", "extern", "static",
        "const", "volatile", "auto", "register", "void", "int", "float",
        "double", "char", "short", "long", "unsigned", "signed", "bool",
        "true", "false", "null", "nil", "def", "fn", "func", "var", "let",
        "new", "delete", "try", "catch", "finally", "throw", "throws",
        "import", "include", "require", "module", "package", "namespace",
        "public", "private", "protected", "async", "await", "yield", "self",
        "Self", "impl", "trait", "in", "not", "and", "or", "is", "as",
        "with", "from", "lambda", "pass", "elif", "except", "assert",
        "raise", "local", "global", "type", "ref", "defer", "go", "chan",
        "select", "make", "len", "append", "panic", "recover", NULL
    };
    static const char *macros[] = {
        "define", "ifdef", "ifndef", "endif", "elif", "pragma", "undef", NULL
    };
    (void)lang;
    for (int i = 0; macros[i]; i++) {
        if ((int)strlen(macros[i]) == len && strncmp(word, macros[i], len) == 0) {
            *type = SYNTAX_MACRO;
            return true;
        }
    }
    for (int i = 0; keywords[i]; i++) {
        if ((int)strlen(keywords[i]) == len && strncmp(word, keywords[i], len) == 0) {
            *type = SYNTAX_KEYWORD;
            return true;
        }
    }
    return false;
}

static void fallback_highlight_line(Document *doc, int row, const char *line, int len) {
    bool slash_comments = language_uses_slash_comments(doc->language_id);
    bool hash_comments = language_uses_hash_comments(doc->language_id);
    int i = 0;
    while (i < len) {
        char ch = line[i];
        if (slash_comments && ch == '/' && i + 1 < len && line[i + 1] == '/') {
            syntax_add_token(&doc->syntax, row, i, row, len, SYNTAX_COMMENT);
            return;
        }
        if (slash_comments && ch == '/' && i + 1 < len && line[i + 1] == '*') {
            int start = i;
            i += 2;
            while (i + 1 < len && !(line[i] == '*' && line[i + 1] == '/')) i++;
            i = (i + 1 < len) ? i + 2 : len;
            syntax_add_token(&doc->syntax, row, start, row, i, SYNTAX_COMMENT);
            continue;
        }
        if (hash_comments && ch == '#') {
            syntax_add_token(&doc->syntax, row, i, row, len, SYNTAX_COMMENT);
            return;
        }
        if (ch == '"' || ch == '\'' || ch == '`') {
            char quote = ch;
            int start = i++;
            while (i < len) {
                if (line[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (line[i++] == quote) break;
            }
            syntax_add_token(&doc->syntax, row, start, row, i, SYNTAX_STRING);
            continue;
        }
        if (isdigit((unsigned char)ch)) {
            int start = i++;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '.' || line[i] == '_')) i++;
            syntax_add_token(&doc->syntax, row, start, row, i, SYNTAX_NUMBER);
            continue;
        }
        if (isalpha((unsigned char)ch) || ch == '_') {
            int start = i++;
            while (i < len && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            SyntaxType type = SYNTAX_NORMAL;
            if (fallback_keyword(line + start, i - start, doc->language_id, &type)) {
                syntax_add_token(&doc->syntax, row, start, row, i, type);
            } else {
                int j = i;
                while (j < len && isspace((unsigned char)line[j])) j++;
                if (j < len && line[j] == '(')
                    syntax_add_token(&doc->syntax, row, start, row, i, SYNTAX_FUNCTION);
            }
            continue;
        }
        if (strchr("+-*/%=!<>&|^~.?:", ch)) {
            syntax_add_token(&doc->syntax, row, i, row, i + 1, SYNTAX_OPERATOR);
        }
        i++;
    }
}

static char *document_absolute_path(const char *path) {
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

void document_mark_dirty(Document *doc) {
    if (!doc) return;
    doc->dirty = true;
    doc->syntax_dirty = true;
    doc->lsp_dirty = true;
    doc->ts_parsed = false;
    doc->ts_attempted = false;
}

void document_init(Document *doc) {
    memset(doc, 0, sizeof(*doc));
    buffer_init(&doc->buffer);
    doc->cursor_count = 1;
    cursor_init(&doc->cursors[0]);
    doc->viewport_lines = 40;
    doc->viewport_cols = 80;
    history_init(&doc->history);
    doc->language_id = NULL;
    syntax_init(&doc->syntax, NULL);
    doc->goto_results = NULL;
    doc->goto_result_count = 0;
    doc->hover_result = NULL;
    doc->diagnostics = NULL;
    doc->syntax_dirty = true;
    doc->lsp_opened = false;
    doc->lsp_version = 0;
    doc->ts_attempted = false;
    macro_init(&doc->macros);
    doc->alt_filepath = NULL;
}

void document_free(Document *doc) {
    buffer_free(&doc->buffer);
    history_free(&doc->history);
    syntax_free(&doc->syntax);
    free(doc->filepath);
    free(doc->clipboard);
    free(doc->language_id);
    if (doc->goto_results) {
        for (int i = 0; i < doc->goto_result_count; i++) {
            free(doc->goto_results[i].uri);
        }
        free(doc->goto_results);
    }
    if (doc->hover_result) {
        lsp_free_hover((LSPHover *)doc->hover_result);
    }
    if (doc->diagnostics) {
        lsp_free_diagnostics((LSPDiagnostics *)doc->diagnostics);
    }
    macro_free(&doc->macros);
    free(doc->alt_filepath);
    memset(doc, 0, sizeof(*doc));
}

void document_open(Document *doc, const char *path) {
    if (!doc || !path) return;
    char *path_copy = strdup(path);
    if (!path_copy) return;
    if (!buffer_load(&doc->buffer, path_copy)) {
        free(path_copy);
        return;
    }
    char *absolute = document_absolute_path(path_copy);
    if (absolute) {
        free(path_copy);
        path_copy = absolute;
    }
    free(doc->filepath);
    doc->filepath = path_copy;
    doc->dirty = false;
    doc->syntax_dirty = true;
    doc->lsp_dirty = false;
    doc->lsp_opened = false;
    doc->lsp_version = 0;
    doc->ts_parsed = false;
    doc->ts_attempted = false;
    doc->cursor_count = 1;
    cursor_init(&doc->cursors[0]);
    doc->scroll_y = 0;
    doc->scroll_x = 0;
    history_clear(&doc->history);
    document_detect_language(doc);
    
    /* Initialize syntax highlighting for the language */
    syntax_free(&doc->syntax);
    syntax_init(&doc->syntax, doc->language_id);
}

void document_save(Document *doc) {
    if (!doc->filepath) return;
    buffer_save(&doc->buffer, doc->filepath);
    doc->dirty = false;
}

void document_save_as(Document *doc, const char *path) {
    if (!doc || !path) return;
    char *path_copy = document_absolute_path(path);
    if (!path_copy) return;
    free(doc->filepath);
    doc->filepath = path_copy;
    buffer_save(&doc->buffer, doc->filepath);
    char *resolved = realpath(doc->filepath, NULL);
    if (resolved) {
        free(doc->filepath);
        doc->filepath = resolved;
    }
    doc->dirty = false;
    doc->syntax_dirty = true;
    doc->lsp_dirty = true;
    doc->lsp_opened = false;
    doc->lsp_version = 0;
    doc->ts_parsed = false;
    doc->ts_attempted = false;
    document_detect_language(doc);
    syntax_free(&doc->syntax);
    syntax_init(&doc->syntax, doc->language_id);
}

void document_insert_char(Document *doc, char c) {
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, &c, 1);
    history_push_insert(&doc->history, pos, &c, 1, cur->row, cur->col);
    cur->col++;
    document_mark_dirty(doc);
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
        cur->col = len;
    }
    document_mark_dirty(doc);
}

void document_delete_selection(Document *doc) {
    if (!doc) return;
    if (doc->cursor_count < 1) doc->cursor_count = 1;
    if (doc->cursor_count > MAX_CURSORS) doc->cursor_count = MAX_CURSORS;

    int order[MAX_CURSORS];
    int count = 0;
    for (int i = 0; i < doc->cursor_count; i++) {
        if (doc->cursors[i].has_selection)
            order[count++] = i;
    }
    if (count == 0) return;

    bool own_group = doc->history.active_group == 0 && count > 1;
    if (own_group)
        history_begin_group(&doc->history);

    for (int i = 1; i < count; i++) {
        for (int j = i; j > 0; j--) {
            int ar, ac, ae_r, ae_c;
            int br, bc, be_r, be_c;
            cursor_normalize(&doc->cursors[order[j]], &ar, &ac, &ae_r, &ae_c);
            cursor_normalize(&doc->cursors[order[j - 1]], &br, &bc, &be_r, &be_c);
            size_t pa = buffer_pos_from_row_col(&doc->buffer, ar, ac);
            size_t pb = buffer_pos_from_row_col(&doc->buffer, br, bc);
            if (pa < pb) {
                int tmp = order[j];
                order[j] = order[j - 1];
                order[j - 1] = tmp;
            } else {
                break;
            }
        }
    }

    for (int i = count - 1; i >= 0; i--) {
        Cursor *cur = &doc->cursors[order[i]];
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
        size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
        if (end < start) continue;
        size_t len = end - start;
        char *deleted = len > 0 ? malloc(len) : NULL;
        if (len > 0 && deleted)
            memcpy(deleted, doc->buffer.text + start, len);
        buffer_delete(&doc->buffer, start, len);
        if (len > 0 && deleted)
            history_push_delete(&doc->history, start, deleted, len, sr, sc);
        free(deleted);
        cursor_move_to(cur, sr, sc);
        cursor_clear_selection(cur);
    }
    if (own_group)
        history_end_group(&doc->history);
    document_mark_dirty(doc);
}

void document_newline(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    char nl = '\n';
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, &nl, 1);
    history_push_insert(&doc->history, pos, &nl, 1, cur->row, cur->col);
    cur->row++;
    cur->col = 0;
    document_mark_dirty(doc);

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
    document_mark_dirty(doc);
}

void document_move_line_up(Document *doc) {
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    size_t lines = buffer_line_count(&doc->buffer);
    if (cur->row == 0 || lines < 2) return;

    size_t prev_start = buffer_pos_from_row_col(&doc->buffer, cur->row - 1, 0);
    size_t cur_start = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
    size_t next_start = (size_t)cur->row + 1 < lines
        ? buffer_pos_from_row_col(&doc->buffer, cur->row + 1, 0)
        : doc->buffer.len;
    size_t len = next_start - cur_start;

    char *line = malloc(len);
    if (!line) return;
    memcpy(line, doc->buffer.text + cur_start, len);

    buffer_delete(&doc->buffer, cur_start, len);
    buffer_insert(&doc->buffer, prev_start, line, len);
    free(line);

    cur->row--;
    document_mark_dirty(doc);
}

void document_move_line_down(Document *doc) {
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    size_t lines = buffer_line_count(&doc->buffer);
    if (cur->row >= (int)lines - 1 || lines < 2) return;

    size_t cur_start = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
    size_t next_start = buffer_pos_from_row_col(&doc->buffer, cur->row + 1, 0);
    size_t after_next = (size_t)cur->row + 2 < lines
        ? buffer_pos_from_row_col(&doc->buffer, cur->row + 2, 0)
        : doc->buffer.len;
    size_t len = next_start - cur_start;
    size_t next_len = after_next - next_start;

    char *line = malloc(len);
    if (!line) return;
    memcpy(line, doc->buffer.text + cur_start, len);

    buffer_delete(&doc->buffer, cur_start, len);
    size_t insert_pos = cur_start + next_len;
    buffer_insert(&doc->buffer, insert_pos, line, len);
    free(line);

    cur->row++;
    document_mark_dirty(doc);
}

static void document_move_cursor_one(Document *doc, Cursor *cur, int dr, int dc) {
    size_t max_lines = buffer_line_count(&doc->buffer);
    
    cur->row += dr;
    cur->col += dc;
    
    /* Clamp row to valid range */
    if (cur->row < 0) cur->row = 0;
    if (cur->row >= (int)max_lines) cur->row = (int)max_lines - 1;
    if (cur->row < 0) cur->row = 0;  /* Handle empty buffer */
    
    /* Handle column wrapping at line boundaries */
    if (cur->col < 0) {
        if (cur->row > 0) {
            cur->row--;
            cur->col = (int)buffer_line_len(&doc->buffer, cur->row) - 1;
            if (cur->col < 0) cur->col = 0;
        } else {
            cur->col = 0;
        }
    }
    
    /* Clamp column to current line (allow positioning at end of line) */
    size_t max_col = buffer_line_len(&doc->buffer, cur->row);
    if (cur->col > (int)max_col) cur->col = (int)max_col;
    if (cur->col < 0) cur->col = 0;
}

void document_move_cursor(Document *doc, int dr, int dc) {
    if (!doc) return;
    if (doc->cursor_count < 1) doc->cursor_count = 1;
    if (doc->cursor_count > MAX_CURSORS) doc->cursor_count = MAX_CURSORS;
    for (int i = 0; i < doc->cursor_count; i++)
        document_move_cursor_one(doc, &doc->cursors[i], dr, dc);

    /* Keep cursor visible in viewport */
    document_sync_viewport_to_cursor(doc);
}

void document_cursor_to(Document *doc, int row, int col) {
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    int max_row = (int)buffer_line_count(&doc->buffer) - 1;
    if (row < 0) row = 0;
    if (row > max_row) row = max_row;
    if (row < 0) row = 0;
    cur->row = row;
    cur->col = col;
    size_t len = buffer_line_len(&doc->buffer, cur->row);
    if (cur->col > (int)len) cur->col = (int)len;
    if (cur->col < 0) cur->col = 0;
    document_sync_viewport_to_cursor(doc);
}

void document_cursor_home(Document *doc) {
    if (!doc) return;
    if (doc->cursor_count < 1) doc->cursor_count = 1;
    if (doc->cursor_count > MAX_CURSORS) doc->cursor_count = MAX_CURSORS;
    for (int i = 0; i < doc->cursor_count; i++)
        doc->cursors[i].col = 0;
}

void document_cursor_end(Document *doc) {
    if (!doc) return;
    if (doc->cursor_count < 1) doc->cursor_count = 1;
    if (doc->cursor_count > MAX_CURSORS) doc->cursor_count = MAX_CURSORS;
    for (int i = 0; i < doc->cursor_count; i++) {
        Cursor *cur = &doc->cursors[i];
        int len = (int)buffer_line_len(&doc->buffer, cur->row);
        cur->col = len;
    }
}

void document_cursor_page_up(Document *doc) {
    document_move_cursor(doc, -doc->viewport_lines, 0);
    document_sync_viewport_to_cursor(doc);
}

void document_cursor_page_down(Document *doc) {
    document_move_cursor(doc, doc->viewport_lines, 0);
    document_sync_viewport_to_cursor(doc);
}

void document_cursor_doc_start(Document *doc) {
    doc->cursors[0].row = 0;
    doc->cursors[0].col = 0;
    document_sync_viewport_to_cursor(doc);
}

void document_cursor_doc_end(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    size_t lines = buffer_line_count(&doc->buffer);
    cur->row = (int)lines - 1;
    if (cur->row < 0) cur->row = 0;
    document_cursor_end(doc);
    document_sync_viewport_to_cursor(doc);
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

/* Ensure cursor is visible in viewport */
void document_sync_viewport_to_cursor(Document *doc) {
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    if (doc->viewport_lines < 1)
        doc->viewport_lines = 1;
    if (doc->viewport_cols < 1)
        doc->viewport_cols = 1;
    
    /* If cursor is above visible area, scroll up */
    if (cur->row < doc->scroll_y) {
        doc->scroll_y = cur->row;
    }
    
    /* If cursor is below visible area, scroll down */
    int visible_bottom = doc->scroll_y + doc->viewport_lines - 1;
    if (cur->row > visible_bottom) {
        doc->scroll_y = cur->row - doc->viewport_lines + 1;
    }
    
    /* Ensure scroll_y doesn't go negative */
    if (doc->scroll_y < 0) doc->scroll_y = 0;

    if (cur->col < doc->scroll_x)
        doc->scroll_x = cur->col;

    int visible_right = doc->scroll_x + doc->viewport_cols - 1;
    if (cur->col > visible_right)
        doc->scroll_x = cur->col - doc->viewport_cols + 1;

    if (doc->scroll_x < 0) doc->scroll_x = 0;
}

void document_undo(Document *doc) {
    HistoryEntry *e = history_peek_undo(&doc->history);
    if (!e) return;
    Cursor *cur = &doc->cursors[0];
    int group = e->group;
    int last_row = e->cursor_row;
    int last_col = e->cursor_col;
    do {
        if (e->type == OP_INSERT) {
            buffer_delete(&doc->buffer, e->pos, e->len);
        } else {
            buffer_insert(&doc->buffer, e->pos, e->text, e->len);
        }
        last_row = e->cursor_row;
        last_col = e->cursor_col;
        history_regress(&doc->history);
        e = history_peek_undo(&doc->history);
    } while (group && e && e->group == group);
    cursor_move_to(cur, last_row, last_col);
    document_mark_dirty(doc);
}

void document_redo(Document *doc) {
    HistoryEntry *e = history_peek_redo(&doc->history);
    if (!e) return;
    Cursor *cur = &doc->cursors[0];
    int group = e->group;
    size_t new_pos = e->pos;
    do {
        if (e->type == OP_INSERT) {
            buffer_insert(&doc->buffer, e->pos, e->text, e->len);
        } else {
            buffer_delete(&doc->buffer, e->pos, e->len);
        }
        new_pos = e->pos;
        if (e->type == OP_INSERT) new_pos += e->len;
        history_advance(&doc->history);
        e = history_peek_redo(&doc->history);
    } while (group && e && e->group == group);
    int row, col;
    buffer_row_col_from_pos(&doc->buffer, new_pos, &row, &col);
    cursor_move_to(cur, row, col);
    document_mark_dirty(doc);
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
    cur->col = i < len ? i : len;
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
    if (!doc) return;
    if (doc->cursor_count < 1) doc->cursor_count = 1;
    if (doc->cursor_count >= MAX_CURSORS) return;

    Cursor *primary = &doc->cursors[0];
    Cursor *last = &doc->cursors[doc->cursor_count - 1];
    const char *word;
    int word_len;
    get_word_at_cursor(primary, &doc->buffer, &word, &word_len);
    if (word_len == 0) return;

    /* Search forward from the last cursor so repeated calls add the next occurrence. */
    size_t search_start = buffer_pos_from_row_col(&doc->buffer, last->row, last->col) + word_len;
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
        bool duplicate = false;
        for (int i = 0; i < doc->cursor_count; i++) {
            if (doc->cursors[i].row == row && doc->cursors[i].col == col) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            search_start = pos + 1;
            continue;
        }
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
    if (!doc) return;
    if (doc->cursor_count < 1) {
        doc->cursor_count = 1;
        cursor_init(&doc->cursors[0]);
        return;
    }
    Cursor primary = doc->cursors[0];
    doc->cursor_count = 1;
    doc->cursors[0] = primary;
}

void document_insert_char_multi(Document *doc, char c) {
    if (!doc) return;
    history_begin_group(&doc->history);
    document_delete_selection(doc);
    if (doc->cursor_count < 1) doc->cursor_count = 1;
    if (doc->cursor_count > MAX_CURSORS) doc->cursor_count = MAX_CURSORS;

    /* Sort cursor indices by buffer position ascending, then iterate descending */
    int order[MAX_CURSORS];
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
        history_push_insert(&doc->history, pos, &c, 1, cur->row, cur->col);
        cur->col++;
    }
    history_end_group(&doc->history);
    document_mark_dirty(doc);
}

void document_delete_char_multi(Document *doc) {
    if (!doc) return;
    for (int i = 0; i < doc->cursor_count; i++) {
        if (doc->cursors[i].has_selection) {
            history_begin_group(&doc->history);
            document_delete_selection(doc);
            history_end_group(&doc->history);
            return;
        }
    }
    if (doc->cursor_count < 1) doc->cursor_count = 1;
    if (doc->cursor_count > MAX_CURSORS) doc->cursor_count = MAX_CURSORS;

    int order[MAX_CURSORS];
    for (int i = 0; i < doc->cursor_count; i++) order[i] = i;
    for (int i = 1; i < doc->cursor_count; i++) {
        for (int j = i; j > 0; j--) {
            size_t pa = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j]].row, doc->cursors[order[j]].col);
            size_t pb = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j-1]].row, doc->cursors[order[j-1]].col);
            if (pa < pb) { int tmp = order[j]; order[j] = order[j-1]; order[j-1] = tmp; }
            else break;
        }
    }
    history_begin_group(&doc->history);
    for (int i = doc->cursor_count - 1; i >= 0; i--) {
        Cursor *cur = &doc->cursors[order[i]];
        if (cur->row == 0 && cur->col == 0) continue;
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
        if (pos == 0) continue;
        char deleted = doc->buffer.text[pos - 1];
        buffer_delete(&doc->buffer, pos - 1, 1);
        history_push_delete(&doc->history, pos - 1, &deleted, 1, cur->row, cur->col);
        if (cur->col > 0) cur->col--;
        else {
            cur->row--;
            int len = (int)buffer_line_len(&doc->buffer, cur->row);
            cur->col = len;
        }
    }
    history_end_group(&doc->history);
    document_mark_dirty(doc);
}

void document_newline_multi(Document *doc) {
    if (!doc) return;
    if (doc->cursor_count < 1) doc->cursor_count = 1;
    if (doc->cursor_count > MAX_CURSORS) doc->cursor_count = MAX_CURSORS;

    int order[MAX_CURSORS];
    for (int i = 0; i < doc->cursor_count; i++) order[i] = i;
    for (int i = 1; i < doc->cursor_count; i++) {
        for (int j = i; j > 0; j--) {
            size_t pa = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j]].row, doc->cursors[order[j]].col);
            size_t pb = buffer_pos_from_row_col(&doc->buffer, doc->cursors[order[j-1]].row, doc->cursors[order[j-1]].col);
            if (pa < pb) { int tmp = order[j]; order[j] = order[j-1]; order[j-1] = tmp; }
            else break;
        }
    }
    history_begin_group(&doc->history);
    for (int i = doc->cursor_count - 1; i >= 0; i--) {
        Cursor *cur = &doc->cursors[order[i]];
        char nl = '\n';
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
        buffer_insert(&doc->buffer, pos, &nl, 1);
        history_push_insert(&doc->history, pos, &nl, 1, cur->row, cur->col);
        cur->row++;
        cur->col = 0;
    }
    history_end_group(&doc->history);
    document_mark_dirty(doc);
}

void document_replace_char(Document *doc, char c) {
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= doc->buffer.len) return;
    history_push_delete(&doc->history, pos, &doc->buffer.text[pos], 1, cur->row, cur->col);
    doc->buffer.text[pos] = c;
    history_push_insert(&doc->history, pos, &c, 1, cur->row, cur->col);
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
}

void document_cursor_first_non_blank(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    int i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t'))
        i++;
    cur->col = i < len ? i : len;
}

void document_join_lines(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    size_t lines = buffer_line_count(&doc->buffer);
    if ((size_t)cur->row + 1 >= lines) return;

    int cur_len = (int)buffer_line_len(&doc->buffer, cur->row);
    size_t join_pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur_len);

    if (join_pos < doc->buffer.len && doc->buffer.text[join_pos] == '\n') {
        const char *next = buffer_line_ptr(&doc->buffer, cur->row + 1);
        int next_indent = 0;
        while (next[next_indent] == ' ' || next[next_indent] == '\t') next_indent++;
        buffer_delete(&doc->buffer, join_pos, 1);
        if (next_indent > 0) {
            buffer_delete(&doc->buffer, join_pos, next_indent);
        }
    }
    cur->col = cur_len;
    document_mark_dirty(doc);
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
    if (cur->col > (int)max_col) cur->col = (int)max_col;
    if (cur->col < 0) cur->col = 0;
    document_mark_dirty(doc);
}

void document_indent_line(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    char tab = '\t';
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
    buffer_insert(&doc->buffer, pos, &tab, 1);
    history_push_insert(&doc->history, pos, &tab, 1, cur->row, 0);
    cur->col++;
    document_mark_dirty(doc);
}

void document_dedent_line(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    if (line[0] == '\t') {
        size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
        buffer_delete(&doc->buffer, pos, 1);
        history_push_delete(&doc->history, pos, "\t", 1, cur->row, 0);
        if (cur->col > 0) cur->col--;
        document_mark_dirty(doc);
    } else if (line[0] == ' ') {
        int spaces = 0;
        while (line[spaces] == ' ' && spaces < 4) spaces++;
        if (spaces > 0) {
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
            buffer_delete(&doc->buffer, pos, spaces);
            history_push_delete(&doc->history, pos, line, spaces, cur->row, 0);
            cur->col -= spaces;
            if (cur->col < 0) cur->col = 0;
            document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
        document_mark_dirty(doc);
        return;
    }
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    do_toggle_case(doc, sr, sc, er, ec);
    document_mark_dirty(doc);
}

void document_lowercase(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    do_set_case(doc, sr, sc, er, ec, 32);
    document_mark_dirty(doc);
}

void document_uppercase(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    do_set_case(doc, sr, sc, er, ec, -32);
    document_mark_dirty(doc);
}

void document_indent_selection(Document *doc) {
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        document_indent_line(doc);
        return;
    }
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    (void)sc; (void)ec;
    int max_row = (int)buffer_line_count(&doc->buffer) - 1;
    if (max_row < 0) return;
    if (sr < 0) sr = 0;
    if (er > max_row) er = max_row;
    if (er < sr) return;
    for (int r = sr; r <= er; r++) {
        char tab = '\t';
        size_t pos = buffer_pos_from_row_col(&doc->buffer, r, 0);
        buffer_insert(&doc->buffer, pos, &tab, 1);
        history_push_insert(&doc->history, pos, &tab, 1, r, 0);
    }
    document_mark_dirty(doc);
}

void document_dedent_selection(Document *doc) {
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        document_dedent_line(doc);
        return;
    }
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    (void)sc; (void)ec;
    int max_row = (int)buffer_line_count(&doc->buffer) - 1;
    if (max_row < 0) return;
    if (sr < 0) sr = 0;
    if (er > max_row) er = max_row;
    if (er < sr) return;
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
    document_mark_dirty(doc);
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
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    if (cur->has_selection) {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        cursor_move_to(cur, er, ec);
        cursor_clear_selection(cur);
    }
    doc->cursor_count = 1;
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
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        if (doc->cursor_count >= MAX_CURSORS) return;
        int line_count = (int)buffer_line_count(&doc->buffer);
        Cursor *base = &doc->cursors[0];
        for (int i = 1; i < doc->cursor_count; i++) {
            if (doc->cursors[i].row > base->row)
                base = &doc->cursors[i];
        }
        int row = base->row + 1;
        if (row >= line_count) return;
        int col = base->col;
        int len = (int)buffer_line_len(&doc->buffer, row);
        if (col > len) col = len;
        Cursor *new_cur = &doc->cursors[doc->cursor_count++];
        cursor_init(new_cur);
        cursor_move_to(new_cur, row, col);
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
    document_mark_dirty(doc);
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
        if (join_pos < doc->buffer.len && doc->buffer.text[join_pos] == '\n') {
            buffer_delete(&doc->buffer, join_pos, 1);
            int indent = 0;
            while (line[indent] == ' ' || line[indent] == '\t') indent++;
            if (indent > 0 && len > 0) {
                buffer_delete(&doc->buffer, join_pos, indent);
            }
        }
    }
    document_mark_dirty(doc);
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
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    int viewport_width = doc->viewport_cols > 0 ? doc->viewport_cols : 80;
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
    if (!doc) return;
    free(doc->search_query);
    doc->search_query = NULL;
    doc->search_len = 0;
    if (!query || len == 0) return;
    doc->search_query = malloc(len + 1);
    if (!doc->search_query) return;
    memcpy(doc->search_query, query, len);
    doc->search_query[len] = '\0';
    doc->search_len = len;
}

static bool search_literal_forward(Document *doc, size_t start, size_t *match_start, size_t *match_end) {
    if (!doc || !doc->search_query || doc->search_len == 0 || start > doc->buffer.len)
        return false;
    const char *found = memmem(doc->buffer.text + start, doc->buffer.len - start,
                               doc->search_query, doc->search_len);
    if (!found) return false;
    *match_start = (size_t)(found - doc->buffer.text);
    *match_end = *match_start + doc->search_len;
    return true;
}

static bool search_literal_prev_before(Document *doc, size_t limit, size_t *match_start, size_t *match_end) {
    if (!doc || !doc->search_query || doc->search_len == 0) return false;
    if (limit > doc->buffer.len) limit = doc->buffer.len;
    bool found = false;
    size_t best = 0;
    for (size_t i = 0; i + doc->search_len <= limit; i++) {
        if (memcmp(doc->buffer.text + i, doc->search_query, doc->search_len) == 0) {
            best = i;
            found = true;
        }
    }
    if (!found) return false;
    *match_start = best;
    *match_end = best + doc->search_len;
    return true;
}

static bool search_regex_forward(Document *doc, size_t start, size_t *match_start, size_t *match_end) {
    if (!doc || !doc->search_query || doc->search_len == 0 || start > doc->buffer.len)
        return false;
    regex_t re;
    if (regcomp(&re, doc->search_query, REG_EXTENDED) != 0)
        return search_literal_forward(doc, start, match_start, match_end);
    regmatch_t m;
    int ok = regexec(&re, doc->buffer.text + start, 1, &m, 0);
    if (ok == 0 && m.rm_so >= 0) {
        *match_start = start + (size_t)m.rm_so;
        *match_end = start + (size_t)m.rm_eo;
        regfree(&re);
        return true;
    }
    regfree(&re);
    return false;
}

static bool search_regex_prev_before(Document *doc, size_t limit, size_t *match_start, size_t *match_end) {
    if (!doc || !doc->search_query || doc->search_len == 0) return false;
    if (limit > doc->buffer.len) limit = doc->buffer.len;
    regex_t re;
    if (regcomp(&re, doc->search_query, REG_EXTENDED) != 0)
        return search_literal_prev_before(doc, limit, match_start, match_end);

    bool found = false;
    size_t offset = 0;
    while (offset < limit) {
        regmatch_t m;
        if (regexec(&re, doc->buffer.text + offset, 1, &m, 0) != 0 || m.rm_so < 0)
            break;
        size_t start = offset + (size_t)m.rm_so;
        size_t end = offset + (size_t)m.rm_eo;
        if (start >= limit)
            break;
        *match_start = start;
        *match_end = end;
        found = true;
        offset = end > offset ? end : offset + 1;
    }
    regfree(&re);
    return found;
}

static void document_select_match_at_positions(Document *doc, size_t start, size_t end) {
    Cursor *cur = &doc->cursors[0];
    int sr, sc, er, ec;
    if (end < start) end = start;
    buffer_row_col_from_pos(&doc->buffer, start, &sr, &sc);
    buffer_row_col_from_pos(&doc->buffer, end, &er, &ec);
    cursor_move_to(cur, sr, sc);
    cur->anchor_row = er;
    cur->anchor_col = ec;
    cur->has_selection = true;
    document_sync_viewport_to_cursor(doc);
}

void document_search_next(Document *doc) {
    if (!doc->search_query || doc->search_len == 0) return;
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (cur->has_selection) {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        pos = buffer_pos_from_row_col(&doc->buffer, er, ec);
    } else if (pos < doc->buffer.len) {
        pos++;
    }

    size_t start = 0, end = 0;
    if (search_regex_forward(doc, pos, &start, &end) ||
        search_regex_forward(doc, 0, &start, &end))
        document_select_match_at_positions(doc, start, end);
}

void document_search_prev(Document *doc) {
    if (!doc->search_query || doc->search_len == 0) return;
    Cursor *cur = &doc->cursors[0];
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (cur->has_selection) {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        pos = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    }

    size_t start = 0, end = 0;
    if (search_regex_prev_before(doc, pos, &start, &end) ||
        search_regex_prev_before(doc, doc->buffer.len, &start, &end))
        document_select_match_at_positions(doc, start, end);
}

void document_extend_to_line_bounds(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) cursor_select_start(cur);
    int len = (int)buffer_line_len(&doc->buffer, cur->row);
    cur->col = len;
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
    cursor_move_to(cur, cur->row, len);
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
        document_mark_dirty(doc);
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
            document_mark_dirty(doc);
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
        document_mark_dirty(doc);
    }
}

void document_copy_selection_above(Document *doc) {
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        if (doc->cursor_count >= MAX_CURSORS) return;
        Cursor *base = &doc->cursors[0];
        for (int i = 1; i < doc->cursor_count; i++) {
            if (doc->cursors[i].row < base->row)
                base = &doc->cursors[i];
        }
        int row = base->row - 1;
        if (row < 0) return;
        int col = base->col;
        int len = (int)buffer_line_len(&doc->buffer, row);
        if (col > len) col = len;
        Cursor *new_cur = &doc->cursors[doc->cursor_count++];
        cursor_init(new_cur);
        cursor_move_to(new_cur, row, col);
        return;
    }
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
    document_mark_dirty(doc);
}

void document_join_lines_with_space(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) {
        size_t len = buffer_line_len(&doc->buffer, cur->row);
        size_t join_pos = buffer_pos_from_row_col(&doc->buffer, cur->row, len);
        if (join_pos < doc->buffer.len && doc->buffer.text[join_pos] == '\n') {
            doc->buffer.text[join_pos] = ' ';
            document_mark_dirty(doc);
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
        document_mark_dirty(doc);
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
    document_mark_dirty(doc);
}

void document_format_selection(Document *doc) {
    if (!doc) return;
    document_trim_whitespace(doc);
}

bool document_format_with_lsp(Document *doc, void *lsp_manager, int tab_size, bool insert_spaces) {
    if (!doc || !doc->filepath || !doc->language_id || !lsp_manager) return false;
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    if (!client || client->status != LSP_STATUS_INITIALIZED) return false;

    char *uri = filepath_to_uri(doc->filepath);
    if (!uri) return false;
    lsp_client_send_formatting_request(client, uri, tab_size, insert_spaces);
    free(uri);

    for (int i = 0; i < 5; i++) {
        usleep(40000);
        char *response = lsp_client_read_response(client);
        if (!response) continue;
        LSPWorkspaceEdit *edit = lsp_parse_formatting_response(response);
        free(response);
        if (edit && edit->count > 0) {
            document_apply_workspace_edit(doc, edit);
            lsp_free_workspace_edit(edit);
            document_notify_lsp_change(doc, manager);
            return true;
        }
        lsp_free_workspace_edit(edit);
    }
    return false;
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
    if (!doc) return;
    document_free(doc);
    document_init(doc);
}

static int cmp_lines(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

void document_sort_selection(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return;
    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    int line_count = er - sr + 1;
    if (line_count < 2) return;

    /* Collect line pointers and lengths */
    const char **lines = malloc(sizeof(char *) * line_count);
    int *lengths = malloc(sizeof(int) * line_count);
    for (int i = 0; i < line_count; i++) {
        lines[i] = buffer_line_ptr(&doc->buffer, sr + i);
        lengths[i] = (int)buffer_line_len(&doc->buffer, sr + i);
    }

    /* Sort the line pointers by content */
    qsort(lines, line_count, sizeof(char *), cmp_lines);

    /* Rebuild the selected region */
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, 0);
    size_t old_end = buffer_pos_from_row_col(&doc->buffer, er, lengths[line_count - 1]);
    size_t old_len = old_end - start;

    /* Build new sorted text */
    size_t new_cap = 0;
    for (int i = 0; i < line_count; i++) new_cap += lengths[i] + 1;
    char *new_text = malloc(new_cap);
    size_t pos = 0;
    for (int i = 0; i < line_count; i++) {
        memcpy(new_text + pos, lines[i], lengths[i]);
        pos += lengths[i];
        if (sr + i < er) {
            new_text[pos++] = '\n';
        }
    }

    buffer_delete(&doc->buffer, start, old_len);
    buffer_insert(&doc->buffer, start, new_text, pos);
    free(new_text);
    free(lines);
    free(lengths);
    document_mark_dirty(doc);
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
    cur->col = i < len ? i : len;
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
            document_mark_dirty(doc);
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
            document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
    } else {
        ec = (int)buffer_line_len(&doc->buffer, er);
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
    } else {
        ec = (int)buffer_line_len(&doc->buffer, er);
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
}

void document_move_file(Document *doc, const char *path) {
    if (!doc || !path) return;
    char *path_copy = strdup(path);
    if (!path_copy) return;
    if (doc->filepath) {
        rename(doc->filepath, path_copy);
        free(doc->filepath);
    }
    doc->filepath = path_copy;
    buffer_save(&doc->buffer, doc->filepath);
    doc->dirty = false;
    doc->syntax_dirty = true;
    doc->lsp_dirty = true;
    doc->ts_parsed = false;
    doc->ts_attempted = false;
    document_detect_language(doc);
    syntax_free(&doc->syntax);
    syntax_init(&doc->syntax, doc->language_id);
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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
    document_mark_dirty(doc);
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

/* Language detection based on file extension */
void document_detect_language(Document *doc) {
    free(doc->language_id);
    doc->language_id = NULL;
    
    if (!doc->filepath) return;
    
    /* Get file extension */
    const char *ext = strrchr(doc->filepath, '.');
    if (!ext) return;
    
    ext++;  /* Skip the dot */
    
    /* Map extensions to language IDs */
    const char *lang_id = NULL;
    if (strcmp(ext, "c") == 0) lang_id = "c";
    else if (strcmp(ext, "h") == 0) lang_id = "c";
    else if (strcmp(ext, "cpp") == 0 || strcmp(ext, "cc") == 0 || strcmp(ext, "cxx") == 0) lang_id = "cpp";
    else if (strcmp(ext, "hpp") == 0 || strcmp(ext, "hh") == 0 || strcmp(ext, "hxx") == 0) lang_id = "cpp";
    else if (strcmp(ext, "m") == 0) lang_id = "objc";  /* Objective-C */
    else if (strcmp(ext, "mm") == 0) lang_id = "objcpp";  /* Objective-C++ */
    else if (strcmp(ext, "cu") == 0) lang_id = "cuda";  /* CUDA */
    else if (strcmp(ext, "rs") == 0) lang_id = "rust";
    else if (strcmp(ext, "py") == 0) lang_id = "python";
    else if (strcmp(ext, "go") == 0) lang_id = "go";
    else if (strcmp(ext, "ts") == 0) lang_id = "typescript";
    else if (strcmp(ext, "js") == 0) lang_id = "javascript";
    else if (strcmp(ext, "java") == 0) lang_id = "java";
    
    if (lang_id) {
        doc->language_id = malloc(strlen(lang_id) + 1);
        strcpy(doc->language_id, lang_id);
    }
}

/* File path to file:// URI conversion */
static char *filepath_to_uri(const char *filepath) {
    if (!filepath) return NULL;

    char *absolute = NULL;
    if (filepath[0] == '/') {
        absolute = strdup(filepath);
    } else {
        char *cwd = getcwd(NULL, 0);
        if (!cwd) return NULL;
        size_t len = strlen(cwd) + 1 + strlen(filepath) + 1;
        absolute = malloc(len);
        if (absolute)
            snprintf(absolute, len, "%s/%s", cwd, filepath);
        free(cwd);
    }
    if (!absolute) return NULL;

    size_t cap = strlen(absolute) * 3 + 8;
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

static bool diagnostic_uri_matches_document(const LSPDiagnostics *diagnostics, Document *doc) {
    if (!diagnostics || !diagnostics->uri || diagnostics->uri[0] == '\0')
        return true;
    if (!doc || !doc->filepath)
        return false;

    char *doc_uri = filepath_to_uri(doc->filepath);
    if (!doc_uri) return false;
    bool matches = strcmp(diagnostics->uri, doc_uri) == 0;
    if (!matches) {
        char decoded[2048];
        size_t decoded_len = 0;
        for (const char *p = diagnostics->uri; *p && decoded_len < sizeof(decoded) - 1; p++) {
            if (*p == '%' && p[1] && p[2]) {
                int hi = (p[1] >= '0' && p[1] <= '9') ? p[1] - '0' :
                         (p[1] >= 'a' && p[1] <= 'f') ? p[1] - 'a' + 10 :
                         (p[1] >= 'A' && p[1] <= 'F') ? p[1] - 'A' + 10 : -1;
                int lo = (p[2] >= '0' && p[2] <= '9') ? p[2] - '0' :
                         (p[2] >= 'a' && p[2] <= 'f') ? p[2] - 'a' + 10 :
                         (p[2] >= 'A' && p[2] <= 'F') ? p[2] - 'A' + 10 : -1;
                if (hi >= 0 && lo >= 0) {
                    decoded[decoded_len++] = (char)((hi << 4) | lo);
                    p += 2;
                    continue;
                }
            }
            decoded[decoded_len++] = *p;
        }
        decoded[decoded_len] = '\0';
        matches = strcmp(decoded, doc_uri) == 0;
    }
    free(doc_uri);
    return matches;
}

void document_notify_lsp_open(Document *doc, void *lsp_manager) {
    if (!doc || !doc->filepath || !doc->language_id || !lsp_manager) return;
    
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    if (!client) return;
    
    char *text = malloc(doc->buffer.len + 1);
    if (!text) return;

    memcpy(text, doc->buffer.text, doc->buffer.len);
    text[doc->buffer.len] = '\0';
    
    char *uri = filepath_to_uri(doc->filepath);
    if (uri) {
        lsp_client_send_didOpen(client, uri, doc->language_id, text);
        doc->lsp_opened = true;
        doc->lsp_version = 1;
        free(uri);
    }
    free(text);
}

void document_notify_lsp_change(Document *doc, void *lsp_manager) {
    if (!doc || !doc->filepath || !doc->language_id || !lsp_manager) return;

    if (!doc->lsp_opened) {
        document_notify_lsp_open(doc, lsp_manager);
        return;
    }
    
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    if (!client) return;
    
    char *text = malloc(doc->buffer.len + 1);
    if (!text) return;

    memcpy(text, doc->buffer.text, doc->buffer.len);
    text[doc->buffer.len] = '\0';
    
    char *uri = filepath_to_uri(doc->filepath);
    if (uri) {
        if (doc->lsp_version < 1)
            doc->lsp_version = 1;
        doc->lsp_version++;
        lsp_client_send_didChange(client, uri, doc->lsp_version, text);
        free(uri);
    }
    free(text);
}

/* Helper: Navigate cursor to line/col with bounds checking */
static void document_goto_position(Document *doc, int line, int col) {
    Cursor *cur = &doc->cursors[0];
    size_t max_lines = buffer_line_count(&doc->buffer);
    
    /* Bounds check line */
    if (line < 0) line = 0;
    if (line >= (int)max_lines) line = (int)max_lines - 1;
    if (line < 0) line = 0;  /* Empty buffer */
    
    cur->row = line;
    
    /* Bounds check column */
    size_t line_len = buffer_line_len(&doc->buffer, line);
    if (col < 0) col = 0;
    if (col > (int)line_len) col = (int)line_len;
    if (col < 0) col = 0;
    
    cur->col = col;
    document_sync_viewport_to_cursor(doc);
}

/* LSP goto definition */
void document_lsp_goto_definition(Document *doc, void *lsp_manager) {
    if (!doc || !doc->language_id || !lsp_manager) return;
    
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    
    if (!client) return;
    
    Cursor *cur = &doc->cursors[0];
    
    /* Convert file path to URI */
    char *uri = filepath_to_uri(doc->filepath);
    if (!uri) return;
    
    /* Send definition request */
    lsp_client_send_definition_request(client, uri, cur->row, cur->col);
    
    /* Give LSP server time to respond */
    usleep(100000);
    
    /* Read response */
    char *response = lsp_client_read_response(client);
    if (!response) {
        free(uri);
        return;
    }
    
    /* Parse response */
    int count = 0;
    LSPLocation *locations = lsp_parse_definition_response(response, &count);
    
    /* Store results */
    if (doc->goto_results) {
        for (int i = 0; i < doc->goto_result_count; i++) {
            free(doc->goto_results[i].uri);
        }
        free(doc->goto_results);
    }
    
    doc->goto_result_count = count;
    if (count > 0) {
        doc->goto_results = malloc(count * sizeof(LSPGotoResult));
        for (int i = 0; i < count; i++) {
            doc->goto_results[i].uri = strdup(locations[i].uri ? locations[i].uri : "");
            doc->goto_results[i].line = locations[i].range.start.line;
            doc->goto_results[i].character = locations[i].range.start.character;
        }
        lsp_free_locations(locations, count);
        
        /* If only one result, navigate directly */
        if (count == 1) {
            if (strstr(doc->goto_results[0].uri, doc->filepath)) {
                document_goto_position(doc, doc->goto_results[0].line, doc->goto_results[0].character);
            }
        }
    }
    
    free(response);
    free(uri);
}

/* LSP goto type definition */
void document_lsp_goto_type_definition(Document *doc, void *lsp_manager) {
    if (!doc || !doc->language_id || !lsp_manager) return;
    
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    
    if (!client) return;
    
    Cursor *cur = &doc->cursors[0];
    
    /* Convert file path to URI */
    char *uri = filepath_to_uri(doc->filepath);
    if (!uri) return;
    
    /* Send type definition request */
    lsp_client_send_type_definition_request(client, uri, cur->row, cur->col);
    
    /* Give LSP server time to respond */
    usleep(100000);
    
    /* Read response */
    char *response = lsp_client_read_response(client);
    if (!response) {
        free(uri);
        return;
    }
    
    /* Parse response */
    int count = 0;
    LSPLocation *locations = lsp_parse_definition_response(response, &count);
    
    /* Store results */
    if (doc->goto_results) {
        for (int i = 0; i < doc->goto_result_count; i++) {
            free(doc->goto_results[i].uri);
        }
        free(doc->goto_results);
    }
    
    doc->goto_result_count = count;
    if (count > 0) {
        doc->goto_results = malloc(count * sizeof(LSPGotoResult));
        for (int i = 0; i < count; i++) {
            doc->goto_results[i].uri = strdup(locations[i].uri ? locations[i].uri : "");
            doc->goto_results[i].line = locations[i].range.start.line;
            doc->goto_results[i].character = locations[i].range.start.character;
        }
        lsp_free_locations(locations, count);
        
        /* If only one result, navigate directly */
        if (count == 1) {
            if (strstr(doc->goto_results[0].uri, doc->filepath)) {
                document_goto_position(doc, doc->goto_results[0].line, doc->goto_results[0].character);
            }
        }
    }
    
    free(response);
    free(uri);
}

/* LSP goto references */
void document_lsp_goto_references(Document *doc, void *lsp_manager) {
    if (!doc || !doc->language_id || !lsp_manager) return;
    
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    
    if (!client) return;
    
    Cursor *cur = &doc->cursors[0];
    
    /* Convert file path to URI */
    char *uri = filepath_to_uri(doc->filepath);
    if (!uri) return;
    
    /* Send references request */
    lsp_client_send_references_request(client, uri, cur->row, cur->col);
    
    /* Give LSP server time to respond */
    usleep(100000);
    
    /* Read response */
    char *response = lsp_client_read_response(client);
    if (!response) {
        free(uri);
        return;
    }
    
    /* Parse response */
    int count = 0;
    LSPLocation *locations = lsp_parse_definition_response(response, &count);
    
    /* Store results */
    if (doc->goto_results) {
        for (int i = 0; i < doc->goto_result_count; i++) {
            free(doc->goto_results[i].uri);
        }
        free(doc->goto_results);
    }
    
    doc->goto_result_count = count;
    if (count > 0) {
        doc->goto_results = malloc(count * sizeof(LSPGotoResult));
        for (int i = 0; i < count; i++) {
            doc->goto_results[i].uri = strdup(locations[i].uri ? locations[i].uri : "");
            doc->goto_results[i].line = locations[i].range.start.line;
            doc->goto_results[i].character = locations[i].range.start.character;
        }
        lsp_free_locations(locations, count);
        
        /* If only one result, navigate directly */
        if (count == 1) {
            if (strstr(doc->goto_results[0].uri, doc->filepath)) {
                document_goto_position(doc, doc->goto_results[0].line, doc->goto_results[0].character);
            }
        }
    }
    
    free(response);
    free(uri);
}

/* LSP goto implementation */
void document_lsp_goto_implementation(Document *doc, void *lsp_manager) {
    if (!doc || !doc->language_id || !lsp_manager) return;
    
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    
    if (!client) return;
    
    Cursor *cur = &doc->cursors[0];
    
    /* Convert file path to URI */
    char *uri = filepath_to_uri(doc->filepath);
    if (!uri) return;
    
    /* Send implementation request */
    lsp_client_send_implementation_request(client, uri, cur->row, cur->col);
    
    /* Give LSP server time to respond */
    usleep(100000);
    
    /* Read response */
    char *response = lsp_client_read_response(client);
    if (!response) {
        free(uri);
        return;
    }
    
    /* Parse response */
    int count = 0;
    LSPLocation *locations = lsp_parse_definition_response(response, &count);
    
    /* Store results */
    if (doc->goto_results) {
        for (int i = 0; i < doc->goto_result_count; i++) {
            free(doc->goto_results[i].uri);
        }
        free(doc->goto_results);
    }
    
    doc->goto_result_count = count;
    if (count > 0) {
        doc->goto_results = malloc(count * sizeof(LSPGotoResult));
        for (int i = 0; i < count; i++) {
            doc->goto_results[i].uri = strdup(locations[i].uri ? locations[i].uri : "");
            doc->goto_results[i].line = locations[i].range.start.line;
            doc->goto_results[i].character = locations[i].range.start.character;
        }
        lsp_free_locations(locations, count);
        
        /* If only one result, navigate directly */
        if (count == 1) {
            if (strstr(doc->goto_results[0].uri, doc->filepath)) {
                document_goto_position(doc, doc->goto_results[0].line, doc->goto_results[0].character);
            }
        }
    }
    
    free(response);
    free(uri);
}

/* LSP hover documentation */
void document_lsp_hover(Document *doc, void *lsp_manager) {
    if (!doc || !doc->language_id || !lsp_manager) return;
    
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    
    if (!client) return;
    
    Cursor *cur = &doc->cursors[0];
    
    /* Convert file path to URI */
    char *uri = filepath_to_uri(doc->filepath);
    if (!uri) return;
    
    /* Send hover request */
    lsp_client_send_hover_request(client, uri, cur->row, cur->col);
    
    /* Give LSP server time to respond */
    usleep(100000);
    
    /* Read response */
    char *response = lsp_client_read_response(client);
    if (!response) {
        free(uri);
        return;
    }
    
    /* Parse response */
    LSPHover *hover = lsp_parse_hover_response(response);
    
    /* Store result */
    if (doc->hover_result) {
        lsp_free_hover((LSPHover *)doc->hover_result);
    }
    
    doc->hover_result = hover;
    
    free(response);
    free(uri);
}

void document_lsp_select_references(Document *doc, void *lsp_manager) {
    if (!doc || !doc->language_id || !doc->filepath || !lsp_manager) return;

    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    if (!client) return;

    Cursor *cur = &doc->cursors[0];
    char *uri = filepath_to_uri(doc->filepath);
    if (!uri) return;

    lsp_client_send_references_request(client, uri, cur->row, cur->col);
    usleep(100000);
    char *response = lsp_client_read_response(client);
    if (!response) {
        free(uri);
        return;
    }

    int count = 0;
    LSPLocation *locations = lsp_parse_definition_response(response, &count);
    doc->cursor_count = 1;
    cur->has_selection = false;
    for (int i = 0; locations && i < count && doc->cursor_count < MAX_CURSORS; i++) {
        if (!locations[i].uri || strcmp(locations[i].uri, uri) != 0) continue;
        Cursor *target = (doc->cursor_count == 1 && !cur->has_selection) ?
            cur : &doc->cursors[doc->cursor_count++];
        cursor_init(target);
        cursor_move_to(target, locations[i].range.start.line, locations[i].range.start.character);
        target->anchor_row = locations[i].range.end.line;
        target->anchor_col = locations[i].range.end.character;
        target->has_selection = true;
    }

    lsp_free_locations(locations, count);
    free(response);
    free(uri);
}

/* Update syntax highlighting from LSP semantic tokens */
void document_update_syntax_from_lsp(Document *doc, void *lsp_manager) {
    if (!doc || !doc->language_id || !lsp_manager) return;
    
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    
    if (!client) return;
    
    /* Convert file path to URI */
    char *uri = filepath_to_uri(doc->filepath);
    if (!uri) return;
    
    /* Send semantic tokens request */
    lsp_client_send_semantic_tokens_request(client, uri);
    
    /* Give LSP server time to respond (non-blocking read follows) */
    usleep(50000);  /* 50ms - reduced from 200ms to minimize freeze */
    
    /* Read response */
    char *response = lsp_client_read_response(client);
    if (!response) {
        free(uri);
        return;
    }
    
    /* Only process if it's a semantic tokens response, not diagnostics */
    if (strstr(response, "textDocument/publishDiagnostics")) {
        /* It's a diagnostics notification, skip it (will be handled by app_update_diagnostics) */
        lsp_client_unread_response(client, response);
        free(response);
        free(uri);
        return;
    }
    
    /* Parse and update syntax */
    syntax_update_from_lsp(&doc->syntax, response);
    
    free(response);
    free(uri);
}

static bool document_run_syntax_fallback(Document *doc, bool clear_existing) {
    if (!doc || !doc->language_id) return false;
    if (!language_is_c_family(doc->language_id) && !language_is_script(doc->language_id))
        return false;

    if (clear_existing)
        syntax_clear(&doc->syntax);
    int token_count_before = doc->syntax.token_count;
    size_t lines = buffer_line_count(&doc->buffer);
    for (size_t row = 0; row < lines; row++) {
        const char *line = buffer_line_ptr(&doc->buffer, row);
        int len = (int)buffer_line_len(&doc->buffer, row);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) len--;
        fallback_highlight_line(doc, (int)row, line, len);
    }
    return doc->syntax.token_count > token_count_before;
}

bool document_update_syntax_fallback(Document *doc) {
    return document_run_syntax_fallback(doc, true);
}

/* Update diagnostics from LSP */
void document_update_diagnostics_from_lsp(Document *doc, void *lsp_manager) {
    if (!doc || !doc->language_id || !lsp_manager) return;
    
    LSPManager *manager = (LSPManager *)lsp_manager;
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    
    if (!client) return;
    
    /* Try to read any pending notifications from LSP server */
    char *response = lsp_client_read_response(client);
    if (!response) return;
    
    /* Check if it's a publishDiagnostics notification */
    LSPDiagnostics *diagnostics = lsp_parse_publish_diagnostics_notification(response);
    
    if (diagnostics) {
        if (diagnostic_uri_matches_document(diagnostics, doc)) {
            if (doc->diagnostics) {
                lsp_free_diagnostics((LSPDiagnostics *)doc->diagnostics);
            }
            doc->diagnostics = diagnostics;
        } else {
            lsp_free_diagnostics(diagnostics);
        }
    } else {
        lsp_client_unread_response(client, response);
    }
    
    free(response);
}

/* Treesitter integration - parse document with treesitter */
bool document_parse_treesitter(Document *doc, void *ts_manager) {
    if (!doc || !doc->filepath || !ts_manager) return false;
    
    TreeSitterManager *manager = (TreeSitterManager *)ts_manager;
    
    /* Get file extension to determine language */
    const char *dot = strrchr(doc->filepath, '.');
    if (!dot || dot == doc->filepath) return false;
    
    const char *ext = dot + 1;
    
    /* Load language parser if not already loaded */
    if (!treesitter_load_language(manager, ext)) {
        return false;
    }
    
    /* Get the correct language based on file extension */
    const char *lang_name = NULL;
    lang_name = treesitter_language_name_for_extension(ext);
    
    if (!lang_name) return false;
    
    TreeSitterLanguage *lang = treesitter_get_language(manager, lang_name);
    if (!lang) return false;
    
    /* Parse with treesitter */
    if (doc->buffer.len == 0) return false;
    treesitter_parse(lang, doc->buffer.text, (uint32_t)doc->buffer.len);
    
    /* Generate syntax tokens from tree-sitter parse */
    bool produced_tokens = treesitter_generate_syntax_tokens(lang, &doc->syntax);
    if (produced_tokens)
        document_run_syntax_fallback(doc, false);
    return produced_tokens;
}

static TreeSitterLanguage *document_treesitter_language(Document *doc, void *ts_manager) {
    if (!doc || !doc->filepath || !ts_manager) return NULL;
    const char *dot = strrchr(doc->filepath, '.');
    if (!dot || dot == doc->filepath) return NULL;

    TreeSitterManager *manager = (TreeSitterManager *)ts_manager;
    const char *ext = dot + 1;
    const char *lang_name = treesitter_language_name_for_extension(ext);
    if (!lang_name) return NULL;
    if (!treesitter_load_language(manager, ext)) return NULL;

    TreeSitterLanguage *lang = treesitter_get_language(manager, lang_name);
    if (!lang || !lang->tree) {
        document_parse_treesitter(doc, ts_manager);
        lang = treesitter_get_language(manager, lang_name);
    }
    if (!lang || !lang->tree) return NULL;
    return lang;
}

static void document_select_range(Document *doc, uint32_t sr, uint32_t sc, uint32_t er, uint32_t ec) {
    Cursor *cur = &doc->cursors[0];
    cursor_move_to(cur, (int)sr, (int)sc);
    cur->anchor_row = (int)er;
    cur->anchor_col = (int)ec;
    cur->has_selection = true;
    document_sync_viewport_to_cursor(doc);
}

static void document_current_range(Document *doc, uint32_t *sr, uint32_t *sc, uint32_t *er, uint32_t *ec) {
    Cursor *cur = &doc->cursors[0];
    if (cur->has_selection) {
        int isr, isc, ier, iec;
        cursor_normalize(cur, &isr, &isc, &ier, &iec);
        *sr = (uint32_t)isr;
        *sc = (uint32_t)isc;
        *er = (uint32_t)ier;
        *ec = (uint32_t)iec;
    } else {
        *sr = (uint32_t)cur->row;
        *sc = (uint32_t)cur->col;
        *er = (uint32_t)cur->row;
        *ec = (uint32_t)cur->col;
    }
}

void document_select_treesitter_parent(Document *doc, void *ts_manager) {
    TreeSitterLanguage *lang = document_treesitter_language(doc, ts_manager);
    if (!lang) return;
    Cursor *cur = &doc->cursors[0];
    uint32_t sr = 0, sc = 0, er = 0, ec = 0;
    if (treesitter_parent_range_at(lang, (uint32_t)cur->row, (uint32_t)cur->col, &sr, &sc, &er, &ec))
        document_select_range(doc, sr, sc, er, ec);
}

void document_select_treesitter_child(Document *doc, void *ts_manager) {
    TreeSitterLanguage *lang = document_treesitter_language(doc, ts_manager);
    if (!lang) return;
    uint32_t csr = 0, csc = 0, cer = 0, cec = 0;
    uint32_t sr = 0, sc = 0, er = 0, ec = 0;
    document_current_range(doc, &csr, &csc, &cer, &cec);
    if (treesitter_child_range_for_range(lang, csr, csc, cer, cec, &sr, &sc, &er, &ec))
        document_select_range(doc, sr, sc, er, ec);
}

void document_select_treesitter_sibling(Document *doc, void *ts_manager, int direction) {
    TreeSitterLanguage *lang = document_treesitter_language(doc, ts_manager);
    if (!lang) return;
    uint32_t csr = 0, csc = 0, cer = 0, cec = 0;
    uint32_t sr = 0, sc = 0, er = 0, ec = 0;
    document_current_range(doc, &csr, &csc, &cer, &cec);
    if (treesitter_sibling_range_for_range(lang, csr, csc, cer, cec, direction, &sr, &sc, &er, &ec))
        document_select_range(doc, sr, sc, er, ec);
}

static void document_select_treesitter_ranges(Document *doc, TreeSitterRange *ranges, int count) {
    if (!doc || !ranges || count <= 0) return;
    if (count > MAX_CURSORS) count = MAX_CURSORS;
    doc->cursor_count = count;
    for (int i = 0; i < count; i++) {
        Cursor *cur = &doc->cursors[i];
        cursor_init(cur);
        cursor_move_to(cur, (int)ranges[i].start_row, (int)ranges[i].start_col);
        cur->anchor_row = (int)ranges[i].end_row;
        cur->anchor_col = (int)ranges[i].end_col;
        cur->has_selection = true;
    }
    document_sync_viewport_to_cursor(doc);
}

void document_select_treesitter_all_siblings(Document *doc, void *ts_manager) {
    TreeSitterLanguage *lang = document_treesitter_language(doc, ts_manager);
    if (!lang) return;
    uint32_t csr = 0, csc = 0, cer = 0, cec = 0;
    TreeSitterRange ranges[MAX_CURSORS];
    document_current_range(doc, &csr, &csc, &cer, &cec);
    int count = treesitter_sibling_ranges_for_range(lang, csr, csc, cer, cec, ranges, MAX_CURSORS);
    document_select_treesitter_ranges(doc, ranges, count);
}

void document_select_treesitter_all_children(Document *doc, void *ts_manager) {
    TreeSitterLanguage *lang = document_treesitter_language(doc, ts_manager);
    if (!lang) return;
    uint32_t csr = 0, csc = 0, cer = 0, cec = 0;
    TreeSitterRange ranges[MAX_CURSORS];
    document_current_range(doc, &csr, &csc, &cer, &cec);
    int count = treesitter_child_ranges_for_range(lang, csr, csc, cer, cec, ranges, MAX_CURSORS);
    document_select_treesitter_ranges(doc, ranges, count);
}

void document_move_to_treesitter_parent_edge(Document *doc, void *ts_manager, bool end_edge) {
    TreeSitterLanguage *lang = document_treesitter_language(doc, ts_manager);
    if (!lang) return;
    Cursor *cur = &doc->cursors[0];
    uint32_t sr = 0, sc = 0, er = 0, ec = 0;
    if (!treesitter_parent_range_at(lang, (uint32_t)cur->row, (uint32_t)cur->col, &sr, &sc, &er, &ec))
        return;
    cursor_move_to(cur, end_edge ? (int)er : (int)sr, end_edge ? (int)ec : (int)sc);
    cursor_clear_selection(cur);
    document_sync_viewport_to_cursor(doc);
}

static int diagnostic_pos_cmp(const LSPDiagnostic *diag, int row, int col) {
    if (diag->start_line != row)
        return diag->start_line < row ? -1 : 1;
    if (diag->start_col != col)
        return diag->start_col < col ? -1 : 1;
    return 0;
}

static int first_diagnostic_index(LSPDiagnostics *diags) {
    if (!diags || diags->count <= 0 || !diags->items) return -1;
    int best = 0;
    for (int i = 1; i < diags->count; i++) {
        if (diagnostic_pos_cmp(&diags->items[i],
                               diags->items[best].start_line,
                               diags->items[best].start_col) < 0) {
            best = i;
        }
    }
    return best;
}

static int last_diagnostic_index(LSPDiagnostics *diags) {
    if (!diags || diags->count <= 0 || !diags->items) return -1;
    int best = 0;
    for (int i = 1; i < diags->count; i++) {
        if (diagnostic_pos_cmp(&diags->items[i],
                               diags->items[best].start_line,
                               diags->items[best].start_col) > 0) {
            best = i;
        }
    }
    return best;
}

static void document_goto_diagnostic_item(Document *doc, const LSPDiagnostic *diag) {
    if (!doc || !diag) return;
    document_cursor_to(doc, diag->start_line, diag->start_col);
    document_sync_viewport_to_cursor(doc);
}

/* Diagnostic navigation functions */
void document_goto_next_diagnostic(Document *doc) {
    if (!doc || !doc->diagnostics) return;
    LSPDiagnostics *diags = (LSPDiagnostics *)doc->diagnostics;
    if (!diags->items || diags->count <= 0) return;
    
    Cursor *cur = &doc->cursors[0];
    int best = -1;
    for (int i = 0; i < diags->count; i++) {
        if (diagnostic_pos_cmp(&diags->items[i], cur->row, cur->col) <= 0)
            continue;
        if (best < 0 ||
            diagnostic_pos_cmp(&diags->items[i],
                               diags->items[best].start_line,
                               diags->items[best].start_col) < 0) {
            best = i;
        }
    }
    if (best < 0)
        best = first_diagnostic_index(diags);
    if (best >= 0)
        document_goto_diagnostic_item(doc, &diags->items[best]);
}

void document_goto_prev_diagnostic(Document *doc) {
    if (!doc || !doc->diagnostics) return;
    LSPDiagnostics *diags = (LSPDiagnostics *)doc->diagnostics;
    if (!diags->items || diags->count <= 0) return;
    
    Cursor *cur = &doc->cursors[0];
    int best = -1;
    for (int i = 0; i < diags->count; i++) {
        if (diagnostic_pos_cmp(&diags->items[i], cur->row, cur->col) >= 0)
            continue;
        if (best < 0 ||
            diagnostic_pos_cmp(&diags->items[i],
                               diags->items[best].start_line,
                               diags->items[best].start_col) > 0) {
            best = i;
        }
    }
    if (best < 0)
        best = last_diagnostic_index(diags);
    if (best >= 0)
        document_goto_diagnostic_item(doc, &diags->items[best]);
}

void document_goto_first_diagnostic(Document *doc) {
    if (!doc || !doc->diagnostics) return;
    LSPDiagnostics *diags = (LSPDiagnostics *)doc->diagnostics;
    int idx = first_diagnostic_index(diags);
    if (idx >= 0)
        document_goto_diagnostic_item(doc, &diags->items[idx]);
}

void document_goto_last_diagnostic(Document *doc) {
    if (!doc || !doc->diagnostics) return;
    LSPDiagnostics *diags = (LSPDiagnostics *)doc->diagnostics;
    int idx = last_diagnostic_index(diags);
    if (idx >= 0)
        document_goto_diagnostic_item(doc, &diags->items[idx]);
}

void document_apply_text_edit(Document *doc, const LSPTextEdit *edit) {
    if (!doc || !edit) return;
    
    int start_line = edit->range.start.line;
    int start_col = edit->range.start.character;
    int end_line = edit->range.end.line;
    int end_col = edit->range.end.character;
    
    size_t total_lines = buffer_line_count(&doc->buffer);
    
    /* Clamp to valid range */
    if (start_line < 0) start_line = 0;
    if (start_line >= (int)total_lines) start_line = total_lines - 1;
    if (start_col < 0) start_col = 0;
    if (end_line < 0) end_line = 0;
    if (end_line >= (int)total_lines) end_line = total_lines - 1;
    
    /* Move cursor to start of range */
    document_cursor_to(doc, start_line, start_col);
    
    /* Delete the text in the range */
    if (start_line != end_line || start_col != end_col) {
        int lines_to_delete = end_line - start_line;
        for (int i = 0; i < lines_to_delete; i++) {
            document_delete_char(doc);
            if (buffer_line_count(&doc->buffer) > 1) {
                document_delete_char(doc);  /* Delete newline */
            }
        }
        
        /* Delete remaining characters on the end line */
        int remaining_chars = end_col - start_col;
        for (int i = 0; i < remaining_chars && i < 1024; i++) {
            document_delete_char(doc);
        }
    }
    
    /* Insert new text */
    if (edit->new_text && edit->new_text[0] != '\0') {
        document_insert_text(doc, edit->new_text);
    }
    
    document_mark_dirty(doc);
}

void document_apply_workspace_edit(Document *doc, const LSPWorkspaceEdit *edit) {
    if (!doc || !edit || edit->count == 0) return;
    
    /* Apply edits in reverse order to preserve positions */
    for (int i = edit->count - 1; i >= 0; i--) {
        document_apply_text_edit(doc, &edit->changes[i]);
    }
    
    /* Sync viewport to cursor after all edits */
    document_sync_viewport_to_cursor(doc);
}

/* ================================================================
 * TEXT OBJECTS
 * ================================================================ */

static int find_matching_bracket_forward(const char *text, size_t len, size_t pos, char open, char close) {
    int depth = 0;
    for (size_t i = pos; i < len; i++) {
        if (text[i] == open) depth++;
        else if (text[i] == close) { depth--; if (depth == 0) return (int)i; }
    }
    return -1;
}

static int find_matching_bracket_backward(const char *text, size_t pos, char open, char close) {
    int depth = 0;
    for (int i = (int)pos; i >= 0; i--) {
        if (text[i] == close) depth++;
        else if (text[i] == open) { depth--; if (depth == 0) return i; }
    }
    return -1;
}

static bool is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static bool is_space(char c) {
    return c == ' ' || c == '\t';
}

static bool is_newline(char c) {
    return c == '\n' || c == '\r';
}

static void find_word_bounds(const char *text, size_t len, size_t pos, size_t *start, size_t *end) {
    if (pos >= len) { *start = pos; *end = pos; return; }
    *start = pos;
    *end = pos + 1;
    while (*start > 0 && is_word_char(text[*start - 1])) (*start)--;
    while (*end < len && is_word_char(text[*end])) (*end)++;
}

static void find_non_space(const char *text, size_t len, size_t *pos, int dir) {
    while (*pos < len) {
        if (!is_space(text[*pos]) && !is_newline(text[*pos])) break;
        *pos += dir > 0 ? 1 : -1;
        if (dir > 0 && *pos >= len) break;
        if (dir < 0 && *pos >= len) *pos = 0;
    }
}

void document_select_inside_word(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *text = doc->buffer.text;
    size_t len = doc->buffer.len;
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= len) return;
    size_t start, end;
    find_word_bounds(text, len, pos, &start, &end);
    int sr, sc, er, ec;
    buffer_row_col_from_pos(&doc->buffer, start, &sr, &sc);
    buffer_row_col_from_pos(&doc->buffer, end, &er, &ec);
    cursor_move_to(cur, sr, sc);
    cur->anchor_row = er;
    cur->anchor_col = ec;
    cur->has_selection = true;
}

void document_select_around_word(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *text = doc->buffer.text;
    size_t len = doc->buffer.len;
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= len) return;
    size_t start, end;
    find_word_bounds(text, len, pos, &start, &end);
    if (start > 0) start--;
    if (end < len) end++;
    int sr, sc, er, ec;
    buffer_row_col_from_pos(&doc->buffer, start, &sr, &sc);
    buffer_row_col_from_pos(&doc->buffer, end, &er, &ec);
    cursor_move_to(cur, sr, sc);
    cur->anchor_row = er;
    cur->anchor_col = ec;
    cur->has_selection = true;
}

static void select_bracket_object(Document *doc, char open, char close, bool around) {
    Cursor *cur = &doc->cursors[0];
    const char *text = doc->buffer.text;
    size_t len = doc->buffer.len;
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= len) return;

    /* Try to find the opening bracket at or before cursor */
    int open_pos = -1;
    if (pos < len && text[pos] == open) open_pos = (int)pos;
    else if (pos > 0) {
        for (int i = (int)pos - 1; i >= 0; i--) {
            if (text[i] == open || text[i] == close) {
                if (text[i] == open) open_pos = i;
                break;
            }
        }
    }
    if (open_pos < 0) return;

    int close_pos = find_matching_bracket_forward(text, len, open_pos, open, close);
    if (close_pos < 0) return;

    size_t sel_start = around ? open_pos : open_pos + 1;
    size_t sel_end = around ? close_pos + 1 : close_pos;

    int sr, sc, er, ec;
    buffer_row_col_from_pos(&doc->buffer, sel_start, &sr, &sc);
    buffer_row_col_from_pos(&doc->buffer, sel_end, &er, &ec);
    cursor_move_to(cur, sr, sc);
    cur->anchor_row = er;
    cur->anchor_col = ec;
    cur->has_selection = true;
}

void document_select_inside_paren(Document *doc) { select_bracket_object(doc, '(', ')', false); }
void document_select_around_paren(Document *doc) { select_bracket_object(doc, '(', ')', true); }
void document_select_inside_bracket(Document *doc) { select_bracket_object(doc, '[', ']', false); }
void document_select_around_bracket(Document *doc) { select_bracket_object(doc, '[', ']', true); }
void document_select_inside_curly(Document *doc) { select_bracket_object(doc, '{', '}', false); }
void document_select_around_curly(Document *doc) { select_bracket_object(doc, '{', '}', true); }
void document_select_inside_angle(Document *doc) { select_bracket_object(doc, '<', '>', false); }
void document_select_around_angle(Document *doc) { select_bracket_object(doc, '<', '>', true); }

static void select_quote_object(Document *doc, char quote, bool around) {
    Cursor *cur = &doc->cursors[0];
    const char *text = doc->buffer.text;
    size_t len = doc->buffer.len;
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= len) return;

    /* Find the opening quote at or before cursor */
    int open_pos = -1;
    if (pos < len && text[pos] == quote) open_pos = (int)pos;
    else {
        for (int i = (int)pos; i >= 0; i--) {
            if (text[i] == quote) { open_pos = i; break; }
        }
    }
    if (open_pos < 0) return;

    /* Find the closing quote after the opening */
    int close_pos = -1;
    for (size_t i = open_pos + 1; i < len; i++) {
        if (text[i] == quote && (i == 0 || text[i-1] != '\\')) {
            close_pos = (int)i;
            break;
        }
    }
    if (close_pos < 0) return;

    size_t sel_start = around ? open_pos : open_pos + 1;
    size_t sel_end = around ? close_pos + 1 : close_pos;

    int sr, sc, er, ec;
    buffer_row_col_from_pos(&doc->buffer, sel_start, &sr, &sc);
    buffer_row_col_from_pos(&doc->buffer, sel_end, &er, &ec);
    cursor_move_to(cur, sr, sc);
    cur->anchor_row = er;
    cur->anchor_col = ec;
    cur->has_selection = true;
}

void document_select_inside_quote(Document *doc) { select_quote_object(doc, '"', false); }
void document_select_around_quote(Document *doc) { select_quote_object(doc, '"', true); }
void document_select_inside_backtick(Document *doc) { select_quote_object(doc, '`', false); }
void document_select_around_backtick(Document *doc) { select_quote_object(doc, '`', true); }

void document_select_inside_paragraph(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *text = doc->buffer.text;
    size_t len = doc->buffer.len;
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= len) return;

    /* Find paragraph start (first non-empty line or start of file) */
    size_t para_start = pos;
    /* Go back to find start of current paragraph */
    while (para_start > 0) {
        /* If we hit two newlines, stop - we're at paragraph boundary */
        if (text[para_start - 1] == '\n') {
            /* Check if the line before is empty */
            size_t check = para_start - 1;
            while (check > 0 && text[check - 1] == '\n') check--;
            if (check == 0 || (check > 0 && text[check - 1] == '\n')) {
                /* Check if current line is non-empty */
                if (para_start < len && text[para_start] != '\n') break;
            }
        }
        para_start--;
    }
    /* Skip to first non-newline */
    while (para_start < len && text[para_start] == '\n') para_start++;

    /* Find paragraph end */
    size_t para_end = para_start;
    while (para_end < len && text[para_end] != '\n') para_end++;
    /* Skip trailing newlines but keep one */
    size_t temp = para_end;
    while (temp < len && text[temp] == '\n') temp++;
    if (temp > para_end + 1) para_end = temp - 1;
    else para_end = temp;

    int sr, sc, er, ec;
    buffer_row_col_from_pos(&doc->buffer, para_start, &sr, &sc);
    buffer_row_col_from_pos(&doc->buffer, para_end, &er, &ec);
    cursor_move_to(cur, sr, sc);
    cur->anchor_row = er;
    cur->anchor_col = ec;
    cur->has_selection = true;
}

void document_select_around_paragraph(Document *doc) {
    Cursor *cur = &doc->cursors[0];
    const char *text = doc->buffer.text;
    size_t len = doc->buffer.len;
    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    if (pos >= len) return;

    /* Find paragraph boundaries */
    size_t para_start = pos;
    while (para_start > 0 && text[para_start - 1] != '\n') para_start--;
    while (para_start > 0 && text[para_start - 1] == '\n') para_start--;

    size_t para_end = pos;
    while (para_end < len && text[para_end] != '\n') para_end++;
    while (para_end < len && text[para_end] == '\n') para_end++;

    int sr, sc, er, ec;
    buffer_row_col_from_pos(&doc->buffer, para_start, &sr, &sc);
    buffer_row_col_from_pos(&doc->buffer, para_end, &er, &ec);
    cursor_move_to(cur, sr, sc);
    cur->anchor_row = er;
    cur->anchor_col = ec;
    cur->has_selection = true;
}

/* ================================================================
 * MACROS
 * ================================================================ */

void macro_init(MacroState *ms) {
    if (!ms) return;
    memset(ms, 0, sizeof(MacroState));
    ms->active_slot = -1;
    ms->last_replayed = -1;
}

void macro_free(MacroState *ms) {
    if (!ms) return;
    memset(ms, 0, sizeof(MacroState));
    ms->active_slot = -1;
}

bool macro_start_record(MacroState *ms, int slot) {
    if (!ms || slot < 0 || slot >= MACRO_SLOTS) return false;
    if (ms->active_slot >= 0) return false; /* already recording */
    ms->active_slot = slot;
    ms->slots[slot].len = 0;
    ms->slots[slot].recording = true;
    return true;
}

void macro_stop_record(MacroState *ms) {
    if (!ms || ms->active_slot < 0) return;
    ms->slots[ms->active_slot].recording = false;
    ms->active_slot = -1;
}

bool macro_is_recording(const MacroState *ms) {
    return ms && ms->active_slot >= 0;
}

void macro_record_key(MacroState *ms, int key) {
    if (!ms || ms->active_slot < 0) return;
    MacroSlot *slot = &ms->slots[ms->active_slot];
    if (slot->len < MACRO_MAX_KEYS) {
        slot->keys[slot->len++] = key;
    }
}

bool macro_replay(MacroState *ms, int slot) {
    if (!ms || slot < 0 || slot >= MACRO_SLOTS) return false;
    if (ms->slots[slot].len == 0) return false;
    ms->last_replayed = slot;
    return true;
}

/* ================================================================
 * REFLOW / INDENT STYLE
 * ================================================================ */

void document_reflow(Document *doc, int text_width) {
    if (!doc || text_width < 1) return;
    Cursor *cur = &doc->cursors[0];
    int sr, sc, er, ec;
    if (cur->has_selection) {
        cursor_normalize(cur, &sr, &sc, &er, &ec);
    } else {
        sr = cur->row;
        sc = 0;
        er = (int)buffer_line_count(&doc->buffer) - 1;
        ec = (int)buffer_line_len(&doc->buffer, er);
    }

    /* Extract selected text */
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    if (start >= end || end > doc->buffer.len) return;

    char *text = malloc(end - start + 1);
    memcpy(text, doc->buffer.text + start, end - start);
    text[end - start] = '\0';

    /* Word-wrap the text */
    Buffer result;
    buffer_init(&result);

    int col = 0;
    size_t word_start = 0;
    bool in_word = false;

    for (size_t i = 0; i <= end - start; i++) {
        char c = (i < end - start) ? text[i] : '\n';
        if (c == '\n') {
            if (in_word) {
                size_t word_len = i - word_start;
                if (col > 0 && col + (int)word_len + 1 > text_width) {
                    buffer_append(&result, "\n", 1);
                    col = 0;
                }
                if (col > 0) buffer_append(&result, " ", 1);
                buffer_append(&result, text + word_start, word_len);
                col += (int)word_len;
                in_word = false;
            }
            if (col > 0) {
                buffer_append(&result, "\n", 1);
                col = 0;
            }
        } else if (is_space(c)) {
            if (in_word) {
                size_t word_len = i - word_start;
                if (col > 0 && col + (int)word_len + 1 > text_width) {
                    buffer_append(&result, "\n", 1);
                    col = 0;
                }
                if (col > 0) buffer_append(&result, " ", 1);
                buffer_append(&result, text + word_start, word_len);
                col += (int)word_len;
                in_word = false;
            }
        } else {
            if (!in_word) {
                word_start = i;
                in_word = true;
            }
        }
    }

    /* Replace the region */
    buffer_delete(&doc->buffer, start, end - start);
    buffer_insert(&doc->buffer, start, result.text, result.len);

    /* Update cursor */
    int new_row, new_col;
    buffer_row_col_from_pos(&doc->buffer, start + result.len, &new_row, &new_col);
    cursor_move_to(cur, new_row, new_col);
    cursor_clear_selection(cur);
    document_mark_dirty(doc);
    buffer_free(&result);
    free(text);
}

void document_indent_style_to_tabs(Document *doc, int tab_width) {
    if (!doc || tab_width < 1) return;
    const char *text = doc->buffer.text;
    size_t len = doc->buffer.len;

    Buffer result;
    buffer_init(&result);

    size_t i = 0;
    while (i < len) {
        /* Count leading spaces */
        int spaces = 0;
        size_t line_start = i;
        while (i < len && text[i] == ' ') {
            spaces++;
            i++;
        }
        /* Convert groups of spaces to tabs */
        int tabs = spaces / tab_width;
        int remaining = spaces % tab_width;
        for (int t = 0; t < tabs; t++) buffer_append(&result, "\t", 1);
        for (int s = 0; s < remaining; s++) buffer_append(&result, " ", 1);
        /* Copy rest of line */
        while (i < len && text[i] != '\n') {
            buffer_append(&result, &text[i], 1);
            i++;
        }
        if (i < len) {
            buffer_append(&result, &text[i], 1); /* newline */
            i++;
        }
    }

    buffer_delete(&doc->buffer, 0, len);
    buffer_insert(&doc->buffer, 0, result.text, result.len);
    document_mark_dirty(doc);
    buffer_free(&result);
}

void document_indent_style_to_spaces(Document *doc, int space_width) {
    if (!doc || space_width < 1) return;
    const char *text = doc->buffer.text;
    size_t len = doc->buffer.len;

    Buffer result;
    buffer_init(&result);

    size_t i = 0;
    while (i < len) {
        /* Count leading tabs */
        int tabs = 0;
        while (i < len && text[i] == '\t') {
            tabs++;
            i++;
        }
        /* Convert tabs to spaces */
        for (int t = 0; t < tabs * space_width; t++)
            buffer_append(&result, " ", 1);
        /* Copy rest of line */
        while (i < len && text[i] != '\n') {
            buffer_append(&result, &text[i], 1);
            i++;
        }
        if (i < len) {
            buffer_append(&result, &text[i], 1);
            i++;
        }
    }

    buffer_delete(&doc->buffer, 0, len);
    buffer_insert(&doc->buffer, 0, result.text, result.len);
    document_mark_dirty(doc);
    buffer_free(&result);
}

/* ================================================================
 * ALTERNATE FILE
 * ================================================================ */

void document_set_alternate(Document *doc, const char *path) {
    if (!doc) return;
    free(doc->alt_filepath);
    doc->alt_filepath = path ? strdup(path) : NULL;
}

const char *document_get_alternate(const Document *doc) {
    return doc ? doc->alt_filepath : NULL;
}

void document_goto_alternate(Document *doc) {
    if (!doc || !doc->alt_filepath) return;
    document_open(doc, doc->alt_filepath);
}

/* ================================================================
 * BLOCK COMMENT / LINE COMMENT
 * ================================================================ */

void document_comment_toggle_block(Document *doc, const char *open, const char *close) {
    if (!doc || !open || !close) return;
    Cursor *cur = &doc->cursors[0];
    int sr, sc, er, ec;
    if (cur->has_selection) {
        cursor_normalize(cur, &sr, &sc, &er, &ec);
    } else {
        sr = cur->row;
        sc = 0;
        er = sr;
        ec = (int)buffer_line_len(&doc->buffer, sr);
    }

    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, 0);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    size_t open_len = strlen(open);
    size_t close_len = strlen(close);

    /* Check if already block-commented */
    bool already_commented = false;
    if (start + open_len + close_len <= doc->buffer.len) {
        if (memcmp(doc->buffer.text + start, open, open_len) == 0) {
            /* Find close marker */
            for (size_t i = start + open_len; i + close_len <= end; i++) {
                if (memcmp(doc->buffer.text + i, close, close_len) == 0) {
                    already_commented = true;
                    break;
                }
            }
        }
    }

    if (already_commented) {
        /* Remove block comment: remove close first (higher offset), then open */
        for (size_t i = start + open_len; i + close_len <= end; i++) {
            if (memcmp(doc->buffer.text + i, close, close_len) == 0) {
                buffer_delete(&doc->buffer, i, close_len);
                end -= close_len;
                break;
            }
        }
        buffer_delete(&doc->buffer, start, open_len);
    } else {
        /* Add block comment: insert open at start, close at end */
        buffer_insert(&doc->buffer, start, open, open_len);
        buffer_insert(&doc->buffer, end + open_len, close, close_len);
    }

    document_mark_dirty(doc);
}

void document_comment_toggle_line(Document *doc, const char *prefix) {
    if (!doc || !prefix) return;
    Cursor *cur = &doc->cursors[0];
    size_t prefix_len = strlen(prefix);

    /* For each line in selection (or current line) */
    int sr, sc, er, ec;
    if (cur->has_selection) {
        cursor_normalize(cur, &sr, &sc, &er, &ec);
    } else {
        sr = cur->row;
        er = sr;
    }

    for (int r = sr; r <= er; r++) {
        size_t line_start = buffer_pos_from_row_col(&doc->buffer, r, 0);
        size_t line_len = buffer_line_len(&doc->buffer, r);

        /* Find first non-whitespace */
        size_t indent_pos = line_start;
        while (indent_pos < line_start + line_len &&
               (doc->buffer.text[indent_pos] == ' ' || doc->buffer.text[indent_pos] == '\t'))
            indent_pos++;

        /* Check if already commented */
        if (indent_pos + prefix_len <= doc->buffer.len &&
            memcmp(doc->buffer.text + indent_pos, prefix, prefix_len) == 0) {
            /* Remove comment prefix */
            buffer_delete(&doc->buffer, indent_pos, prefix_len);
        } else {
            /* Add comment prefix at indent */
            buffer_insert(&doc->buffer, indent_pos, prefix, prefix_len);
        }
    }

    document_mark_dirty(doc);
}

/* ================================================================
 * SYSTEM CLIPBOARD
 * ================================================================ */

bool document_yank_to_system_clipboard(Document *doc) {
    if (!doc) return false;
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return false;

    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    if (start >= end) return false;

    size_t len = end - start;
    char *text = malloc(len + 1);
    memcpy(text, doc->buffer.text + start, len);
    text[len] = '\0';

    /* Use xclip to copy to clipboard */
    FILE *pipe = popen("xclip -selection clipboard 2>/dev/null || xsel --clipboard --input 2>/dev/null", "w");
    if (!pipe) { free(text); return false; }
    fwrite(text, 1, len, pipe);
    pclose(pipe);
    free(text);
    return true;
}

bool document_paste_from_system_clipboard(Document *doc) {
    if (!doc) return false;
    Cursor *cur = &doc->cursors[0];

    FILE *pipe = popen("xclip -selection clipboard -o 2>/dev/null || xsel --clipboard --output 2>/dev/null", "r");
    if (!pipe) return false;

    Buffer clip;
    buffer_init(&clip);
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), pipe)) > 0)
        buffer_append(&clip, chunk, n);
    pclose(pipe);

    if (clip.len == 0) { buffer_free(&clip); return false; }

    /* Remove trailing newline if present (linewise paste) */
    while (clip.len > 0 && (clip.text[clip.len - 1] == '\n' || clip.text[clip.len - 1] == '\r'))
        clip.len--;

    if (clip.len == 0) { buffer_free(&clip); return false; }

    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, clip.text, clip.len);
    cur->col += (int)clip.len;
    cursor_clear_selection(cur);
    document_mark_dirty(doc);
    buffer_free(&clip);
    return true;
}

bool document_paste_before_from_system_clipboard(Document *doc) {
    if (!doc) return false;
    Cursor *cur = &doc->cursors[0];

    FILE *pipe = popen("xclip -selection clipboard -o 2>/dev/null || xsel --clipboard --output 2>/dev/null", "r");
    if (!pipe) return false;

    Buffer clip;
    buffer_init(&clip);
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), pipe)) > 0)
        buffer_append(&clip, chunk, n);
    pclose(pipe);

    if (clip.len == 0) { buffer_free(&clip); return false; }

    while (clip.len > 0 && (clip.text[clip.len - 1] == '\n' || clip.text[clip.len - 1] == '\r'))
        clip.len--;

    if (clip.len == 0) { buffer_free(&clip); return false; }

    size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
    buffer_insert(&doc->buffer, pos, clip.text, clip.len);
    cursor_clear_selection(cur);
    document_mark_dirty(doc);
    buffer_free(&clip);
    return true;
}

bool document_replace_selection_from_system_clipboard(Document *doc) {
    if (!doc) return false;
    Cursor *cur = &doc->cursors[0];
    if (!cur->has_selection) return false;

    int sr, sc, er, ec;
    cursor_normalize(cur, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);

    FILE *pipe = popen("xclip -selection clipboard -o 2>/dev/null || xsel --clipboard --output 2>/dev/null", "r");
    if (!pipe) return false;

    Buffer clip;
    buffer_init(&clip);
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), pipe)) > 0)
        buffer_append(&clip, chunk, n);
    pclose(pipe);

    if (clip.len == 0) { buffer_free(&clip); return false; }

    buffer_delete(&doc->buffer, start, end - start);
    buffer_insert(&doc->buffer, start, clip.text, clip.len);
    cursor_move_to(cur, sr, sc);
    cursor_clear_selection(cur);
    document_mark_dirty(doc);
    buffer_free(&clip);
    return true;
}

bool document_yank_main_to_system_clipboard(Document *doc) {
    if (!doc) return false;
    Cursor *cur = &doc->cursors[0];

    /* If no selection, yank the current line */
    int sr, sc, er, ec;
    if (cur->has_selection) {
        cursor_normalize(cur, &sr, &sc, &er, &ec);
    } else {
        sr = cur->row;
        sc = 0;
        er = sr;
        ec = (int)buffer_line_len(&doc->buffer, sr);
    }

    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    if (start >= end) return false;

    size_t len = end - start;
    char *text = malloc(len + 1);
    memcpy(text, doc->buffer.text + start, len);
    text[len] = '\0';

    FILE *pipe = popen("xclip -selection clipboard 2>/dev/null || xsel --clipboard --input 2>/dev/null", "w");
    if (!pipe) { free(text); return false; }
    fwrite(text, 1, len, pipe);
    pclose(pipe);
    free(text);
    return true;
}

/* ================================================================
 * WINDOW / SPLIT MANAGEMENT
 * ================================================================ */

void window_manager_init(WindowManager *wm) {
    if (!wm) return;
    memset(wm, 0, sizeof(WindowManager));
    wm->count = 1;
    wm->active = 0;
    wm->windows[0].doc_index = 0;
    wm->windows[0].x = 0;
    wm->windows[0].y = 0;
    wm->windows[0].width = 100;
    wm->windows[0].height = 40;
    wm->windows[0].visible = true;
    wm->windows[0].parent = -1;
}

int window_split_vertical(WindowManager *wm, int doc_index) {
    if (!wm || wm->count >= MAX_WINDOWS) return -1;
    int idx = wm->count;
    Window *parent = &wm->windows[wm->active];
    Window *w = &wm->windows[idx];
    memset(w, 0, sizeof(Window));
    w->doc_index = doc_index;
    w->x = parent->x + parent->width / 2;
    w->y = parent->y;
    w->width = parent->width / 2;
    w->height = parent->height;
    w->visible = true;
    w->parent = wm->active;
    w->is_horizontal = false;
    parent->width = parent->width / 2;
    parent->active_child = idx;
    wm->count++;
    wm->active = idx;
    return idx;
}

int window_split_horizontal(WindowManager *wm, int doc_index) {
    if (!wm || wm->count >= MAX_WINDOWS) return -1;
    int idx = wm->count;
    Window *parent = &wm->windows[wm->active];
    Window *w = &wm->windows[idx];
    memset(w, 0, sizeof(Window));
    w->doc_index = doc_index;
    w->x = parent->x;
    w->y = parent->y + parent->height / 2;
    w->width = parent->width;
    w->height = parent->height / 2;
    w->visible = true;
    w->parent = wm->active;
    w->is_horizontal = true;
    parent->height = parent->height / 2;
    parent->active_child = idx;
    wm->count++;
    wm->active = idx;
    return idx;
}

void window_close(WindowManager *wm) {
    if (!wm || wm->count <= 1) return;
    int idx = wm->active;
    for (int i = idx; i < wm->count - 1; i++)
        wm->windows[i] = wm->windows[i + 1];
    memset(&wm->windows[wm->count - 1], 0, sizeof(Window));
    wm->count--;
    if (idx >= wm->count) idx = wm->count - 1;
    wm->active = idx < 0 ? 0 : idx;
    window_equalize(wm);
}

void window_next(WindowManager *wm) {
    if (!wm || wm->count <= 1) return;
    int start = wm->active;
    do {
        wm->active = (wm->active + 1) % wm->count;
    } while (wm->active != start && !wm->windows[wm->active].visible);
}

void window_prev(WindowManager *wm) {
    if (!wm || wm->count <= 1) return;
    int start = wm->active;
    do {
        wm->active = (wm->active - 1 + wm->count) % wm->count;
    } while (wm->active != start && !wm->windows[wm->active].visible);
}

void window_goto_left(WindowManager *wm) {
    if (!wm) return;
    Window *w = &wm->windows[wm->active];
    int best = -1;
    int best_dist = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (i == wm->active || !wm->windows[i].visible) continue;
        Window *s = &wm->windows[i];
        bool overlaps = s->y < w->y + w->height && s->y + s->height > w->y;
        int dist = w->x - (s->x + s->width);
        if (overlaps && dist >= 0 && (best < 0 || dist < best_dist)) {
            best = i;
            best_dist = dist;
        }
    }
    if (best >= 0) wm->active = best;
}

void window_goto_right(WindowManager *wm) {
    if (!wm) return;
    Window *w = &wm->windows[wm->active];
    int best = -1;
    int best_dist = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (i == wm->active || !wm->windows[i].visible) continue;
        Window *s = &wm->windows[i];
        bool overlaps = s->y < w->y + w->height && s->y + s->height > w->y;
        int dist = s->x - (w->x + w->width);
        if (overlaps && dist >= 0 && (best < 0 || dist < best_dist)) {
            best = i;
            best_dist = dist;
        }
    }
    if (best >= 0) wm->active = best;
}

void window_goto_up(WindowManager *wm) {
    if (!wm) return;
    Window *w = &wm->windows[wm->active];
    int best = -1;
    int best_dist = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (i == wm->active || !wm->windows[i].visible) continue;
        Window *s = &wm->windows[i];
        bool overlaps = s->x < w->x + w->width && s->x + s->width > w->x;
        int dist = w->y - (s->y + s->height);
        if (overlaps && dist >= 0 && (best < 0 || dist < best_dist)) {
            best = i;
            best_dist = dist;
        }
    }
    if (best >= 0) wm->active = best;
}

void window_goto_down(WindowManager *wm) {
    if (!wm) return;
    Window *w = &wm->windows[wm->active];
    int best = -1;
    int best_dist = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (i == wm->active || !wm->windows[i].visible) continue;
        Window *s = &wm->windows[i];
        bool overlaps = s->x < w->x + w->width && s->x + s->width > w->x;
        int dist = s->y - (w->y + w->height);
        if (overlaps && dist >= 0 && (best < 0 || dist < best_dist)) {
            best = i;
            best_dist = dist;
        }
    }
    if (best >= 0) wm->active = best;
}

static int window_neighbor(WindowManager *wm, int dx, int dy) {
    if (!wm || wm->active < 0 || wm->active >= wm->count) return -1;
    Window *w = &wm->windows[wm->active];
    int best = -1;
    int best_dist = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (i == wm->active || !wm->windows[i].visible) continue;
        Window *s = &wm->windows[i];
        bool overlaps = dx != 0 ?
            (s->y < w->y + w->height && s->y + s->height > w->y) :
            (s->x < w->x + w->width && s->x + s->width > w->x);
        int dist = 0;
        if (dx < 0) dist = w->x - (s->x + s->width);
        else if (dx > 0) dist = s->x - (w->x + w->width);
        else if (dy < 0) dist = w->y - (s->y + s->height);
        else if (dy > 0) dist = s->y - (w->y + w->height);
        if (overlaps && dist >= 0 && (best < 0 || dist < best_dist)) {
            best = i;
            best_dist = dist;
        }
    }
    return best;
}

static void window_swap_geometry(WindowManager *wm, int neighbor) {
    if (!wm || neighbor < 0 || neighbor >= wm->count) return;
    Window *a = &wm->windows[wm->active];
    Window *b = &wm->windows[neighbor];
    int x = a->x, y = a->y, width = a->width, height = a->height;
    a->x = b->x;
    a->y = b->y;
    a->width = b->width;
    a->height = b->height;
    b->x = x;
    b->y = y;
    b->width = width;
    b->height = height;
}

void window_swap_left(WindowManager *wm) {
    window_swap_geometry(wm, window_neighbor(wm, -1, 0));
}

void window_swap_right(WindowManager *wm) {
    window_swap_geometry(wm, window_neighbor(wm, 1, 0));
}

void window_swap_up(WindowManager *wm) {
    window_swap_geometry(wm, window_neighbor(wm, 0, -1));
}

void window_swap_down(WindowManager *wm) {
    window_swap_geometry(wm, window_neighbor(wm, 0, 1));
}

void window_maximize(WindowManager *wm) {
    if (!wm) return;
    Window *w = &wm->windows[wm->active];
    w->x = 0; w->y = 0;
    w->width = 100; w->height = 40;
}

void window_equalize(WindowManager *wm) {
    if (!wm) return;
    if (wm->count <= 1) {
        wm->windows[0].x = 0;
        wm->windows[0].y = 0;
        wm->windows[0].width = 100;
        wm->windows[0].height = 40;
        wm->windows[0].visible = true;
        return;
    }
    int width = 100 / wm->count;
    for (int i = 0; i < wm->count; i++) {
        wm->windows[i].x = i * width;
        wm->windows[i].y = 0;
        wm->windows[i].width = (i == wm->count - 1) ? 100 - i * width : width;
        wm->windows[i].height = 40;
        wm->windows[i].visible = true;
    }
}

/* ================================================================
 * PER-LANGUAGE SETTINGS
 * ================================================================ */

static const LanguageSettings language_settings_table[] = {
    { "c",           8, true,  "/*", "*/", "//",  true },
    { "cpp",         8, true,  "/*", "*/", "//",  true },
    { "objc",        8, true,  "/*", "*/", "//",  true },
    { "objcpp",      8, true,  "/*", "*/", "//",  true },
    { "rust",        4, false, "/*", "*/", "//",  true },
    { "python",      4, false, NULL, NULL, "#",   true },
    { "go",          4, false, "/*", "*/", "//",  true },
    { "javascript",  2, false, "/*", "*/", "//",  true },
    { "typescript",  2, false, "/*", "*/", "//",  true },
    { "java",        4, false, "/*", "*/", "//",  true },
    { "html",        4, false, "<!--","-->", NULL, false },
    { "css",         4, false, "/*", "*/", NULL,  false },
    { "json",        2, false, NULL, NULL, NULL,  false },
    { "yaml",        2, false, NULL, NULL, "#",   false },
    { "toml",        4, false, NULL, NULL, "#",   false },
    { "markdown",    4, false, NULL, NULL, NULL,  false },
    { "sh",          4, false, NULL, NULL, "#",   true },
    { "bash",        4, false, NULL, NULL, "#",   true },
    { "zig",         4, false, "///", "///", "//", true },
    { NULL, 0, false, NULL, NULL, NULL, false }
};

const LanguageSettings *language_settings_get(const char *language_id) {
    if (!language_id) return NULL;
    for (int i = 0; language_settings_table[i].language; i++) {
        if (strcmp(language_settings_table[i].language, language_id) == 0)
            return &language_settings_table[i];
    }
    return NULL;
}

void language_settings_detect(Document *doc, LanguageSettings *out) {
    if (!doc || !out) return;
    const LanguageSettings *ls = language_settings_get(doc->language_id);
    if (ls) {
        *out = *ls;
    } else {
        out->language = doc->language_id;
        out->tab_width = 4;
        out->use_tabs = false;
        out->comment_open = NULL;
        out->comment_close = NULL;
        out->line_comment = NULL;
        out->auto_format = false;
    }
}
