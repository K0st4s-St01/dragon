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
static bool hover_loading = false;
static LSPClient *hover_pending_client = NULL;
static int hover_pending_id = -1;
static const double HOVER_TIMEOUT = 10.0;  /* 10 seconds */

static void hover_clear_display(void) {
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
}

static void hover_set_content(const char *content) {
    hover_clear_display();
    if (!content || strlen(content) == 0) return;

    hover_display.content = strdup(content);
    if (!hover_display.content) return;
    hover_display.max_width = 80;

    char *copy = strdup(content);
    if (!copy) {
        hover_clear_display();
        return;
    }
    hover_display.line_count = 1;
    for (char *p = copy; *p; p++) {
        if (*p == '\n')
            hover_display.line_count++;
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
}

static char *hover_file_uri(const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path) + 8;
    char *uri = malloc(len);
    if (!uri) return NULL;
    snprintf(uri, len, "file://%s", path);
    return uri;
}

static void hover_draw_fit(Gui *g, Renderer *r, const char *text,
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

void panel_lsp_hover_request(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    if (!doc || !doc->language_id || !doc->filepath) return;
    LSPManager *manager = (LSPManager *)app_get_lsp_manager(app);
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    if (!client || client->status != LSP_STATUS_INITIALIZED) return;

    char *uri = hover_file_uri(doc->filepath);
    if (!uri) return;
    Cursor *cur = &doc->cursors[0];
    lsp_client_send_hover_request(client, uri, cur->row, cur->col);
    free(uri);

    hover_clear_display();
    hover_pending_client = client;
    hover_pending_id = client->id - 1;
    hover_loading = true;
    hover_open = true;
    hover_scroll = 0;
    hover_open_time = glfwGetTime();
}

void panel_lsp_hover_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    if (!doc || !doc->hover_result) return;
    const char *content = lsp_hover_get_contents((LSPHover *)doc->hover_result);
    hover_set_content(content);
    if (!hover_display.content) return;
    hover_loading = false;
    hover_pending_client = NULL;
    hover_pending_id = -1;
    hover_open = true;
    hover_scroll = 0;
    hover_open_time = glfwGetTime();
}

void panel_lsp_hover_close(App *app) {
    (void)app;
    hover_open = false;
    hover_loading = false;
    hover_pending_client = NULL;
    hover_pending_id = -1;
}

bool panel_lsp_hover_is_open(void) {
    return hover_open;
}

bool panel_lsp_hover_key(App *app, int key, int mods) {
    if (!hover_open) return false;
    if ((key == GLFW_KEY_D && (mods & GLFW_MOD_CONTROL)) || key == GLFW_KEY_PAGE_DOWN) {
        hover_scroll += 6;
        if (hover_scroll > hover_display.line_count - 1)
            hover_scroll = hover_display.line_count > 0 ? hover_display.line_count - 1 : 0;
        hover_open_time = glfwGetTime();
        return true;
    } else if ((key == GLFW_KEY_U && (mods & GLFW_MOD_CONTROL)) || key == GLFW_KEY_PAGE_UP) {
        hover_scroll -= 6;
        if (hover_scroll < 0) hover_scroll = 0;
        hover_open_time = glfwGetTime();
        return true;
    } else if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER) {
        panel_lsp_hover_close(app);
        return true;
    }
    return false;
}

bool panel_lsp_hover_handle_lsp_response(App *app, LSPClient *client, int response_id, const char *response) {
    if (!hover_pending_client || client != hover_pending_client || response_id != hover_pending_id)
        return false;

    Document *doc = (Document *)app_get_doc(app);
    LSPHover *hover = lsp_parse_hover_response(response);
    if (doc) {
        if (doc->hover_result)
            lsp_free_hover((LSPHover *)doc->hover_result);
        doc->hover_result = hover;
    } else {
        lsp_free_hover(hover);
        hover = NULL;
    }
    const char *content = hover ? lsp_hover_get_contents(hover) : NULL;
    hover_set_content(content);
    hover_loading = false;
    hover_pending_client = NULL;
    hover_pending_id = -1;
    if (!hover_display.content)
        hover_open = false;
    else
        hover_open_time = glfwGetTime();
    return true;
}

void panel_lsp_hover_render(Gui *g, App *app) {
    if (!hover_open) return;
    
    /* Check timeout */
    double now = glfwGetTime();
    if (now - hover_open_time > HOVER_TIMEOUT) {
        panel_lsp_hover_close(app);
        return;
    }
    
    if (hover_loading) {
        Renderer *r = app_get_renderer(app);
        Theme *t = theme_get();
        int w = app_get_width(app);
        int h = app_get_height(app);
        float pw = 260.0f;
        float ph = 56.0f;
        float px = (float)w / 2.0f - pw / 2.0f;
        float py = (float)h / 2.0f - ph / 2.0f;
        renderer_draw_rect(r, px, py, pw, ph,
                           t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
        renderer_draw_rect(r, px, py, pw, 2.0f,
                           t->accent[0], t->accent[1], t->accent[2], 1.0f);
        font_draw(&g->font, r, "Loading hover...", px + 12.0f, py + 20.0f,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], 1.0f);
        return;
    }

    if (!hover_display.content || hover_display.line_count == 0) {
        return;
    }
    
    Document *doc = (Document *)app_get_doc(app);
    if (!doc) return;
    Cursor *cur = &doc->cursors[0];
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int win_w = app_get_width(app);
    int win_h = app_get_height(app);
    
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
            float line_w = font_text_width(&g->font, hover_display.lines[i]);
            if (line_w > content_w) {
                content_w = line_w;
            }
        }
    }
    
    float tw = content_w + padding * 2;
    int max_lines = hover_display.line_count < 20 ? hover_display.line_count : 20;
    float th = max_lines * line_h + padding * 2;
    
    /* Cap size */
    float max_tw = (float)win_w - 24.0f;
    if (max_tw > 640.0f) max_tw = 640.0f;
    if (tw > max_tw) tw = max_tw;
    float max_th = (float)win_h - 48.0f;
    if (max_th > 420.0f) max_th = 420.0f;
    if (th > max_th) th = max_th;
    max_lines = (int)((th - padding * 2.0f) / line_h);
    if (max_lines < 1) max_lines = 1;
    if (tooltip_x + tw > (float)win_w - 12.0f)
        tooltip_x = (float)win_w - tw - 12.0f;
    if (tooltip_y + th > (float)win_h - 12.0f)
        tooltip_y = (float)win_h - th - 12.0f;
    if (tooltip_x < 12.0f) tooltip_x = 12.0f;
    if (tooltip_y < 12.0f) tooltip_y = 12.0f;
    
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
            hover_draw_fit(g, r, hover_display.lines[i],
                           tooltip_x + padding, tooltip_x + tw - padding, text_y,
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
