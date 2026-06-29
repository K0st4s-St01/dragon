#include <stdio.h>
#include "panel_buffer_picker.h"
#include "app.h"
#include "document.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <string.h>
#include <GLFW/glfw3.h>

static bool   bp_open = false;
static int    bp_selected = 0;

typedef struct {
    int         index;
    const char *filepath;
    bool        is_dirty;
    bool        is_current;
} BufferEntry;

static BufferEntry bp_buffers[64];
static int bp_buffer_count = 0;

void panel_buffer_picker_open(App *app) {
    bp_open = true;
    bp_selected = 0;
    bp_buffer_count = 0;

    int current = app_get_current_buffer_index(app);
    int count = app_get_buffer_count(app);
    if (count > (int)(sizeof(bp_buffers) / sizeof(bp_buffers[0])))
        count = (int)(sizeof(bp_buffers) / sizeof(bp_buffers[0]));

    for (int i = 0; i < count; i++) {
        Document *doc = (Document *)app_get_doc_at(app, i);
        if (!doc) continue;
        bp_buffers[bp_buffer_count].index = i;
        bp_buffers[bp_buffer_count].filepath = doc->filepath;
        bp_buffers[bp_buffer_count].is_dirty = doc->dirty;
        bp_buffers[bp_buffer_count].is_current = i == current;
        if (i == current)
            bp_selected = bp_buffer_count;
        bp_buffer_count++;
    }
}

void panel_buffer_picker_close(App *app) {
    (void)app;
    bp_open = false;
}

bool panel_buffer_picker_is_open(void) {
    return bp_open;
}

static void panel_buffer_picker_refresh(App *app) {
    int keep_index = -1;
    if (bp_selected >= 0 && bp_selected < bp_buffer_count)
        keep_index = bp_buffers[bp_selected].index;

    int count = app_get_buffer_count(app);
    int current = app_get_current_buffer_index(app);
    bp_buffer_count = 0;
    if (count > (int)(sizeof(bp_buffers) / sizeof(bp_buffers[0])))
        count = (int)(sizeof(bp_buffers) / sizeof(bp_buffers[0]));

    for (int i = 0; i < count; i++) {
        Document *doc = (Document *)app_get_doc_at(app, i);
        if (!doc) continue;
        bp_buffers[bp_buffer_count].index = i;
        bp_buffers[bp_buffer_count].filepath = doc->filepath;
        bp_buffers[bp_buffer_count].is_dirty = doc->dirty;
        bp_buffers[bp_buffer_count].is_current = i == current;
        if (i == keep_index)
            bp_selected = bp_buffer_count;
        bp_buffer_count++;
    }

    if (bp_selected >= bp_buffer_count)
        bp_selected = bp_buffer_count > 0 ? bp_buffer_count - 1 : 0;
    if (bp_selected < 0)
        bp_selected = 0;
}

void panel_buffer_picker_key(App *app, int key) {
    if (!bp_open) return;
    
    if (key == GLFW_KEY_UP || key == GLFW_KEY_TAB) {
        if (bp_selected > 0) bp_selected--;
    } else if (key == GLFW_KEY_DOWN) {
        if (bp_selected < bp_buffer_count - 1) bp_selected++;
    } else if (key == GLFW_KEY_PAGE_UP) {
        bp_selected -= 10;
        if (bp_selected < 0) bp_selected = 0;
    } else if (key == GLFW_KEY_PAGE_DOWN) {
        bp_selected += 10;
        if (bp_selected >= bp_buffer_count)
            bp_selected = bp_buffer_count > 0 ? bp_buffer_count - 1 : 0;
    } else if (key == GLFW_KEY_HOME) {
        bp_selected = 0;
    } else if (key == GLFW_KEY_END) {
        bp_selected = bp_buffer_count > 0 ? bp_buffer_count - 1 : 0;
    } else if (key == GLFW_KEY_ENTER) {
        if (bp_buffer_count > 0)
            app_switch_to_buffer(app, bp_buffers[bp_selected].index);
        panel_buffer_picker_close(app);
    } else if (key == GLFW_KEY_D || key == GLFW_KEY_X || key == GLFW_KEY_DELETE) {
        if (bp_buffer_count > 1 && bp_selected >= 0 && bp_selected < bp_buffer_count) {
            app_close_buffer(app, bp_buffers[bp_selected].index);
            panel_buffer_picker_refresh(app);
        }
    }
}

static const char *buffer_basename(const char *path) {
    if (!path || !*path) return "[No Name]";
    const char *slash = strrchr(path, '/');
    return slash && slash[1] ? slash + 1 : path;
}

static void buffer_parent_label(const char *path, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!path || !*path) {
        snprintf(out, out_size, "scratch buffer");
        return;
    }
    const char *slash = strrchr(path, '/');
    if (!slash || slash == path) {
        snprintf(out, out_size, slash == path ? "/" : ".");
        return;
    }
    size_t len = (size_t)(slash - path);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

