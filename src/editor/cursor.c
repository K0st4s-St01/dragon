#include "dragon_editor/cursor.h"
#include <string.h>

void cursor_init(Cursor *c) {
    memset(c, 0, sizeof(*c));
}

void cursor_set(Cursor *c, int row, int col) {
    c->row = row;
    c->col = col;
    c->desired_col = col;  /* Update desired column when explicitly set */
}

void cursor_move(Cursor *c, int dr, int dc) {
    if (dr != 0) {
        /* Vertical movement: use desired column if moving up/down */
        c->row += dr;
        /* Restore desired column on vertical movement */
        c->col = c->desired_col;
    } else if (dc != 0) {
        /* Horizontal movement: update desired column */
        c->col += dc;
        c->desired_col = c->col;
    }
    
    if (c->row < 0) c->row = 0;
    if (c->col < 0) c->col = 0;
}

void cursor_move_to(Cursor *c, int row, int col) {
    c->row = row;
    c->col = col;
    c->desired_col = col;  /* Update desired column when moving to explicit position */
    if (c->row < 0) c->row = 0;
    if (c->col < 0) c->col = 0;
}

void cursor_select_start(Cursor *c) {
    c->anchor_row = c->row;
    c->anchor_col = c->col;
    c->has_selection = true;
}

void cursor_select_end(Cursor *c) {
    (void)c;
}

void cursor_clear_selection(Cursor *c) {
    c->has_selection = false;
    c->anchor_row = c->row;
    c->anchor_col = c->col;
}

bool cursor_has_selection(Cursor *c) {
    return c->has_selection;
}

void cursor_normalize(Cursor *c, int *sr, int *sc, int *er, int *ec) {
    if (c->anchor_row < c->row ||
        (c->anchor_row == c->row && c->anchor_col <= c->col)) {
        *sr = c->anchor_row;
        *sc = c->anchor_col;
        *er = c->row;
        *ec = c->col;
    } else {
        *sr = c->row;
        *sc = c->col;
        *er = c->anchor_row;
        *ec = c->anchor_col;
    }
}
