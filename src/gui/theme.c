#include "dragon_editor/theme.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    Theme theme;
} NamedTheme;

static const NamedTheme builtin_themes[] = {
    {
        "dragon",
        {
            .bg              = {0.045f, 0.050f, 0.060f, 1.0f},
            .fg              = {0.82f, 0.84f, 0.86f, 1.0f},
            .gutter_bg       = {0.035f, 0.040f, 0.050f, 1.0f},
            .gutter_fg       = {0.38f, 0.42f, 0.48f, 1.0f},
            .status_bg       = {0.030f, 0.035f, 0.045f, 1.0f},
            .status_fg       = {0.74f, 0.78f, 0.82f, 1.0f},
            .selection_bg    = {0.18f, 0.28f, 0.40f, 0.88f},
            .cursor_color    = {0.95f, 0.76f, 0.32f, 1.0f},
            .line_highlight  = {0.085f, 0.095f, 0.115f, 0.86f},
            .menu_bg         = {0.055f, 0.060f, 0.075f, 0.98f},
            .menu_fg         = {0.82f, 0.84f, 0.86f, 1.0f},
            .menu_selected   = {0.16f, 0.25f, 0.35f, 0.92f},
            .accent          = {0.35f, 0.68f, 0.78f, 1.0f},
            .error           = {0.95f, 0.35f, 0.34f, 1.0f},
            .warning         = {0.92f, 0.72f, 0.36f, 1.0f},
            .keyword         = {0.68f, 0.56f, 0.88f, 1.0f},
            .string          = {0.58f, 0.74f, 0.45f, 1.0f},
            .number          = {0.84f, 0.62f, 0.44f, 1.0f},
            .comment         = {0.40f, 0.46f, 0.52f, 1.0f},
            .function_color  = {0.52f, 0.74f, 0.90f, 1.0f},
            .type_color      = {0.42f, 0.72f, 0.68f, 1.0f},
            .variable_color  = {0.80f, 0.82f, 0.84f, 1.0f},
            .macro_color     = {0.82f, 0.58f, 0.72f, 1.0f},
            .operator_color  = {0.78f, 0.78f, 0.70f, 1.0f},
            .namespace_color = {0.48f, 0.70f, 0.78f, 1.0f},
        }
    },
    {
        "ember",
        {
            .bg              = {0.060f, 0.047f, 0.042f, 1.0f},
            .fg              = {0.90f, 0.84f, 0.76f, 1.0f},
            .gutter_bg       = {0.042f, 0.034f, 0.032f, 1.0f},
            .gutter_fg       = {0.47f, 0.39f, 0.34f, 1.0f},
            .status_bg       = {0.040f, 0.030f, 0.028f, 1.0f},
            .status_fg       = {0.86f, 0.76f, 0.66f, 1.0f},
            .selection_bg    = {0.38f, 0.20f, 0.13f, 0.88f},
            .cursor_color    = {1.00f, 0.72f, 0.28f, 1.0f},
            .line_highlight  = {0.105f, 0.075f, 0.062f, 0.90f},
            .menu_bg         = {0.080f, 0.058f, 0.050f, 0.98f},
            .menu_fg         = {0.90f, 0.84f, 0.76f, 1.0f},
            .menu_selected   = {0.34f, 0.17f, 0.11f, 0.92f},
            .accent          = {0.95f, 0.47f, 0.22f, 1.0f},
            .error           = {1.00f, 0.25f, 0.20f, 1.0f},
            .warning         = {1.00f, 0.73f, 0.25f, 1.0f},
            .keyword         = {0.95f, 0.42f, 0.34f, 1.0f},
            .string          = {0.78f, 0.70f, 0.38f, 1.0f},
            .number          = {0.98f, 0.65f, 0.38f, 1.0f},
            .comment         = {0.55f, 0.43f, 0.36f, 1.0f},
            .function_color  = {0.98f, 0.72f, 0.42f, 1.0f},
            .type_color      = {0.84f, 0.52f, 0.44f, 1.0f},
            .variable_color  = {0.92f, 0.84f, 0.76f, 1.0f},
            .macro_color     = {1.00f, 0.50f, 0.30f, 1.0f},
            .operator_color  = {0.94f, 0.78f, 0.58f, 1.0f},
            .namespace_color = {0.88f, 0.58f, 0.42f, 1.0f},
        }
    },
    {
        "glacier",
        {
            .bg              = {0.040f, 0.052f, 0.064f, 1.0f},
            .fg              = {0.82f, 0.90f, 0.92f, 1.0f},
            .gutter_bg       = {0.030f, 0.040f, 0.050f, 1.0f},
            .gutter_fg       = {0.38f, 0.50f, 0.56f, 1.0f},
            .status_bg       = {0.028f, 0.036f, 0.044f, 1.0f},
            .status_fg       = {0.72f, 0.84f, 0.88f, 1.0f},
            .selection_bg    = {0.12f, 0.34f, 0.42f, 0.88f},
            .cursor_color    = {0.55f, 0.95f, 0.94f, 1.0f},
            .line_highlight  = {0.065f, 0.085f, 0.100f, 0.90f},
            .menu_bg         = {0.048f, 0.065f, 0.078f, 0.98f},
            .menu_fg         = {0.82f, 0.90f, 0.92f, 1.0f},
            .menu_selected   = {0.10f, 0.30f, 0.38f, 0.92f},
            .accent          = {0.38f, 0.82f, 0.92f, 1.0f},
            .error           = {0.98f, 0.34f, 0.38f, 1.0f},
            .warning         = {0.94f, 0.74f, 0.32f, 1.0f},
            .keyword         = {0.52f, 0.72f, 0.98f, 1.0f},
            .string          = {0.54f, 0.84f, 0.72f, 1.0f},
            .number          = {0.72f, 0.78f, 1.00f, 1.0f},
            .comment         = {0.42f, 0.54f, 0.60f, 1.0f},
            .function_color  = {0.45f, 0.88f, 0.95f, 1.0f},
            .type_color      = {0.50f, 0.80f, 0.88f, 1.0f},
            .variable_color  = {0.82f, 0.90f, 0.92f, 1.0f},
            .macro_color     = {0.70f, 0.64f, 0.95f, 1.0f},
            .operator_color  = {0.78f, 0.88f, 0.90f, 1.0f},
            .namespace_color = {0.42f, 0.75f, 0.92f, 1.0f},
        }
    },
    {
        "black+",
        {
            .bg              = {0.000f, 0.000f, 0.000f, 1.0f},
            .fg              = {1.000f, 1.000f, 1.000f, 1.0f},
            .gutter_bg       = {0.000f, 0.000f, 0.000f, 1.0f},
            .gutter_fg       = {0.740f, 0.740f, 0.740f, 1.0f},
            .status_bg       = {0.000f, 0.000f, 0.000f, 1.0f},
            .status_fg       = {1.000f, 1.000f, 1.000f, 1.0f},
            .selection_bg    = {1.000f, 1.000f, 1.000f, 0.30f},
            .cursor_color    = {1.000f, 1.000f, 1.000f, 1.0f},
            .line_highlight  = {0.080f, 0.080f, 0.080f, 1.0f},
            .menu_bg         = {0.000f, 0.000f, 0.000f, 0.98f},
            .menu_fg         = {1.000f, 1.000f, 1.000f, 1.0f},
            .menu_selected   = {0.180f, 0.180f, 0.180f, 1.0f},
            .accent          = {1.000f, 0.000f, 0.000f, 1.0f},
            .error           = {1.000f, 0.000f, 0.000f, 1.0f},
            .warning         = {1.000f, 1.000f, 0.000f, 1.0f},
            .keyword         = {1.000f, 0.000f, 0.000f, 1.0f},
            .string          = {0.000f, 1.000f, 0.000f, 1.0f},
            .number          = {1.000f, 1.000f, 0.000f, 1.0f},
            .comment         = {0.680f, 0.680f, 0.680f, 1.0f},
            .function_color  = {0.000f, 0.900f, 1.000f, 1.0f},
            .type_color      = {1.000f, 0.000f, 1.000f, 1.0f},
            .variable_color  = {1.000f, 1.000f, 1.000f, 1.0f},
            .macro_color     = {1.000f, 0.450f, 0.000f, 1.0f},
            .operator_color  = {1.000f, 1.000f, 1.000f, 1.0f},
            .namespace_color = {0.000f, 1.000f, 1.000f, 1.0f},
        }
    },
};

