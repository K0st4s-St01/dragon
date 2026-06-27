#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "panel_lsp_hover.h"
#include "app.h"
#include "document.h"
#include "lsp.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"
#include "cursor.h"

#include <GLFW/glfw3.h>

typedef struct {
    char *content;
    int max_width;
    int line_count;
    char **lines;
} HoverDisplay;

static bool hover_open = false;
static int hover_scroll = 0;
static HoverDisplay hover_display = {0};
static double hover_open_time = 0;
static const double HOVER_TIMEOUT = 10.0;  /* 10 seconds */

void panel_lsp_hover_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    
    if (!doc->hover_result) {
        return;
    }
    
    /* Clear old display */
    if (hover_display.content) {
        free(hover_display.content);
        if (hover_display.lines) {
            for (int i = 0; i < hover_display.line_count; i++) {
                free(hover_display.lines[i]);
            }
            free(hover_display.lines);
        }
        memset(&hover_display, 0, sizeof(hover_display));
    }
    
    /* Get hover content */
    const char *content = lsp_hover_get_contents((LSPHover *)doc->hover_result);
    if (!content || strlen(content) == 0) {
        return;
    }
    
    /* Store content */
    hover_display.content = strdup(content);
    hover_display.max_width = 80;
    
    /* Split into lines */
    char *copy = strdup(content);
    hover_display.line_count = 1;
    
    for (char *p = copy; *p; p++) {
        if (*p == '\n') {
            hover_display.line_count++;
        }
    }
    
    hover_display.lines = malloc(hover_display.line_count * sizeof(char *));
    
    char *line_start = copy;
    int line_idx = 0;
    for (char *p = copy; *p; p++) {
        if (*p == '\n') {
            *p = '\0';
            hover_display.lines[line_idx] = strdup(line_start);
            line_idx++;
            line_start = p + 1;
        }
    }
    if (line_idx < hover_display.line_count) {
        hover_display.lines[line_idx] = strdup(line_start);
    }
    
    free(copy);
    
    hover_open = true;
    hover_scroll = 0;
    hover_open_time = glfwGetTime();
}

void panel_lsp_hover_close(App *app) {
    (void)app;
    hover_open = false;
}

bool panel_lsp_hover_is_open(void) {
    return hover_open;
}

void panel_lsp_hover_key(App *app, int key, int mods) {
    if (!hover_open) return;
    if ((key == GLFW_KEY_D && (mods & GLFW_MOD_CONTROL)) || key == GLFW_KEY_PAGE_DOWN) {
        hover_scroll += 6;
        if (hover_scroll > hover_display.line_count - 1)
            hover_scroll = hover_display.line_count > 0 ? hover_display.line_count - 1 : 0;
        hover_open_time = glfwGetTime();
    } else if ((key == GLFW_KEY_U && (mods & GLFW_MOD_CONTROL)) || key == GLFW_KEY_PAGE_UP) {
        hover_scroll -= 6;
        if (hover_scroll < 0) hover_scroll = 0;
        hover_open_time = glfwGetTime();
    } else if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER) {
        panel_lsp_hover_close(app);
    }
}

void panel_lsp_hover_render(Gui *g, App *app) {
    if (!hover_open) return;
    
    /* Check timeout */
    double now = glfwGetTime();
    if (now - hover_open_time > HOVER_TIMEOUT) {
        panel_lsp_hover_close(app);
        return;
    }
    
    if (!hover_display.content || hover_display.line_count == 0) {
        return;
    }
    
    Document *doc = (Document *)app_get_doc(app);
    Cursor *cur = &doc->cursors[0];
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    
    /* Position: offset from cursor */
    float char_w = g->font.glyph_w;
    float char_h = g->font.glyph_h;
    float tooltip_x = cur->col * char_w + 100;
    float tooltip_y = cur->row * char_h + 150;
    
    /* Calculate size */
    float padding = 8;
    float line_h = char_h + 4;
    float content_w = 0;
    
    for (int i = 0; i < hover_display.line_count; i++) {
        if (hover_display.lines[i]) {
            float line_w = strlen(hover_display.lines[i]) * char_w;
            if (line_w > content_w) {
                content_w = line_w;
            }
        }
    }
    
    float tw = content_w + padding * 2;
    int max_lines = hover_display.line_count < 20 ? hover_display.line_count : 20;
    float th = max_lines * line_h + padding * 2;
    
    /* Cap size */
    if (tw > 600) tw = 600;
    if (th > 400) th = 400;
    
    /* Draw background */
    renderer_draw_rect(r, tooltip_x, tooltip_y, tw, th,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], 0.95f);
    
    /* Draw border */
    renderer_draw_rect(r, tooltip_x, tooltip_y, tw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, tooltip_x, tooltip_y + th - 1, tw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, tooltip_x, tooltip_y, 1, th,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, tooltip_x + tw - 1, tooltip_y, 1, th,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Draw content */
    float text_y = tooltip_y + padding;
    for (int i = hover_scroll; i < hover_display.line_count && (i - hover_scroll) < max_lines; i++) {
        if (hover_display.lines[i]) {
            font_draw(&g->font, r, hover_display.lines[i], 
                      tooltip_x + padding, text_y,
                      t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        }
        text_y += line_h;
    }

    if (hover_display.line_count > max_lines) {
        char scroll_info[48];
        snprintf(scroll_info, sizeof(scroll_info), "%d/%d", hover_scroll + 1, hover_display.line_count);
        font_draw(&g->font, r, scroll_info,
                  tooltip_x + tw - padding - font_text_width(&g->font, scroll_info),
                  tooltip_y + th - line_h,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }
}
