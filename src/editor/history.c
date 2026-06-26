#include "dragon_editor/history.h"
#include <stdlib.h>
#include <string.h>

void history_init(History *h) {
    memset(h, 0, sizeof(*h));
    h->current = -1;
}

void history_free(History *h) {
    for (int i = 0; i < h->count; i++)
        free(h->entries[i].text);
    memset(h, 0, sizeof(*h));
    h->current = -1;
}

void history_clear(History *h) {
    for (int i = 0; i < h->count; i++)
        free(h->entries[i].text);
    h->count = 0;
    h->current = -1;
}

static void push_entry(History *h, OpType type, size_t pos, const char *text,
                       size_t len, int cursor_row, int cursor_col) {
    /* Discard any redo entries beyond current */
    for (int i = h->current + 1; i < h->count; i++)
        free(h->entries[i].text);
    h->count = h->current + 1;

    if (h->count >= HISTORY_MAX) {
        free(h->entries[0].text);
        memmove(&h->entries[0], &h->entries[1],
                sizeof(HistoryEntry) * (HISTORY_MAX - 1));
        h->count = HISTORY_MAX - 1;
        h->current--;
    }

    HistoryEntry *e = &h->entries[h->count];
    e->type = type;
    e->pos = pos;
    e->text = malloc(len);
    memcpy(e->text, text, len);
    e->len = len;
    e->cursor_row = cursor_row;
    e->cursor_col = cursor_col;
    h->count++;
    h->current = h->count - 1;
}

void history_push_insert(History *h, size_t pos, const char *text, size_t len,
                         int cursor_row, int cursor_col) {
    push_entry(h, OP_INSERT, pos, text, len, cursor_row, cursor_col);
}

void history_push_delete(History *h, size_t pos, const char *text, size_t len,
                         int cursor_row, int cursor_col) {
    push_entry(h, OP_DELETE, pos, text, len, cursor_row, cursor_col);
}

bool history_can_undo(History *h) {
    return h->current >= 0;
}

bool history_can_redo(History *h) {
    return h->current < h->count - 1;
}

HistoryEntry *history_peek_undo(History *h) {
    if (!history_can_undo(h)) return NULL;
    return &h->entries[h->current];
}

HistoryEntry *history_peek_redo(History *h) {
    if (!history_can_redo(h)) return NULL;
    return &h->entries[h->current + 1];
}

void history_advance(History *h) {
    if (h->current < h->count - 1)
        h->current++;
}

void history_regress(History *h) {
    if (h->current >= 0)
        h->current--;
}
