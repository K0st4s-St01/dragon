#include "panel_plugins.h"
#include "app.h"
#include "config.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

static bool plugins_open = false;
static int plugins_selected = 0;
static int plugins_scroll = 0;

void panel_plugins_open(App *app) {
    (void)app;
    plugins_open = true;
    plugins_selected = 0;
    plugins_scroll = 0;
}

void panel_plugins_close(App *app) {
    (void)app;
    plugins_open = false;
}

bool panel_plugins_is_open(void) {
    return plugins_open;
}

static void plugins_draw_fit(Gui *g, Renderer *r, const char *text,
                             float x, float right, float y,
                             float cr, float cg, float cb, float ca) {
    if (!text || !*text || right <= x) return;
    char clipped[512];
    size_t copy = strlen(text);
    if (copy >= sizeof(clipped)) copy = sizeof(clipped) - 1;
    memcpy(clipped, text, copy);
    clipped[copy] = '\0';
    size_t len = strlen(clipped);
    while (len > 4 && x + font_text_width(&g->font, clipped) > right) {
        clipped[--len] = '\0';
        if (len > 3) {
            clipped[len - 3] = '.';
            clipped[len - 2] = '.';
            clipped[len - 1] = '.';
        }
    }
    font_draw(&g->font, r, clipped, x, y, cr, cg, cb, ca);
}

void panel_plugins_key(App *app, int key) {
    if (!plugins_open) return;
    Config *cfg = app_get_config(app);
    int count = cfg ? cfg->plugin_count : 0;
    switch (key) {
    case GLFW_KEY_ESCAPE:
        plugins_open = false;
        break;
    case GLFW_KEY_DOWN:
    case GLFW_KEY_J:
        if (plugins_selected < count - 1) plugins_selected++;
        break;
    case GLFW_KEY_UP:
    case GLFW_KEY_K:
        if (plugins_selected > 0) plugins_selected--;
        break;
    case GLFW_KEY_PAGE_DOWN:
        plugins_selected += 8;
        if (plugins_selected >= count) plugins_selected = count > 0 ? count - 1 : 0;
        break;
    case GLFW_KEY_PAGE_UP:
        plugins_selected -= 8;
        if (plugins_selected < 0) plugins_selected = 0;
        break;
    case GLFW_KEY_HOME:
        plugins_selected = 0;
        break;
    case GLFW_KEY_END:
        plugins_selected = count > 0 ? count - 1 : 0;
        break;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_SPACE:
        if (cfg && plugins_selected >= 0 && plugins_selected < count) {
            bool enabled = cfg->plugins[plugins_selected].enabled ? false : true;
            app_set_plugin_enabled(app, plugins_selected, enabled);
        }
        break;
    case GLFW_KEY_R:
        app_reload_config(app);
        cfg = app_get_config(app);
        count = cfg ? cfg->plugin_count : 0;
        if (plugins_selected >= count) plugins_selected = count > 0 ? count - 1 : 0;
        if (plugins_selected < 0) plugins_selected = 0;
        plugins_scroll = 0;
        break;
    default:
        break;
    }
}

void panel_plugins_render(Gui *g, App *app) {
    if (!plugins_open) return;

    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    Config *cfg = app_get_config(app);
    int w = app_get_width(app);
    int h = app_get_height(app);
    int count = cfg ? cfg->plugin_count : 0;

    float pw = (float)w * 0.58f;
    if (pw < 620.0f) pw = 620.0f;
    if (pw > 860.0f) pw = 860.0f;
    if (pw > (float)w - 48.0f) pw = (float)w - 48.0f;
    float ph = (float)h - 120.0f;
    if (ph < 280.0f) ph = 280.0f;
    float px = (float)w * 0.5f - pw * 0.5f;
    float py = 56.0f;
    float row_h = g->font.glyph_h + 12.0f;

    renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0, 0, 0, 0.35f);
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 2, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py + 40.0f, pw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.35f);

    font_draw(&g->font, r, "Plugins", px + 14.0f, py + 12.0f,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);

    char meta[96];
    snprintf(meta, sizeof(meta), "%d configured", count);
    float meta_w = font_text_width(&g->font, meta);
    font_draw(&g->font, r, meta, px + pw - meta_w - 14.0f, py + 12.0f,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);

    renderer_draw_rect(r, px, py + ph - 29.0f, pw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.25f);
    const char *footer = "j/k move  space toggle+save  r reload config  esc close";
    plugins_draw_fit(g, r, footer, px + 14.0f, px + pw - 14.0f, py + ph - 24.0f,
                     t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);

    float list_y = py + 50.0f;
    int visible = (int)((ph - 92.0f) / row_h);
    if (visible < 1) visible = 1;
    if (plugins_selected < plugins_scroll) plugins_scroll = plugins_selected;
    if (plugins_selected >= plugins_scroll + visible) plugins_scroll = plugins_selected - visible + 1;
    if (plugins_scroll < 0) plugins_scroll = 0;

    if (count == 0) {
        font_draw(&g->font, r, "No plugins configured", px + 14.0f, list_y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
        return;
    }

    for (int row = 0; row < visible && plugins_scroll + row < count; row++) {
        int i = plugins_scroll + row;
        ConfigPlugin *plugin = &cfg->plugins[i];
        float y = list_y + (float)row * row_h;
        if (i == plugins_selected) {
            renderer_draw_rect(r, px + 8.0f, y - 4.0f, pw - 16.0f, row_h,
                               t->menu_selected[0], t->menu_selected[1],
                               t->menu_selected[2], t->menu_selected[3]);
        }

        const char *state = !plugin->enabled ? "disabled" : (plugin->loaded ? "loaded" : "configured");
        float state_r = plugin->loaded ? t->string[0] : (!plugin->enabled ? t->gutter_fg[0] : t->warning[0]);
        float state_g = plugin->loaded ? t->string[1] : (!plugin->enabled ? t->gutter_fg[1] : t->warning[1]);
        float state_b = plugin->loaded ? t->string[2] : (!plugin->enabled ? t->gutter_fg[2] : t->warning[2]);

        float state_x = px + pw - 112.0f;
        plugins_draw_fit(g, r, plugin->name, px + 18.0f, state_x - 16.0f, y,
                         t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        plugins_draw_fit(g, r, state, state_x, px + pw - 18.0f, y,
                         state_r, state_g, state_b, 1.0f);

        char detail[256];
        if (plugin->description[0])
            snprintf(detail, sizeof(detail), "%s", plugin->description);
        else if (plugin->path[0])
            snprintf(detail, sizeof(detail), "%s", plugin->path);
        else
            snprintf(detail, sizeof(detail), "inline config");
        float detail_right = plugin->language_count > 0 ? px + pw - 210.0f : state_x - 16.0f;
        plugins_draw_fit(g, r, detail, px + 18.0f, detail_right, y + g->font.glyph_h + 2.0f,
                         t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);

        if (plugin->language_count > 0) {
            char langs[48];
            snprintf(langs, sizeof(langs), "%d lang", plugin->language_count);
            font_draw(&g->font, r, langs, px + pw - 190.0f, y + g->font.glyph_h + 2.0f,
                      t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
        }
    }
}
