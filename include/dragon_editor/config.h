#ifndef DE_CONFIG_H
#define DE_CONFIG_H

#include <stdint.h>

#define CONFIG_MAX_LANGUAGES 64
#define CONFIG_MAX_EXTENSIONS 16
#define CONFIG_MAX_LSP_ARGS 16
#define CONFIG_MAX_PLUGINS 32
#define CONFIG_MAX_LANGUAGE_WORDS 64

typedef struct {
    char id[64];
    char extensions[CONFIG_MAX_EXTENSIONS][24];
    int extension_count;
    int tab_width;
    int use_tabs;
    char comment_open[24];
    char comment_close[24];
    char line_comment[24];
    int auto_format;
    char tree_sitter[64];
    char tree_sitter_path[256];
    char format_command[256];
    char lsp_command[128];
    char lsp_args[CONFIG_MAX_LSP_ARGS][128];
    int lsp_arg_count;
    int source_plugin;
    char keywords[CONFIG_MAX_LANGUAGE_WORDS][32];
    int keyword_count;
    char type_keywords[CONFIG_MAX_LANGUAGE_WORDS][32];
    int type_keyword_count;
    char macro_keywords[CONFIG_MAX_LANGUAGE_WORDS][32];
    int macro_keyword_count;
} ConfigLanguage;

typedef struct {
    char name[64];
    char path[256];
    char version[32];
    char description[160];
    int enabled;
    int loaded;
    int language_count;
} ConfigPlugin;

typedef struct Config {
    /* Editor settings */
    int tab_width;
    int font_size;
    int line_numbers;
    int line_wrapping;
    char theme_name[64];
    
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
        float type_color[4];
        float variable_color[4];
        float macro_color[4];
        float operator_color[4];
        float namespace_color[4];
    } theme;
    
    /* LSP settings */
    struct {
        int auto_format;
        int auto_hover;
        int diagnostic_delay_ms;
    } lsp;

    ConfigLanguage languages[CONFIG_MAX_LANGUAGES];
    int language_count;

    ConfigPlugin plugins[CONFIG_MAX_PLUGINS];
    int plugin_count;
} Config;

/* Load config from ./dragon.toml, then ~/.config/dragon/dragon.toml */
Config *config_load(void);

/* Get default config (if no file found) */
Config *config_default(void);

/* Free config */
void config_free(Config *cfg);

#endif
