#ifndef DE_CURSOR_H
#define DE_CURSOR_H

#include <stdbool.h>

typedef struct {
    int row, col;
    int anchor_row, anchor_col;
    bool has_selection;
} Cursor;

void cursor_init(Cursor *c);
void cursor_set(Cursor *c, int row, int col);
void cursor_move(Cursor *c, int dr, int dc);
void cursor_move_to(Cursor *c, int row, int col);
void cursor_select_start(Cursor *c);
void cursor_select_end(Cursor *c);
void cursor_clear_selection(Cursor *c);
bool cursor_has_selection(Cursor *c);
void cursor_normalize(Cursor *c, int *start_row, int *start_col,
                       int *end_row, int *end_col);

#endif
