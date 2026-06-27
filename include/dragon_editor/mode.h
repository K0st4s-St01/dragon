#ifndef DE_MODE_H
#define DE_MODE_H

#include <stdbool.h>

typedef enum {
    MODE_NORMAL,
    MODE_INSERT,
    MODE_SELECT,
    MODE_VIEW,
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
    /* Last motion tracking for Alt-. */
    char  last_motion_type;  /* 'f', 'F', 't', 'T', or 0 */
    char  last_motion_char;  /* character used in last motion */
    bool  view_mode_sticky;  /* Z - sticky view mode stays active */
    bool  select_mode_initialized;  /* True if selection was started when entering select mode */
    bool  suppress_next_char;  /* Suppress next character input if mode changed this frame */
} ModeState;

void mode_init(ModeState *ms);
void mode_set(ModeState *ms, Mode m);
Mode mode_get(ModeState *ms);
bool mode_is(ModeState *ms, Mode m);

#endif
