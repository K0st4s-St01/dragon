#include "dragon_editor/mode.h"
#include <string.h>

void mode_init(ModeState *ms) {
    memset(ms, 0, sizeof(*ms));
    ms->current = MODE_NORMAL;
    ms->previous = MODE_NORMAL;
}

void mode_set(ModeState *ms, Mode m) {
    ms->previous = ms->current;
    ms->current = m;
    
    /* Reset select mode initialization flag when changing modes */
    if (m != MODE_SELECT) {
        ms->select_mode_initialized = false;
    }
    
    /* Suppress next character if mode just changed */
    if (m != ms->previous) {
        ms->suppress_next_char = true;
    }
}

Mode mode_get(ModeState *ms) {
    return ms->current;
}

bool mode_is(ModeState *ms, Mode m) {
    return ms->current == m;
}
