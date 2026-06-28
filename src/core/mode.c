#include "dragon_editor/mode.h"
#include <string.h>

void mode_init(ModeState *ms) {
    memset(ms, 0, sizeof(*ms));
    ms->current = MODE_NORMAL;
    ms->previous = MODE_NORMAL;
}

void mode_set(ModeState *ms, Mode m) {
    bool changed = (m != ms->current);
    ms->previous = ms->current;
    ms->current = m;
    
    /* Reset select mode initialization flag when changing modes */
    if (m != MODE_SELECT) {
        ms->select_mode_initialized = false;
    }

    if (changed) {
        ms->g_pending = false;
        ms->pending_key = 0;
        memset(ms->pending_keys, 0, sizeof(ms->pending_keys));
        ms->pending_len = 0;
        ms->count = 0;
        ms->pending_operator = 0;
        ms->pending_text_obj = 0;
        ms->view_mode_sticky = false;
    }
    
    /* Suppress next character if mode just changed */
    if (changed) {
        ms->suppress_next_char = true;
    }
}

Mode mode_get(ModeState *ms) {
    return ms->current;
}

bool mode_is(ModeState *ms, Mode m) {
    return ms->current == m;
}
