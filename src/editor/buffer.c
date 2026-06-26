#include "dragon_editor/buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 4096

void buffer_init(Buffer *b) {
    b->text = malloc(INITIAL_CAP);
    b->text[0] = '\0';
    b->len = 0;
    b->cap = INITIAL_CAP;
}

void buffer_free(Buffer *b) {
    free(b->text);
    b->text = NULL;
    b->len = 0;
    b->cap = 0;
}

void buffer_clear(Buffer *b) {
    b->text[0] = '\0';
    b->len = 0;
}

bool buffer_load(Buffer *b, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if ((size_t)sz >= b->cap) {
        b->cap = (size_t)sz + 1;
        b->text = realloc(b->text, b->cap);
    }
    b->len = fread(b->text, 1, (size_t)sz, f);
    b->text[b->len] = '\0';
    fclose(f);
    return true;
}

bool buffer_save(Buffer *b, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fwrite(b->text, 1, b->len, f);
    fclose(f);
    return true;
}

size_t buffer_line_count(Buffer *b) {
    if (b->len == 0) return 1;
    size_t count = 1;
    for (size_t i = 0; i < b->len; i++)
        if (b->text[i] == '\n') count++;
    return count;
}

size_t buffer_line_len(Buffer *b, size_t line) {
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
    if (b->len + len >= b->cap) {
        b->cap = (b->len + len) * 2;
        b->text = realloc(b->text, b->cap);
    }
    memmove(&b->text[pos + len], &b->text[pos], b->len - pos + 1);
    memcpy(&b->text[pos], text, len);
    b->len += len;
}

void buffer_delete(Buffer *b, size_t pos, size_t len) {
    if (pos >= b->len) return;
    if (pos + len > b->len) len = b->len - pos;
    memmove(&b->text[pos], &b->text[pos + len], b->len - pos - len + 1);
    b->len -= len;
}

void buffer_append(Buffer *b, const char *text, size_t len) {
    buffer_insert(b, b->len, text, len);
}

size_t buffer_pos_from_row_col(Buffer *b, int row, int col) {
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
    int r = 0, c = 0;
    for (size_t i = 0; i < pos && i < b->len; i++) {
        if (b->text[i] == '\n') { r++; c = 0; }
        else c++;
    }
    *row = r;
    *col = c;
}
