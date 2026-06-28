#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "panel_lsp_diagnostics.h"
#include "app.h"
#include "document.h"
#include "buffer.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"
#include "lsp.h"

#include <GLFW/glfw3.h>

static bool   lsp_diag_open = false;
static int    lsp_diag_selected = 0;
static double lsp_diag_open_time = 0;

typedef struct {
    int line;
    int character;
    int severity;
    char message[256];
    char preview[100];
} LSPDiagnosticEntry;

static LSPDiagnosticEntry lsp_diag_entries[128];
static int lsp_diag_count = 0;

void panel_lsp_diagnostics_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);

    lsp_diag_open = true;
    lsp_diag_selected = 0;
    lsp_diag_open_time = glfwGetTime();
    lsp_diag_count = 0;

    if (!doc->diagnostics)
        return;

    LSPDiagnostics *diag = (LSPDiagnostics *)doc->diagnostics;
    
    /* Populate diagnostics entries */
    lsp_diag_count = 0;
    for (int i = 0; i < diag->count && lsp_diag_count < 128; i++) {
        lsp_diag_entries[lsp_diag_count].line = diag->items[i].start_line;
        lsp_diag_entries[lsp_diag_count].character = diag->items[i].start_col;
        lsp_diag_entries[lsp_diag_count].severity = diag->items[i].severity;
        
        const char *message = diag->items[i].message ? diag->items[i].message : "";
        strncpy(lsp_diag_entries[lsp_diag_count].message, message, 255);
        lsp_diag_entries[lsp_diag_count].message[255] = '\0';
        
        /* Get line preview */
        const char *line_ptr = buffer_line_ptr(&doc->buffer, diag->items[i].start_line);
        int line_len = (int)buffer_line_len(&doc->buffer, diag->items[i].start_line);
        while (line_len > 0 && (line_ptr[line_len-1] == '\n' || line_ptr[line_len-1] == '\r'))
            line_len--;
        
        strncpy(lsp_diag_entries[lsp_diag_count].preview, line_ptr, 
                line_len < 100 ? line_len : 99);
        lsp_diag_entries[lsp_diag_count].preview[line_len < 100 ? line_len : 99] = '\0';
        
        lsp_diag_count++;
    }
    
    if (lsp_diag_count == 1)
        document_cursor_to(doc, lsp_diag_entries[0].line, lsp_diag_entries[0].character);
}

void panel_lsp_diagnostics_close(App *app) {
    (void)app;
    lsp_diag_open = false;
    lsp_diag_selected = 0;
}

bool panel_lsp_diagnostics_is_open(void) {
    return lsp_diag_open;
}

void panel_lsp_diagnostics_key(App *app, int key) {
    if (!lsp_diag_open) return;
    
    Document *doc = (Document *)app_get_doc(app);
    
    switch (key) {
        case GLFW_KEY_UP:
            if (lsp_diag_selected > 0) {
                lsp_diag_selected--;
            }
            break;
        case GLFW_KEY_DOWN:
            if (lsp_diag_selected < lsp_diag_count - 1) {
                lsp_diag_selected++;
            }
            break;
        case GLFW_KEY_ENTER:
            if (lsp_diag_selected < lsp_diag_count) {
                document_cursor_to(doc,
                                   lsp_diag_entries[lsp_diag_selected].line,
                                   lsp_diag_entries[lsp_diag_selected].character);
                document_sync_viewport_to_cursor(doc);
                panel_lsp_diagnostics_close(app);
            }
            break;
        case GLFW_KEY_ESCAPE:
            panel_lsp_diagnostics_close(app);
            break;
    }
}

void panel_lsp_diagnostics_render(Gui *g, App *app) {
    (void)g;
    if (!lsp_diag_open) return;
    
    Theme *t = theme_get();
    Renderer *r = app_get_renderer(app);
    int w = app_get_width(app);
    int h = app_get_height(app);
    
    float pw = 800.0f;
    float ph = 500.0f;
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
    font_draw(&g->font, r, "Diagnostics", px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Diagnostics list */
    float item_y = py + 40;
    float line_h = g->font.glyph_h + 6;
    int max_visible = (int)((ph - 60) / line_h);
    int start = 0;
    if (lsp_diag_selected >= max_visible)
        start = lsp_diag_selected - max_visible + 1;
    
    if (lsp_diag_count == 0) {
        font_draw(&g->font, r, "No diagnostics", px + 14, item_y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    } else {
        for (int i = start; i < lsp_diag_count && (i - start) < max_visible; i++) {
            float ry = item_y + (i - start) * line_h;
            bool sel = (i == lsp_diag_selected);
            
            /* Selection highlight */
            if (sel) {
                renderer_draw_rect(r, px + 4, ry - 2, pw - 8, line_h,
                                   t->menu_selected[0], t->menu_selected[1],
                                   t->menu_selected[2], t->menu_selected[3]);
            }
            
            /* Severity indicator */
            bool is_error = lsp_diag_entries[i].severity == LSP_DIAG_ERROR;
            bool is_warning = lsp_diag_entries[i].severity == LSP_DIAG_WARNING;
            char sev_char = is_error ? 'E' : (is_warning ? 'W' : 'I');
            float sev_r = is_error ? t->error[0] : (is_warning ? t->warning[0] : t->accent[0]);
            float sev_g = is_error ? t->error[1] : (is_warning ? t->warning[1] : t->accent[1]);
            float sev_b = is_error ? t->error[2] : (is_warning ? t->warning[2] : t->accent[2]);
            char sev_str[2] = {sev_char, '\0'};
            font_draw(&g->font, r, sev_str, px + 14, ry,
                      sev_r, sev_g, sev_b, 1.0f);
            
            /* Line:Col info */
            char location[32];
            snprintf(location, sizeof(location), "%4d:%-3d", 
                     lsp_diag_entries[i].line + 1, lsp_diag_entries[i].character + 1);
            font_draw(&g->font, r, location, px + 40, ry,
                      t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
            
            /* Message */
            font_draw(&g->font, r, lsp_diag_entries[i].message, px + 140, ry,
                      t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        }
    }
    
    /* Help text at bottom */
    float help_y = py + ph - 24;
    font_draw(&g->font, r, "Enter: Go  Esc: Cancel  Up/Down: Navigate", px + 14, help_y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
