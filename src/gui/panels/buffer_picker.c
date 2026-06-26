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
    const char *filepath;
    bool        is_dirty;
} BufferEntry;

/* For now we'll support up to 32 buffers */
static BufferEntry bp_buffers[32];
static int bp_buffer_count = 0;

void panel_buffer_picker_open(App *app) {
    bp_open = true;
    bp_selected = 0;
    bp_buffer_count = 0;
    
    /* Collect current document */
    Document *doc = (Document *)app_get_doc(app);
    if (doc->filepath) {
        bp_buffers[bp_buffer_count].filepath = doc->filepath;
        bp_buffers[bp_buffer_count].is_dirty = doc->dirty;
        bp_buffer_count++;
    }
    
    /* TODO: When multiple buffers are supported, iterate through them */
}

void panel_buffer_picker_close(App *app) {
    (void)app;
    bp_open = false;
}

bool panel_buffer_picker_is_open(void) {
    return bp_open;
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
        if (bp_buffer_count > 0 && bp_buffers[bp_selected].filepath) {
            /* Switch to selected buffer */
            /* For now just close since we only have one buffer */
            /* TODO: Implement actual buffer switching */
        }
        panel_buffer_picker_close(app);
    }
}

void panel_buffer_picker_render(Gui *g, App *app) {
    if (!bp_open) return;
    
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);
    
    float pw = 500.0f;
    float ph = 400.0f;
    float px = (float)w / 2 - pw / 2;
    float py = (float)h / 2 - ph / 2;
    
    /* Dim the rest of the screen */
    renderer_draw_rect(r, 0, 0, (float)w, (float)h,
                       0.0f, 0.0f, 0.0f, 0.5f);
    
    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    
    /* Border */
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py+ph-1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px+pw-1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Title */
    font_draw(&g->font, r, "Buffer Picker", px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Results list */
    float result_y = py + 40;
    float line_h = g->font.glyph_h + 6;
    int max_visible = (int)((ph - 60) / line_h);
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
            
            /* Buffer indicator */
            char indicator[16];
            snprintf(indicator, sizeof(indicator), "[%d]", i + 1);
            font_draw(&g->font, r, indicator, px + 14, ry,
                      t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
            
            /* Filename */
            const char *name = bp_buffers[i].filepath;
            /* Extract just the filename from path */
            const char *basename = strrchr(name, '/');
            if (basename) basename++;
            else basename = name;
            
            font_draw(&g->font, r, basename, px + 60, ry,
                      t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
            
            /* Dirty indicator */
            if (bp_buffers[i].is_dirty) {
                float dirty_x = px + pw - 40;
                font_draw(&g->font, r, "[+]", dirty_x, ry,
                          t->accent[0], t->accent[1], t->accent[2], 1.0f);
            }
        }
    }
    
    /* Help text at bottom */
    float help_y = py + ph - 24;
    font_draw(&g->font, r, "Enter: Open  Esc: Cancel", px + 14, help_y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
