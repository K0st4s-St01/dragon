#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "panel_lsp_goto.h"
#include "app.h"
#include "document.h"
#include "buffer.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <GLFW/glfw3.h>

static bool   lsp_goto_open = false;
static int    lsp_goto_selected = 0;

typedef struct {
    char uri[512];
    int line;
    int character;
    char preview[100];
} LSPGotoEntry;

static LSPGotoEntry lsp_goto_entries[64];
static int lsp_goto_count = 0;

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

static bool uri_to_path(const char *uri, char *out, size_t out_size) {
    if (!uri || !out || out_size == 0) return false;

    const char *path = uri;
    if (strncmp(path, "file://", 7) == 0) {
        path += 7;
        if (strncmp(path, "localhost/", 10) == 0)
            path += 9;
        else if (path[0] != '/') {
            const char *slash = strchr(path, '/');
            path = slash ? slash : path;
        }
    }

    size_t len = 0;
    for (const char *p = path; *p && len < out_size - 1; p++) {
        if (*p == '%' && p[1] && p[2]) {
            int hi = hex_value(p[1]);
            int lo = hex_value(p[2]);
            if (hi >= 0 && lo >= 0) {
                out[len++] = (char)((hi << 4) | lo);
                p += 2;
                continue;
            }
        }
        out[len++] = *p;
    }
    out[len] = '\0';
    return out[0] != '\0';
}

static const char *display_path(const char *uri) {
    const char *path = uri;
    if (path && strncmp(path, "file://", 7) == 0)
        path += 7;
    if (path && strncmp(path, "localhost/", 10) == 0)
        path += 9;
    return path ? path : "";
}

static bool goto_entry_matches_doc(const LSPGotoEntry *entry, Document *doc) {
    if (!entry || !doc || !doc->filepath) return false;

    char path[1024];
    if (!uri_to_path(entry->uri, path, sizeof(path)))
        return false;
    return strcmp(path, doc->filepath) == 0;
}

static bool jump_to_entry(App *app, const LSPGotoEntry *entry) {
    if (!app || !entry) return false;

    char path[1024];
    if (!uri_to_path(entry->uri, path, sizeof(path)))
        return false;

    app_open_file(app, path);
    Document *doc = (Document *)app_get_doc(app);
    if (!doc || !goto_entry_matches_doc(entry, doc))
        return false;

    doc->cursor_count = 1;
    cursor_clear_selection(&doc->cursors[0]);
    document_cursor_to(doc, entry->line, entry->character);
    document_sync_viewport_to_cursor(doc);
    return true;
}

static void goto_draw_fit(Gui *g, Renderer *r, const char *text,
                          float x, float right, float y,
                          float cr, float cg, float cb, float ca) {
    if (!text || !*text || right <= x) return;
    char clipped[256];
    snprintf(clipped, sizeof(clipped), "%s", text);
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

void panel_lsp_goto_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    
    /* Check if there are goto results */
    if (!doc->goto_results || doc->goto_result_count == 0) {
        return;
    }
    
    /* If only one result, navigate directly without showing picker */
    if (doc->goto_result_count == 1) {
        LSPGotoResult *res = &doc->goto_results[0];
        LSPGotoEntry entry = {{0}, res->line, res->character, {0}};
        strncpy(entry.uri, res->uri ? res->uri : "", sizeof(entry.uri) - 1);
        jump_to_entry(app, &entry);
        return;
    }
    
    /* Multiple results - show picker */
    lsp_goto_open = true;
    lsp_goto_selected = 0;
    lsp_goto_count = 0;
    
    /* Load results into picker entries */
    for (int i = 0; i < doc->goto_result_count && i < 64; i++) {
        LSPGotoResult *res = &doc->goto_results[i];
        
        strncpy(lsp_goto_entries[lsp_goto_count].uri, res->uri, 511);
        lsp_goto_entries[lsp_goto_count].uri[511] = '\0';
        
        lsp_goto_entries[lsp_goto_count].line = res->line;
        lsp_goto_entries[lsp_goto_count].character = res->character;
        
        /* Get preview text from goto result line if available */
        if (goto_entry_matches_doc(&lsp_goto_entries[lsp_goto_count], doc)) {
            Buffer *buf = &doc->buffer;
            if (res->line >= 0 && res->line < (int)buffer_line_count(buf)) {
                const char *line = buffer_line_ptr(buf, res->line);
                size_t line_len = buffer_line_len(buf, res->line);
                
                /* Skip leading whitespace */
                size_t start = 0;
                while (start < line_len && (line[start] == ' ' || line[start] == '\t'))
                    start++;
                
                /* Copy up to 99 chars for preview */
                size_t copy_len = line_len - start;
                if (copy_len > 99) copy_len = 99;
                
                memcpy(lsp_goto_entries[lsp_goto_count].preview, line + start, copy_len);
                lsp_goto_entries[lsp_goto_count].preview[copy_len] = '\0';
            } else {
                lsp_goto_entries[lsp_goto_count].preview[0] = '\0';
            }
        } else {
            snprintf(lsp_goto_entries[lsp_goto_count].preview, 100, 
                     "in %s", display_path(res->uri));
        }
        
        lsp_goto_count++;
    }
}

