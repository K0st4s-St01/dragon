#ifndef DE_CONFIG_H
#define DE_CONFIG_H

#include <stdint.h>

typedef struct {
    /* Editor settings */
    int tab_width;
    int font_size;
    int line_numbers;
    int line_wrapping;
    
    /* Theme colors (RGBA, 0.0-1.0) */
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
    } theme;
    
    /* LSP settings */
    struct {
        int auto_format;
        int auto_hover;
        int diagnostic_delay_ms;
    } lsp;
} Config;

/* Load config from ~/.config/dragon/dragon.toml */
Config *config_load(void);

/* Get default config (if no file found) */
Config *config_default(void);

/* Free config */
void config_free(Config *cfg);

#endif
