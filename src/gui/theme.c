#include "dragon_editor/theme.h"

static Theme default_theme = {
    .bg            = {0.00f, 0.00f, 0.00f, 1.0f},
    .fg            = {1.00f, 1.00f, 1.00f, 1.0f},
    .gutter_bg     = {0.04f, 0.04f, 0.04f, 1.0f},
    .gutter_fg     = {0.60f, 0.60f, 0.60f, 1.0f},
    .status_bg     = {0.08f, 0.08f, 0.08f, 1.0f},
    .status_fg     = {1.00f, 1.00f, 1.00f, 1.0f},
    .selection_bg  = {0.20f, 0.35f, 0.60f, 1.0f},
    .cursor_color  = {1.00f, 1.00f, 1.00f, 1.0f},
    .line_highlight = {0.08f, 0.08f, 0.08f, 1.0f},
    .menu_bg       = {0.04f, 0.04f, 0.04f, 0.98f},
    .menu_fg       = {1.00f, 1.00f, 1.00f, 1.0f},
    .menu_selected = {0.15f, 0.30f, 0.60f, 1.0f},
    .accent        = {0.50f, 0.70f, 1.00f, 1.0f},
    .error         = {1.00f, 0.30f, 0.30f, 1.0f},
    .warning       = {1.00f, 0.85f, 0.20f, 1.0f},
    .keyword       = {0.80f, 0.50f, 1.00f, 1.0f},
    .string        = {0.40f, 1.00f, 0.40f, 1.0f},
    .number        = {1.00f, 0.70f, 0.30f, 1.0f},
    .comment       = {0.50f, 0.50f, 0.55f, 1.0f},
    .function_color = {0.50f, 0.80f, 1.00f, 1.0f},
};

Theme *theme_default(void) {
    return &default_theme;
}

Theme *theme_get(void) {
    return &default_theme;
}