void panel_lsp_goto_close(App *app) {
    (void)app;
    lsp_goto_open = false;
}

bool panel_lsp_goto_is_open(void) {
    return lsp_goto_open;
}

void panel_lsp_goto_key(App *app, int key) {
    if (!lsp_goto_open) return;
    
    if (key == GLFW_KEY_UP || key == GLFW_KEY_TAB) {
        if (lsp_goto_selected > 0) lsp_goto_selected--;
    } else if (key == GLFW_KEY_DOWN) {
        if (lsp_goto_selected < lsp_goto_count - 1) lsp_goto_selected++;
    } else if (key == GLFW_KEY_PAGE_UP) {
        lsp_goto_selected -= 10;
        if (lsp_goto_selected < 0) lsp_goto_selected = 0;
    } else if (key == GLFW_KEY_PAGE_DOWN) {
        lsp_goto_selected += 10;
        if (lsp_goto_selected >= lsp_goto_count)
            lsp_goto_selected = lsp_goto_count > 0 ? lsp_goto_count - 1 : 0;
    } else if (key == GLFW_KEY_HOME) {
        lsp_goto_selected = 0;
    } else if (key == GLFW_KEY_END) {
        lsp_goto_selected = lsp_goto_count > 0 ? lsp_goto_count - 1 : 0;
    } else if (key == GLFW_KEY_ENTER) {
        if (lsp_goto_count > 0) {
            LSPGotoEntry *entry = &lsp_goto_entries[lsp_goto_selected];
            jump_to_entry(app, entry);
        }
        panel_lsp_goto_close(app);
    } else if (key == GLFW_KEY_ESCAPE) {
        panel_lsp_goto_close(app);
    }
}

void panel_lsp_goto_render(Gui *g, App *app) {
    if (!lsp_goto_open) return;
    
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);
    
    float pw = (float)w * 0.62f;
    if (pw < 560.0f) pw = 560.0f;
    if (pw > 820.0f) pw = 820.0f;
    if (pw > (float)w - 48.0f) pw = (float)w - 48.0f;
    float ph = (float)h * 0.60f;
    if (ph < 340.0f) ph = 340.0f;
    if (ph > (float)h - 80.0f) ph = (float)h - 80.0f;
    float px = (float)w / 2 - pw / 2;
    float py = (float)h / 2 - ph / 2;
    
    /* Dim the rest of the screen */
    renderer_draw_rect(r, 0, 0, (float)w, (float)h,
                       0.0f, 0.0f, 0.0f, 0.5f);
    
    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    
    renderer_draw_rect(r, px, py, pw, 2, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py + 36.0f, pw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.24f);
    
    /* Title */
    font_draw(&g->font, r, "LSP Results", px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Results list */
    float result_y = py + 48;
    float line_h = g->font.glyph_h + 6;
    int max_visible = (int)((ph - 86) / line_h);
    if (max_visible < 1) max_visible = 1;
    int start = 0;
    if (lsp_goto_selected >= max_visible)
        start = lsp_goto_selected - max_visible + 1;
    
    if (lsp_goto_count == 0) {
        font_draw(&g->font, r, "No results", px + 14, result_y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    } else {
        for (int i = start; i < lsp_goto_count && (i - start) < max_visible; i++) {
            float ry = result_y + (i - start) * line_h;
            bool sel = (i == lsp_goto_selected);
            
            /* Selection highlight */
            if (sel) {
                renderer_draw_rect(r, px + 4, ry - 2, pw - 8, line_h,
                                   t->menu_selected[0], t->menu_selected[1],
                                   t->menu_selected[2], t->menu_selected[3]);
            }
            
            /* Line/column info */
            char location[32];
            snprintf(location, sizeof(location), "%4d:%-3d", 
                     lsp_goto_entries[i].line + 1, lsp_goto_entries[i].character + 1);
            font_draw(&g->font, r, location, px + 14, ry,
                      t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
            
            /* Preview text */
            if (lsp_goto_entries[i].preview[0]) {
                goto_draw_fit(g, r, lsp_goto_entries[i].preview, px + 120, px + pw - 16, ry,
                              t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
            }
        }
    }
    
    /* Help text at bottom */
    float help_y = py + ph - 24;
    renderer_draw_rect(r, px, help_y - 5.0f, pw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.20f);
    font_draw(&g->font, r, "Enter jump  Esc close  Up/Down move  PageUp/PageDown jump", px + 14, help_y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
