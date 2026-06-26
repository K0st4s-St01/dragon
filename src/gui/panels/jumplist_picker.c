#include <stdio.h>
#include "panel_jumplist_picker.h"
#include "app.h"
#include "document.h"
#include "buffer.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <string.h>
#include <GLFW/glfw3.h>

static bool   jp_open = false;
static int    jp_selected = 0;

typedef struct {
    int row;
    int col;
    char preview[80];
} JumpEntry;

static JumpEntry jp_jumps[256];
static int jp_jump_count = 0;

void panel_jumplist_picker_open(App *app) {
    jp_open = true;
    jp_selected = 0;
    jp_jump_count = 0;
    
    /* Collect jumplist entries */
    Document *doc = (Document *)app_get_doc(app);
    
    for (int i = 0; i < doc->jumplist_len; i++) {
        int row = doc->jumplist[i][0];
        int col = doc->jumplist[i][1];
        
        jp_jumps[jp_jump_count].row = row;
        jp_jumps[jp_jump_count].col = col;
        
        /* Get preview text from that line */
        Buffer *buf = &doc->buffer;
        if (row >= 0 && row < (int)buffer_line_count(buf)) {
            const char *line = buffer_line_ptr(buf, row);
            size_t line_len = buffer_line_len(buf, row);
            
            /* Skip leading whitespace for preview */
            size_t start = 0;
            while (start < line_len && (line[start] == ' ' || line[start] == '\t'))
                start++;
            
            /* Copy up to 79 chars for preview */
            size_t copy_len = line_len - start;
            if (copy_len > 79) copy_len = 79;
            
            memcpy(jp_jumps[jp_jump_count].preview, line + start, copy_len);
            jp_jumps[jp_jump_count].preview[copy_len] = '\0';
        } else {
            jp_jumps[jp_jump_count].preview[0] = '\0';
        }
        
        jp_jump_count++;
        if (jp_jump_count >= 256) break;
    }
    
    /* Set selection to current position if available */
    if (doc->jumplist_pos >= 0 && doc->jumplist_pos < jp_jump_count) {
        jp_selected = doc->jumplist_pos;
    }
}

void panel_jumplist_picker_close(App *app) {
    (void)app;
    jp_open = false;
}

bool panel_jumplist_picker_is_open(void) {
    return jp_open;
}

void panel_jumplist_picker_key(App *app, int key) {
    if (!jp_open) return;
    
    if (key == GLFW_KEY_UP || key == GLFW_KEY_TAB) {
        if (jp_selected > 0) jp_selected--;
    } else if (key == GLFW_KEY_DOWN) {
        if (jp_selected < jp_jump_count - 1) jp_selected++;
    } else if (key == GLFW_KEY_PAGE_UP) {
        jp_selected -= 10;
        if (jp_selected < 0) jp_selected = 0;
    } else if (key == GLFW_KEY_PAGE_DOWN) {
        jp_selected += 10;
        if (jp_selected >= jp_jump_count)
            jp_selected = jp_jump_count > 0 ? jp_jump_count - 1 : 0;
    } else if (key == GLFW_KEY_HOME) {
        jp_selected = 0;
    } else if (key == GLFW_KEY_END) {
        jp_selected = jp_jump_count > 0 ? jp_jump_count - 1 : 0;
    } else if (key == GLFW_KEY_ENTER) {
        if (jp_jump_count > 0) {
            /* Jump to selected position */
            Document *doc = (Document *)app_get_doc(app);
            Cursor *cur = &doc->cursors[0];
            cursor_move_to(cur, jp_jumps[jp_selected].row, jp_jumps[jp_selected].col);
            doc->jumplist_pos = jp_selected;
        }
        panel_jumplist_picker_close(app);
    }
}

void panel_jumplist_picker_render(Gui *g, App *app) {
    if (!jp_open) return;
    
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);
    
    float pw = 600.0f;
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
    font_draw(&g->font, r, "Jumplist", px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Results list */
    float result_y = py + 40;
    float line_h = g->font.glyph_h + 6;
    int max_visible = (int)((ph - 60) / line_h);
    int start = 0;
    if (jp_selected >= max_visible)
        start = jp_selected - max_visible + 1;
    
    if (jp_jump_count == 0) {
        font_draw(&g->font, r, "No jump history", px + 14, result_y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    } else {
        for (int i = start; i < jp_jump_count && (i - start) < max_visible; i++) {
            float ry = result_y + (i - start) * line_h;
            bool sel = (i == jp_selected);
            
            /* Selection highlight */
            if (sel) {
                renderer_draw_rect(r, px + 4, ry - 2, pw - 8, line_h,
                                   t->menu_selected[0], t->menu_selected[1],
                                   t->menu_selected[2], t->menu_selected[3]);
            }
            
            /* Line/column info */
            char location[32];
            snprintf(location, sizeof(location), "%4d:%-3d", 
                     jp_jumps[i].row + 1, jp_jumps[i].col + 1);
            font_draw(&g->font, r, location, px + 14, ry,
                      t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
            
            /* Preview text */
            if (jp_jumps[i].preview[0]) {
                font_draw(&g->font, r, jp_jumps[i].preview, px + 100, ry,
                          t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
            }
        }
    }
    
    /* Help text at bottom */
    float help_y = py + ph - 24;
    font_draw(&g->font, r, "Enter: Jump  Esc: Cancel", px + 14, help_y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
