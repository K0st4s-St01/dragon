#include "panel_treesitter_inspector.h"
#include "app.h"
#include "document.h"
#include "treesitter.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

static bool ts_inspector_open = false;
static char ts_info[1024];

static const char *doc_tree_sitter_language(Document *doc) {
    if (!doc || !doc->filepath) return NULL;
    const char *dot = strrchr(doc->filepath, '.');
    if (!dot || !dot[1]) return NULL;
    return treesitter_language_name_for_extension(dot + 1);
}

void panel_treesitter_inspector_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    TreeSitterManager *mgr = (TreeSitterManager *)app_get_treesitter_manager(app);
    ts_info[0] = '\0';

    if (!doc || !mgr) {
        snprintf(ts_info, sizeof(ts_info), "tree-sitter unavailable");
    } else {
        const char *lang_name = doc_tree_sitter_language(doc);
        TreeSitterLanguage *lang = lang_name ? treesitter_get_language(mgr, lang_name) : NULL;
        Cursor *cur = &doc->cursors[0];
        if (!lang || !treesitter_describe_node_at(lang, (uint32_t)cur->row, (uint32_t)cur->col,
                                                  ts_info, sizeof(ts_info))) {
            snprintf(ts_info, sizeof(ts_info), "no tree-sitter node at cursor");
        }
    }
    ts_inspector_open = true;
}

void panel_treesitter_inspector_close(App *app) {
    (void)app;
    ts_inspector_open = false;
}

bool panel_treesitter_inspector_is_open(void) {
    return ts_inspector_open;
}

static void ts_draw_fit(Gui *g, Renderer *r, const char *text,
                        float x, float right, float y,
                        float cr, float cg, float cb, float ca) {
    if (!text || !*text || right <= x) return;
    char clipped[256];
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

void panel_treesitter_inspector_key(App *app, int key) {
    if (!ts_inspector_open) return;
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER)
        panel_treesitter_inspector_close(app);
}

void panel_treesitter_inspector_render(Gui *g, App *app) {
    if (!ts_inspector_open) return;

    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);

    float pw = (float)w * 0.46f;
    if (pw < 420.0f) pw = 420.0f;
    if (pw > 640.0f) pw = 640.0f;
    if (pw > (float)w - 48.0f) pw = (float)w - 48.0f;
    float ph = 190.0f;
    float px = (float)w / 2.0f - pw / 2.0f;
    float py = (float)h / 2.0f - ph / 2.0f;

    renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0, 0, 0, 0.35f);
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 2, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py + 36.0f, pw, 1, t->accent[0], t->accent[1], t->accent[2], 0.28f);

    font_draw(&g->font, r, "Tree-sitter", px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1);

    char line[256];
    const char *p = ts_info;
    float y = py + 48;
    while (*p && y < py + ph - 34) {
        size_t len = 0;
        while (p[len] && p[len] != '\n' && len < sizeof(line) - 1) len++;
        memcpy(line, p, len);
        line[len] = '\0';
        ts_draw_fit(g, r, line, px + 18, px + pw - 18, y,
                    t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        y += g->font.glyph_h + 7;
        p += len;
        if (*p == '\n') p++;
    }

    renderer_draw_rect(r, px, py + ph - 29.0f, pw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.25f);
    font_draw(&g->font, r, "Enter/Esc close", px + 14, py + ph - 24,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