static Theme current_theme;
static char current_theme_name[64] = "dragon";
static bool current_theme_ready = false;

static int theme_name_cmp(const char *a, const char *b) {
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb))
            return (int)tolower(ca) - (int)tolower(cb);
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static void theme_copy(Theme *dst, const Theme *src) {
    if (dst && src)
        memcpy(dst, src, sizeof(*dst));
}

Theme *theme_default(void) {
    if (!current_theme_ready)
        theme_apply_named("dragon");
    return &current_theme;
}

Theme *theme_get(void) {
    if (!current_theme_ready)
        theme_apply_named("dragon");
    return &current_theme;
}

const char *theme_current_name(void) {
    return current_theme_name;
}

bool theme_get_named(const char *name, Theme *out) {
    if (!name || !*name || !out) return false;
    int count = (int)(sizeof(builtin_themes) / sizeof(builtin_themes[0]));
    for (int i = 0; i < count; i++) {
        if (theme_name_cmp(name, builtin_themes[i].name) == 0) {
            theme_copy(out, &builtin_themes[i].theme);
            return true;
        }
    }
    return false;
}

bool theme_apply_named(const char *name) {
    if (!name || !*name) return false;
    int count = (int)(sizeof(builtin_themes) / sizeof(builtin_themes[0]));
    for (int i = 0; i < count; i++) {
        if (theme_name_cmp(name, builtin_themes[i].name) == 0) {
            theme_copy(&current_theme, &builtin_themes[i].theme);
            snprintf(current_theme_name, sizeof(current_theme_name), "%s", builtin_themes[i].name);
            current_theme_ready = true;
            return true;
        }
    }
    return false;
}

int theme_list_names(const char **names, int max_names) {
    int count = (int)(sizeof(builtin_themes) / sizeof(builtin_themes[0]));
    if (!names || max_names <= 0)
        return count;
    int copied = count < max_names ? count : max_names;
    for (int i = 0; i < copied; i++)
        names[i] = builtin_themes[i].name;
    return count;
}

void theme_apply_config(const Config *cfg) {
    if (!cfg) return;
    memcpy(&current_theme, &cfg->theme, sizeof(current_theme));
    snprintf(current_theme_name, sizeof(current_theme_name), "%s",
             cfg->theme_name[0] ? cfg->theme_name : "custom");
    current_theme_ready = true;
}
