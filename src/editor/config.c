#include "dragon_editor/config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include "../vendor/tomlc99/toml.h"

/* Helper to parse color arrays from TOML table */
static void parse_color(toml_table_t *tbl, const char *key, float color[4]) {
    color[0] = color[1] = color[2] = 1.0f;
    color[3] = 1.0f;
    
    toml_array_t *arr = toml_array_in(tbl, key);
    if (!arr) return;
    
    for (int i = 0; i < 4 && i < toml_array_nelem(arr); i++) {
        double d = toml_double_at(arr, i).ok ? toml_double_at(arr, i).u.d : 1.0;
        color[i] = (float)d;
    }
}

static int parse_int(toml_table_t *tbl, const char *key, int default_val) {
    toml_datum_t d = toml_int_in(tbl, key);
    return d.ok ? (int)d.u.i : default_val;
}

static void set_color(float color[4], float r, float g, float b, float a) {
    color[0] = r;
    color[1] = g;
    color[2] = b;
    color[3] = a;
}

Config *config_default(void) {
    Config *cfg = calloc(1, sizeof(Config));
    
    /* Editor defaults */
    cfg->tab_width = 4;
    cfg->font_size = 14;
    cfg->line_numbers = 1;
    cfg->line_wrapping = 0;
    
    /* Theme defaults */
    set_color(cfg->theme.bg,              0.045f, 0.050f, 0.060f, 1.0f);
    set_color(cfg->theme.fg,              0.82f,  0.84f,  0.86f,  1.0f);
    set_color(cfg->theme.gutter_bg,       0.035f, 0.040f, 0.050f, 1.0f);
    set_color(cfg->theme.gutter_fg,       0.38f,  0.42f,  0.48f,  1.0f);
    set_color(cfg->theme.status_bg,       0.030f, 0.035f, 0.045f, 1.0f);
    set_color(cfg->theme.status_fg,       0.74f,  0.78f,  0.82f,  1.0f);
    set_color(cfg->theme.selection_bg,    0.18f,  0.28f,  0.40f,  0.88f);
    set_color(cfg->theme.cursor_color,    0.95f,  0.76f,  0.32f,  1.0f);
    set_color(cfg->theme.line_highlight,  0.085f, 0.095f, 0.115f, 0.86f);
    set_color(cfg->theme.menu_bg,         0.055f, 0.060f, 0.075f, 0.98f);
    set_color(cfg->theme.menu_fg,         0.82f,  0.84f,  0.86f,  1.0f);
    set_color(cfg->theme.menu_selected,   0.16f,  0.25f,  0.35f,  0.92f);
    set_color(cfg->theme.accent,          0.35f,  0.68f,  0.78f,  1.0f);
    set_color(cfg->theme.error,           0.95f,  0.35f,  0.34f,  1.0f);
    set_color(cfg->theme.warning,         0.92f,  0.72f,  0.36f,  1.0f);
    set_color(cfg->theme.keyword,         0.68f,  0.56f,  0.88f,  1.0f);
    set_color(cfg->theme.string,          0.58f,  0.74f,  0.45f,  1.0f);
    set_color(cfg->theme.number,          0.84f,  0.62f,  0.44f,  1.0f);
    set_color(cfg->theme.comment,         0.40f,  0.46f,  0.52f,  1.0f);
    set_color(cfg->theme.function_color,  0.52f,  0.74f,  0.90f,  1.0f);
    set_color(cfg->theme.type_color,      0.42f,  0.72f,  0.68f,  1.0f);
    set_color(cfg->theme.variable_color,  0.80f,  0.82f,  0.84f,  1.0f);
    set_color(cfg->theme.macro_color,     0.82f,  0.58f,  0.72f,  1.0f);
    set_color(cfg->theme.operator_color,  0.78f,  0.78f,  0.70f,  1.0f);
    set_color(cfg->theme.namespace_color, 0.48f,  0.70f,  0.78f,  1.0f);
    
    /* LSP defaults */
    cfg->lsp.auto_format = 0;
    cfg->lsp.auto_hover = 1;
    cfg->lsp.diagnostic_delay_ms = 100;
    
    return cfg;
}

