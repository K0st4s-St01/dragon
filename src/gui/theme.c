#include "dragon_editor/theme.h"
#include <string.h>

static Theme current_theme = {
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
};

Theme *theme_default(void) {
    return &current_theme;
}

Theme *theme_get(void) {
    return &current_theme;
}

/* Apply theme from config struct */
void theme_apply_config(const void *config_ptr) {
    if (!config_ptr) return;
    
    /* Forward declare Config struct to avoid circular includes */
    typedef struct {
        int tab_width;
        int font_size;
        int line_numbers;
        int line_wrapping;
        struct {
            float bg[4];
            float fg[4];
            float gutter_bg[4];
            float gutter_fg[4];
            float status_bg[4];
            float status_fg[4];
            float selection_bg[4];
            float cursor_color[4];
            float line_highlight[4];
            float menu_bg[4];
            float menu_fg[4];
            float menu_selected[4];
            float accent[4];
            float error[4];
            float warning[4];
            float keyword[4];
            float string[4];
            float number[4];
            float comment[4];
            float function_color[4];
            float type_color[4];
            float variable_color[4];
            float macro_color[4];
            float operator_color[4];
            float namespace_color[4];
        } theme;
        struct {
            int auto_format;
            int auto_hover;
            int diagnostic_delay_ms;
        } lsp;
    } ConfigProxy;
    
    const ConfigProxy *cfg = (const ConfigProxy *)config_ptr;
    
    /* Copy all colors from config to current theme */
    memcpy(current_theme.bg, cfg->theme.bg, sizeof(float) * 4);
    memcpy(current_theme.fg, cfg->theme.fg, sizeof(float) * 4);
    memcpy(current_theme.gutter_bg, cfg->theme.gutter_bg, sizeof(float) * 4);
    memcpy(current_theme.gutter_fg, cfg->theme.gutter_fg, sizeof(float) * 4);
    memcpy(current_theme.status_bg, cfg->theme.status_bg, sizeof(float) * 4);
    memcpy(current_theme.status_fg, cfg->theme.status_fg, sizeof(float) * 4);
    memcpy(current_theme.selection_bg, cfg->theme.selection_bg, sizeof(float) * 4);
    memcpy(current_theme.cursor_color, cfg->theme.cursor_color, sizeof(float) * 4);
    memcpy(current_theme.line_highlight, cfg->theme.line_highlight, sizeof(float) * 4);
    memcpy(current_theme.menu_bg, cfg->theme.menu_bg, sizeof(float) * 4);
    memcpy(current_theme.menu_fg, cfg->theme.menu_fg, sizeof(float) * 4);
    memcpy(current_theme.menu_selected, cfg->theme.menu_selected, sizeof(float) * 4);
    memcpy(current_theme.accent, cfg->theme.accent, sizeof(float) * 4);
    memcpy(current_theme.error, cfg->theme.error, sizeof(float) * 4);
    memcpy(current_theme.warning, cfg->theme.warning, sizeof(float) * 4);
    memcpy(current_theme.keyword, cfg->theme.keyword, sizeof(float) * 4);
    memcpy(current_theme.string, cfg->theme.string, sizeof(float) * 4);
    memcpy(current_theme.number, cfg->theme.number, sizeof(float) * 4);
     memcpy(current_theme.comment, cfg->theme.comment, sizeof(float) * 4);
     memcpy(current_theme.function_color, cfg->theme.function_color, sizeof(float) * 4);
     memcpy(current_theme.type_color, cfg->theme.type_color, sizeof(float) * 4);
     memcpy(current_theme.variable_color, cfg->theme.variable_color, sizeof(float) * 4);
     memcpy(current_theme.macro_color, cfg->theme.macro_color, sizeof(float) * 4);
     memcpy(current_theme.operator_color, cfg->theme.operator_color, sizeof(float) * 4);
     memcpy(current_theme.namespace_color, cfg->theme.namespace_color, sizeof(float) * 4);
}
