#ifndef DE_MODE_H
#define DE_MODE_H

#include <stdbool.h>

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_SELECT,
    MODE_VIEW,
    MODE_COMMAND_PALETTE,
    MODE_COMMAND,
    MODE_GOTO,
    MODE_FIND,
    MODE_SEARCH,
    MODE_EXTEND,
    MODE_UNDO,
    MODE_MACRO,
} Mode;

typedef struct {
    Mode  current;
    Mode  previous;
    bool  g_pending;
    char  pending_key;
    char  pending_keys[8];
    int   pending_len;
    int   count;
    char  last_insert_text[4096];
    int   last_insert_len;
    int   last_insert_cursor_delta;
    bool  has_last_insert;
} ModeState;

void mode_init(ModeState *ms);
void mode_set(ModeState *ms, Mode m);
Mode mode_get(ModeState *ms);
bool mode_is(ModeState *ms, Mode m);

#endif