Config *config_load(void) {
    Config *cfg = config_default();
    
    /* Get home directory */
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (!pw) return cfg;
        home = pw->pw_dir;
    }
    
    /* Build config file path */
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/dragon/dragon.toml", home);
    
    /* Try to open and parse config file */
    FILE *f = fopen(path, "r");
    if (!f) {
        /* Config file doesn't exist, use defaults */
        fprintf(stderr, "Config: Using defaults (no config file found at %s)\n", path);
        return cfg;
    }
    
    fprintf(stderr, "Config: Loading from %s\n", path);
    
    char errbuf[200];
    toml_table_t *conf = toml_parse_file(f, errbuf, sizeof(errbuf));
    fclose(f);
    
    if (!conf) {
        fprintf(stderr, "Config: Parse error: %s\n", errbuf);
        return cfg;
    }
    
    /* Parse editor section */
    toml_table_t *editor = toml_table_in(conf, "editor");
    if (editor) {
        cfg->tab_width = parse_int(editor, "tab_width", cfg->tab_width);
        if (cfg->tab_width <= 0) cfg->tab_width = 4;
        cfg->font_size = parse_int(editor, "font_size", cfg->font_size);
        cfg->line_numbers = parse_int(editor, "line_numbers", cfg->line_numbers);
        cfg->line_wrapping = parse_int(editor, "line_wrapping", cfg->line_wrapping);
        fprintf(stderr, "Config: Editor settings loaded (tab_width=%d, font_size=%d)\n", 
                cfg->tab_width, cfg->font_size);
    }
    
    /* Parse theme section */
    toml_table_t *theme = toml_table_in(conf, "theme");
    if (theme) {
        parse_color(theme, "bg", cfg->theme.bg);
        parse_color(theme, "fg", cfg->theme.fg);
        parse_color(theme, "gutter_bg", cfg->theme.gutter_bg);
        parse_color(theme, "gutter_fg", cfg->theme.gutter_fg);
        parse_color(theme, "status_bg", cfg->theme.status_bg);
        parse_color(theme, "status_fg", cfg->theme.status_fg);
        parse_color(theme, "selection_bg", cfg->theme.selection_bg);
        parse_color(theme, "cursor_color", cfg->theme.cursor_color);
        parse_color(theme, "line_highlight", cfg->theme.line_highlight);
        parse_color(theme, "menu_bg", cfg->theme.menu_bg);
        parse_color(theme, "menu_fg", cfg->theme.menu_fg);
        parse_color(theme, "menu_selected", cfg->theme.menu_selected);
        parse_color(theme, "accent", cfg->theme.accent);
        parse_color(theme, "error", cfg->theme.error);
        parse_color(theme, "warning", cfg->theme.warning);
        parse_color(theme, "keyword", cfg->theme.keyword);
        parse_color(theme, "string", cfg->theme.string);
        parse_color(theme, "number", cfg->theme.number);
         parse_color(theme, "comment", cfg->theme.comment);
         parse_color(theme, "function_color", cfg->theme.function_color);
         parse_color(theme, "type_color", cfg->theme.type_color);
         parse_color(theme, "variable_color", cfg->theme.variable_color);
         parse_color(theme, "macro_color", cfg->theme.macro_color);
         parse_color(theme, "operator_color", cfg->theme.operator_color);
         parse_color(theme, "namespace_color", cfg->theme.namespace_color);
         fprintf(stderr, "Config: Theme colors loaded from config file\n");
    }
    
    /* Parse lsp section */
    toml_table_t *lsp = toml_table_in(conf, "lsp");
    if (lsp) {
        cfg->lsp.auto_format = parse_int(lsp, "auto_format", cfg->lsp.auto_format);
        cfg->lsp.auto_hover = parse_int(lsp, "auto_hover", cfg->lsp.auto_hover);
        cfg->lsp.diagnostic_delay_ms = parse_int(lsp, "diagnostic_delay_ms", cfg->lsp.diagnostic_delay_ms);
        fprintf(stderr, "Config: LSP settings loaded\n");
    }
    
    toml_free(conf);
    return cfg;
}

void config_free(Config *cfg) {
    if (cfg) free(cfg);
}