static void draw_fit_text(Gui *g, Renderer *r, const char *text,
                          float x, float right, float y,
                          float cr, float cg, float cb, float ca) {
    if (!text || !*text || right <= x) return;
    float max_w = right - x;
    if (font_text_width(&g->font, text) <= max_w) {
        font_draw(&g->font, r, text, x, y, cr, cg, cb, ca);
        return;
    }

    char clipped[256];
    snprintf(clipped, sizeof(clipped), "%s", text);
    size_t len = strlen(clipped);
    while (len > 4 && font_text_width(&g->font, clipped) > max_w) {
        clipped[--len] = '\0';
        if (len > 3) {
            clipped[len - 3] = '.';
            clipped[len - 2] = '.';
            clipped[len - 1] = '.';
        }
    }
    font_draw(&g->font, r, clipped, x, y, cr, cg, cb, ca);
}

void panel_buffer_picker_render(Gui *g, App *app) {
    if (!bp_open) return;
    
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);
    
    float pw = (float)w * 0.58f;
    if (pw < 520.0f) pw = 520.0f;
    if (pw > (float)w - 48.0f) pw = (float)w - 48.0f;
    float ph = (float)h * 0.62f;
    if (ph < 320.0f) ph = 320.0f;
    if (ph > (float)h - 80.0f) ph = (float)h - 80.0f;
    float px = (float)w / 2 - pw / 2;
    float py = (float)h / 2 - ph / 2;
    
    /* Dim the rest of the screen */
    renderer_draw_rect(r, 0, 0, (float)w, (float)h,
                       0.0f, 0.0f, 0.0f, 0.5f);
    
    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    
    renderer_draw_rect(r, px, py, pw, 38.0f,
                       t->status_bg[0], t->status_bg[1], t->status_bg[2], 0.52f);
    renderer_draw_rect(r, px, py, pw, 2.0f, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py + 38.0f, pw, 1.0f,
                       t->accent[0], t->accent[1], t->accent[2], 0.28f);

    char title[96];
    snprintf(title, sizeof(title), "Buffers  %d open", bp_buffer_count);
    font_draw(&g->font, r, title, px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Results list */
    float result_y = py + 48;
    float line_h = g->font.glyph_h * 2.0f + 12.0f;
    int max_visible = (int)((ph - 86) / line_h);
    if (max_visible < 1) max_visible = 1;
    int start = 0;
    if (bp_selected >= max_visible)
        start = bp_selected - max_visible + 1;
    
    if (bp_buffer_count == 0) {
        font_draw(&g->font, r, "No open buffers", px + 14, result_y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    } else {
        for (int i = start; i < bp_buffer_count && (i - start) < max_visible; i++) {
            float ry = result_y + (i - start) * line_h;
            bool sel = (i == bp_selected);
            
            /* Selection highlight */
            if (sel) {
                renderer_draw_rect(r, px + 4, ry - 2, pw - 8, line_h,
                                   t->menu_selected[0], t->menu_selected[1],
                                   t->menu_selected[2], t->menu_selected[3]);
            }
            
            char indicator[24];
            snprintf(indicator, sizeof(indicator), "%s%d",
                     bp_buffers[i].is_current ? ">" : " ", bp_buffers[i].index + 1);
            font_draw(&g->font, r, indicator, px + 14, ry,
                      bp_buffers[i].is_current ? t->accent[0] : t->gutter_fg[0],
                      bp_buffers[i].is_current ? t->accent[1] : t->gutter_fg[1],
                      bp_buffers[i].is_current ? t->accent[2] : t->gutter_fg[2], 1.0f);
            
            const char *basename = buffer_basename(bp_buffers[i].filepath);
            float text_left = px + 58.0f;
            float text_right = px + pw - 78.0f;
            draw_fit_text(g, r, basename, text_left, text_right, ry,
                          t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);

            char parent[256];
            buffer_parent_label(bp_buffers[i].filepath, parent, sizeof(parent));
            draw_fit_text(g, r, parent, text_left, text_right,
                          ry + g->font.glyph_h + 4.0f,
                          t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
            
            if (bp_buffers[i].is_dirty) {
                float dirty_x = px + pw - 50;
                font_draw(&g->font, r, "+", dirty_x, ry,
                          t->warning[0], t->warning[1], t->warning[2], 1.0f);
            }
        }
    }
    
    /* Help text at bottom */
    float help_y = py + ph - 24;
    renderer_draw_rect(r, px, help_y - 5.0f, pw, 1.0f,
                       t->accent[0], t->accent[1], t->accent[2], 0.28f);
    font_draw(&g->font, r, "Enter open  d/x close  j/k move  Esc cancel", px + 14, help_y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
