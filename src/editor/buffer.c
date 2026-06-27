#include "dragon_editor/buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 4096

void buffer_init(Buffer *b) {
    if (!b) return;
    b->text = malloc(INITIAL_CAP);
    if (!b->text) {
        b->len = 0;
        b->cap = 0;
        return;
    }
    b->text[0] = '\0';
    b->len = 0;
    b->cap = INITIAL_CAP;
}

void buffer_free(Buffer *b) {
    if (!b) return;
    free(b->text);
    b->text = NULL;
    b->len = 0;
    b->cap = 0;
}

void buffer_clear(Buffer *b) {
    if (!b || !b->text) return;
    b->text[0] = '\0';
    b->len = 0;
}

bool buffer_load(Buffer *b, const char *path) {
    if (!b || !path) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long sz = ftell(f);
    if (sz < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    if ((size_t)sz + 1 > b->cap) {
        size_t new_cap = (size_t)sz + 1;
        char *text = realloc(b->text, new_cap);
        if (!text) {
            fclose(f);
            return false;
        }
        b->text = text;
        b->cap = new_cap;
    }
    b->len = fread(b->text, 1, (size_t)sz, f);
    b->text[b->len] = '\0';
    fclose(f);
    return true;
}

bool buffer_save(Buffer *b, const char *path) {
    if (!b || !b->text || !path) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(b->text, 1, b->len, f) == b->len;
    fclose(f);
    return ok;
}

size_t buffer_line_count(Buffer *b) {
    if (!b || !b->text) return 1;
    if (b->len == 0) return 1;
    size_t count = 1;
    for (size_t i = 0; i < b->len; i++)
        if (b->text[i] == '\n') count++;
    return count;
}

size_t buffer_line_len(Buffer *b, size_t line) {
    if (!b || !b->text) return 0;
    size_t cur = 0;
    size_t start = 0;
    for (size_t i = 0; i <= b->len; i++) {
        if (i == b->len || b->text[i] == '\n') {
            if (cur == line) return i - start;
            start = i + 1;
            cur++;
        }
    }
    return 0;
}

const char *buffer_line_ptr(Buffer *b, size_t line) {
    if (!b || !b->text) return "";
    size_t cur = 0;
    for (size_t i = 0; i <= b->len; i++) {
        if (i == 0 || b->text[i-1] == '\n') {
            if (cur == line) return &b->text[i];
            cur++;
        }
    }
    return &b->text[b->len];
}

void buffer_insert(Buffer *b, size_t pos, const char *text, size_t len) {
    if (!b || !b->text || !text || len == 0) return;
    if (pos > b->len) pos = b->len;
    if (len > (size_t)-1 - b->len - 1) return;
    if (b->len + len + 1 > b->cap) {
        size_t new_cap = b->cap ? b->cap : INITIAL_CAP;
        while (new_cap < b->len + len + 1) {
            if (new_cap > (size_t)-1 / 2) {
                new_cap = b->len + len + 1;
                break;
            }
            new_cap *= 2;
        }
        char *new_text = realloc(b->text, new_cap);
        if (!new_text) return;
        b->text = new_text;
        b->cap = new_cap;
    }
    memmove(&b->text[pos + len], &b->text[pos], b->len - pos + 1);
    memcpy(&b->text[pos], text, len);
    b->len += len;
}

void buffer_delete(Buffer *b, size_t pos, size_t len) {
    if (!b || !b->text || len == 0) return;
    if (pos >= b->len) return;
    if (pos + len > b->len) len = b->len - pos;
    memmove(&b->text[pos], &b->text[pos + len], b->len - pos - len + 1);
    b->len -= len;
}

void buffer_append(Buffer *b, const char *text, size_t len) {
    if (!b) return;
    buffer_insert(b, b->len, text, len);
}

size_t buffer_pos_from_row_col(Buffer *b, int row, int col) {
    if (!b || !b->text) return 0;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    size_t pos = 0;
    for (int r = 0; r < row; r++) {
        while (pos < b->len && b->text[pos] != '\n') pos++;
        if (pos < b->len) pos++;
    }
    pos += col;
    if (pos > b->len) pos = b->len;
    return pos;
}

void buffer_row_col_from_pos(Buffer *b, size_t pos, int *row, int *col) {
    if (!row || !col) return;
    if (!b || !b->text) {
        *row = 0;
        *col = 0;
        return;
    }
    int r = 0, c = 0;
    for (size_t i = 0; i < pos && i < b->len; i++) {
        if (b->text[i] == '\n') { r++; c = 0; }
        else c++;
    }
    *row = r;
    *col = c;
}
