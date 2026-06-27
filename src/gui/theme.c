#include "dragon_editor/theme.h"
#include <string.h>

static Theme current_theme = {
    /* ULTRA EVIL: Pure black background with pure bright colors */
    .bg            = {0.00f, 0.00f, 0.00f, 1.0f},        /* Pure black - void */
    .fg            = {1.00f, 0.00f, 0.00f, 1.0f},        /* Pure bright red foreground */
    .gutter_bg     = {0.00f, 0.00f, 0.00f, 1.0f},        /* Pure black gutter - seamless */
    .gutter_fg     = {1.00f, 0.00f, 1.00f, 1.0f},        /* Pure bright magenta - evil! */
    .status_bg     = {0.05f, 0.00f, 0.05f, 1.0f},        /* Deep dark purple status bar */
    .status_fg     = {1.00f, 0.00f, 1.00f, 1.0f},        /* Pure bright magenta status */
    .selection_bg  = {1.00f, 0.00f, 0.50f, 1.0f},        /* Bright magenta/pink selection */
    .cursor_color  = {1.00f, 0.00f, 0.00f, 1.0f},        /* Pure bright red cursor - glowing */
    .line_highlight = {0.20f, 0.00f, 0.00f, 0.6f},       /* Darker red line highlight */
    .menu_bg       = {0.00f, 0.00f, 0.00f, 0.99f},       /* Pure black menu - maximum contrast */
    .menu_fg       = {1.00f, 0.00f, 1.00f, 1.0f},        /* Pure bright magenta menu text */
    .menu_selected = {1.00f, 0.00f, 0.50f, 0.9f},        /* Bright magenta/pink menu selection */
    .accent        = {1.00f, 0.00f, 1.00f, 1.0f},        /* Pure bright magenta accent - EVIL! */
    .error         = {1.00f, 0.00f, 0.00f, 1.0f},        /* Pure bright red errors */
    .warning       = {1.00f, 1.00f, 0.00f, 1.0f},        /* Pure bright yellow warnings */
    .keyword       = {1.00f, 0.00f, 1.00f, 1.0f},        /* Pure bright magenta keywords */
    .string        = {1.00f, 1.00f, 0.00f, 1.0f},        /* Pure bright yellow strings */
    .number        = {0.00f, 1.00f, 1.00f, 1.0f},        /* Pure bright cyan numbers */
    .comment       = {0.50f, 0.00f, 0.50f, 1.0f},        /* Dark purple comments - evil */
    .function_color = {0.00f, 1.00f, 0.00f, 1.0f},       /* Pure bright lime green functions */
    .type_color    = {0.00f, 1.00f, 1.00f, 1.0f},        /* Pure bright cyan types */
    .variable_color = {1.00f, 0.75f, 0.00f, 1.0f},       /* Bright orange variables */
    .macro_color   = {1.00f, 0.00f, 0.75f, 1.0f},        /* Bright magenta/pink macros */
    .operator_color = {1.00f, 1.00f, 0.00f, 1.0f},       /* Bright yellow operators */
    .namespace_color = {0.00f, 1.00f, 0.50f, 1.0f},      /* Bright cyan-green namespaces */
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
