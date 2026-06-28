#include "panel_settings.h"
#include "app.h"
#include "config.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <GLFW/glfw3.h>
#include <stdio.h>

static bool settings_open = false;
static int settings_scroll = 0;

void panel_settings_open(App *app) {
    (void)app;
    settings_open = true;
    settings_scroll = 0;
}

void panel_settings_close(App *app) {
    (void)app;
    settings_open = false;
}

bool panel_settings_is_open(void) {
    return settings_open;
}

void panel_settings_key(App *app, int key) {
    (void)app;
    if (!settings_open) return;
    switch (key) {
    case GLFW_KEY_DOWN:
    case GLFW_KEY_J:
        settings_scroll++;
        break;
    case GLFW_KEY_UP:
    case GLFW_KEY_K:
        if (settings_scroll > 0) settings_scroll--;
        break;
    case GLFW_KEY_PAGE_DOWN:
        settings_scroll += 8;
        break;
    case GLFW_KEY_PAGE_UP:
        settings_scroll -= 8;
        if (settings_scroll < 0) settings_scroll = 0;
        break;
    case GLFW_KEY_HOME:
        settings_scroll = 0;
        break;
    case GLFW_KEY_ESCAPE:
        settings_open = false;
        break;
    default:
        break;
    }
}

static void draw_row(Gui *g, Renderer *r, Theme *t,
                     float x, float y, const char *key, const char *value) {
    font_draw(&g->font, r, key, x, y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    font_draw(&g->font, r, value, x + 230, y,
              t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
}

static void color_value(char *buf, size_t size, const float c[4]) {
    snprintf(buf, size, "%.2f, %.2f, %.2f, %.2f", c[0], c[1], c[2], c[3]);
}

void panel_settings_render(Gui *g, App *app) {
    if (!settings_open) return;

    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    Config *cfg = app_get_config(app);
    int w = app_get_width(app);
    int h = app_get_height(app);

    float pw = 560.0f;
    float ph = (float)h - 100.0f;
    if (ph < 260.0f) ph = 260.0f;
    float px = (float)w / 2.0f - pw / 2.0f;
    float py = 50.0f;

    renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0, 0, 0, 0.35f);
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py + ph - 1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px + pw - 1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);

    font_draw(&g->font, r, "Settings", px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1);

    const char *keys[40];
    char values[40][64];
    int count = 0;

#define ADD_INT(name, value) do { keys[count] = name; snprintf(values[count], sizeof(values[count]), "%d", value); count++; } while (0)
#define ADD_STR(name, value) do { keys[count] = name; snprintf(values[count], sizeof(values[count]), "%s", value); count++; } while (0)
#define ADD_COLOR(name, value) do { keys[count] = name; color_value(values[count], sizeof(values[count]), value); count++; } while (0)

    if (cfg) {
        ADD_INT("editor.tab_width", cfg->tab_width);
        ADD_INT("editor.font_size", cfg->font_size);
        ADD_INT("editor.line_numbers", cfg->line_numbers);
        ADD_INT("editor.line_wrapping", cfg->line_wrapping);
        ADD_STR("editor.theme", cfg->theme_name);
        ADD_INT("lsp.auto_format", cfg->lsp.auto_format);
        ADD_INT("lsp.auto_hover", cfg->lsp.auto_hover);
        ADD_INT("lsp.diagnostic_delay_ms", cfg->lsp.diagnostic_delay_ms);
        ADD_COLOR("theme.bg", cfg->theme.bg);
        ADD_COLOR("theme.fg", cfg->theme.fg);
        ADD_COLOR("theme.gutter_bg", cfg->theme.gutter_bg);
        ADD_COLOR("theme.gutter_fg", cfg->theme.gutter_fg);
        ADD_COLOR("theme.status_bg", cfg->theme.status_bg);
        ADD_COLOR("theme.status_fg", cfg->theme.status_fg);
        ADD_COLOR("theme.selection_bg", cfg->theme.selection_bg);
        ADD_COLOR("theme.cursor_color", cfg->theme.cursor_color);
        ADD_COLOR("theme.line_highlight", cfg->theme.line_highlight);
        ADD_COLOR("theme.menu_bg", cfg->theme.menu_bg);
        ADD_COLOR("theme.menu_fg", cfg->theme.menu_fg);
        ADD_COLOR("theme.menu_selected", cfg->theme.menu_selected);
        ADD_COLOR("theme.accent", cfg->theme.accent);
        ADD_COLOR("theme.error", cfg->theme.error);
        ADD_COLOR("theme.warning", cfg->theme.warning);
        ADD_COLOR("theme.keyword", cfg->theme.keyword);
        ADD_COLOR("theme.string", cfg->theme.string);
        ADD_COLOR("theme.number", cfg->theme.number);
        ADD_COLOR("theme.comment", cfg->theme.comment);
        ADD_COLOR("theme.function_color", cfg->theme.function_color);
        ADD_COLOR("theme.type_color", cfg->theme.type_color);
        ADD_COLOR("theme.variable_color", cfg->theme.variable_color);
        ADD_COLOR("theme.macro_color", cfg->theme.macro_color);
        ADD_COLOR("theme.operator_color", cfg->theme.operator_color);
        ADD_COLOR("theme.namespace_color", cfg->theme.namespace_color);
    }

#undef ADD_INT
#undef ADD_COLOR

    float row_h = g->font.glyph_h + 7.0f;
    float list_y = py + 42.0f;
    int visible = (int)((ph - 76.0f) / row_h);
    int max_scroll = count > visible ? count - visible : 0;
    if (settings_scroll > max_scroll) settings_scroll = max_scroll;
    if (settings_scroll < 0) settings_scroll = 0;

    for (int i = 0; i < visible && i + settings_scroll < count; i++) {
        int idx = i + settings_scroll;
        draw_row(g, r, t, px + 18, list_y + i * row_h, keys[idx], values[idx]);
    }

    float footer_y = py + ph - 24.0f;
    renderer_draw_rect(r, px, footer_y - 4, pw, 1,
                       t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], 0.3f);
    font_draw(&g->font, r, "j/k scroll  Esc close", px + 14, footer_y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
