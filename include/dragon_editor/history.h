#ifndef DE_HISTORY_H
#define DE_HISTORY_H

#include <stddef.h>
#include <stdbool.h>
#include "cursor.h"

typedef enum {
    OP_INSERT,
    OP_DELETE,
} OpType;

#define HISTORY_CURSOR_MAX 64

typedef struct {
    OpType  type;
    size_t  pos;       /* Byte offset in buffer */
    char   *text;      /* Inserted text (OP_INSERT) or deleted text (OP_DELETE) */
    size_t  len;       /* Length of text */
    int     cursor_row;
    int     cursor_col;
    int     group;
    Cursor  before_cursors[HISTORY_CURSOR_MAX];
    Cursor  after_cursors[HISTORY_CURSOR_MAX];
    int     before_cursor_count;
    int     after_cursor_count;
} HistoryEntry;

#define HISTORY_MAX 1024

typedef struct {
    HistoryEntry entries[HISTORY_MAX];
    int count;
    int current;       /* Index of the next entry to redo (-1 = nothing to redo) */
    int next_group;
    int active_group;
} History;

void history_init(History *h);
void history_free(History *h);
void history_clear(History *h);
void history_begin_group(History *h);
void history_end_group(History *h);
void history_push_insert(History *h, size_t pos, const char *text, size_t len,
                         int cursor_row, int cursor_col);
void history_push_delete(History *h, size_t pos, const char *text, size_t len,
                         int cursor_row, int cursor_col);
bool history_can_undo(History *h);
bool history_can_redo(History *h);
HistoryEntry *history_peek_undo(History *h);
HistoryEntry *history_peek_redo(History *h);
void history_advance(History *h);
void history_regress(History *h);

#endif
