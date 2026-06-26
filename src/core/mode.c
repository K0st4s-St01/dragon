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
}

Mode mode_get(ModeState *ms) {
    return ms->current;
}

bool mode_is(ModeState *ms, Mode m) {
    return ms->current == m;
}
