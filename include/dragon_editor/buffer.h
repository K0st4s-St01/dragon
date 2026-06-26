#ifndef DE_BUFFER_H
#define DE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char  *text;
    size_t len;
    size_t cap;
} Buffer;

void   buffer_init(Buffer *b);
void   buffer_free(Buffer *b);
void   buffer_clear(Buffer *b);
bool   buffer_load(Buffer *b, const char *path);
bool   buffer_save(Buffer *b, const char *path);
size_t buffer_line_count(Buffer *b);
size_t buffer_line_len(Buffer *b, size_t line);
const char *buffer_line_ptr(Buffer *b, size_t line);

void   buffer_insert(Buffer *b, size_t pos, const char *text, size_t len);
void   buffer_delete(Buffer *b, size_t pos, size_t len);
void   buffer_append(Buffer *b, const char *text, size_t len);
size_t buffer_pos_from_row_col(Buffer *b, int row, int col);
void   buffer_row_col_from_pos(Buffer *b, size_t pos, int *row, int *col);

#endif
