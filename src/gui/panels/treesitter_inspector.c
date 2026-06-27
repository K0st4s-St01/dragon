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

    float pw = 520.0f;
    float ph = 180.0f;
    float px = (float)w / 2.0f - pw / 2.0f;
    float py = 70.0f;

    renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0, 0, 0, 0.35f);
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py + ph - 1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px + pw - 1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);

    font_draw(&g->font, r, "Tree-sitter", px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1);

    char line[256];
    const char *p = ts_info;
    float y = py + 40;
    while (*p && y < py + ph - 34) {
        size_t len = 0;
        while (p[len] && p[len] != '\n' && len < sizeof(line) - 1) len++;
        memcpy(line, p, len);
        line[len] = '\0';
        font_draw(&g->font, r, line, px + 18, y,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        y += g->font.glyph_h + 7;
        p += len;
        if (*p == '\n') p++;
    }

    font_draw(&g->font, r, "Enter/Esc close", px + 14, py + ph - 24,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
