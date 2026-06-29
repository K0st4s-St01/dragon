#include "panel_workspace_diagnostics.h"
#include "app.h"
#include "document.h"
#include "lsp.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

#define WD_MAX 256

typedef struct {
    int buffer_index;
    int line;
    int col;
    int severity;
    char file[256];
    char message[256];
} WorkspaceDiagnostic;

static bool wd_open = false;
static int wd_selected = 0;
static int wd_scroll = 0;
static WorkspaceDiagnostic wd_items[WD_MAX];
static int wd_count = 0;

void panel_workspace_diagnostics_open(App *app) {
    wd_open = true;
    wd_selected = 0;
    wd_scroll = 0;
    wd_count = 0;

    int buffers = app_get_buffer_count(app);
    for (int b = 0; b < buffers && wd_count < WD_MAX; b++) {
        Document *doc = (Document *)app_get_doc_at(app, b);
        if (!doc || !doc->diagnostics) continue;
        LSPDiagnostics *diag = (LSPDiagnostics *)doc->diagnostics;
        for (int i = 0; i < diag->count && wd_count < WD_MAX; i++) {
            wd_items[wd_count].buffer_index = b;
            wd_items[wd_count].line = diag->items[i].start_line;
            wd_items[wd_count].col = diag->items[i].start_col;
            wd_items[wd_count].severity = diag->items[i].severity;
            snprintf(wd_items[wd_count].file, sizeof(wd_items[wd_count].file), "%s",
                     doc->filepath ? doc->filepath : "[No Name]");
            const char *message = diag->items[i].message ? diag->items[i].message : "";
            const char *code = diag->items[i].code ? diag->items[i].code : "";
            if (code[0]) {
                snprintf(wd_items[wd_count].message, sizeof(wd_items[wd_count].message),
                         "[%s] %s", code, message);
            } else {
                snprintf(wd_items[wd_count].message, sizeof(wd_items[wd_count].message), "%s",
                         message);
            }
            wd_count++;
        }
    }
}

void panel_workspace_diagnostics_close(App *app) {
    (void)app;
    wd_open = false;
}

bool panel_workspace_diagnostics_is_open(void) {
    return wd_open;
}

static void wd_draw_fit(Gui *g, Renderer *r, const char *text,
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

void panel_workspace_diagnostics_key(App *app, int key) {
    if (!wd_open) return;
    switch (key) {
    case GLFW_KEY_DOWN:
    case GLFW_KEY_J:
        if (wd_selected < wd_count - 1) wd_selected++;
        break;
    case GLFW_KEY_UP:
    case GLFW_KEY_K:
        if (wd_selected > 0) wd_selected--;
        break;
    case GLFW_KEY_PAGE_DOWN:
        wd_selected += 10;
        if (wd_selected >= wd_count) wd_selected = wd_count > 0 ? wd_count - 1 : 0;
        break;
    case GLFW_KEY_PAGE_UP:
        wd_selected -= 10;
        if (wd_selected < 0) wd_selected = 0;
        break;
    case GLFW_KEY_HOME:
        wd_selected = 0;
        break;
    case GLFW_KEY_END:
        wd_selected = wd_count > 0 ? wd_count - 1 : 0;
        break;
    case GLFW_KEY_ENTER:
        if (wd_selected >= 0 && wd_selected < wd_count) {
            app_switch_to_buffer(app, wd_items[wd_selected].buffer_index);
            Document *doc = (Document *)app_get_doc(app);
            document_cursor_to(doc, wd_items[wd_selected].line, wd_items[wd_selected].col);
            document_sync_viewport_to_cursor(doc);
            panel_workspace_diagnostics_close(app);
        }
        break;
    case GLFW_KEY_ESCAPE:
        panel_workspace_diagnostics_close(app);
        break;
    default:
        break;
    }
}

void panel_workspace_diagnostics_render(Gui *g, App *app) {
    if (!wd_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app), h = app_get_height(app);
    float pw = (float)w * 0.70f;
    if (pw < 640.0f) pw = 640.0f;
    if (pw > 980.0f) pw = 980.0f;
    if (pw > (float)w - 48.0f) pw = (float)w - 48.0f;
    float ph = (float)h * 0.64f;
    if (ph < 360.0f) ph = 360.0f;
    if (ph > (float)h - 80.0f) ph = (float)h - 80.0f;
    float px = (float)w / 2 - pw / 2;
    float py = (float)h / 2 - ph / 2;
    renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0, 0, 0, 0.5f);
    renderer_draw_rect(r, px, py, pw, ph, t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 2, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py + 36.0f, pw, 1, t->accent[0], t->accent[1], t->accent[2], 0.28f);
    font_draw(&g->font, r, "Workspace Diagnostics (open buffers)", px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1);

    float row_h = g->font.glyph_h + 7;
    int visible = (int)((ph - 86) / row_h);
    if (visible < 1) visible = 1;
    if (wd_selected < wd_scroll) wd_scroll = wd_selected;
    if (wd_selected >= wd_scroll + visible) wd_scroll = wd_selected - visible + 1;
    if (wd_scroll < 0) wd_scroll = 0;
    if (wd_count == 0) {
        font_draw(&g->font, r, "No diagnostics in open buffers", px + 18, py + 48,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }
    for (int i = wd_scroll; i < wd_count && (i - wd_scroll) < visible; i++) {
        float y = py + 48 + (i - wd_scroll) * row_h;
        if (i == wd_selected)
            renderer_draw_rect(r, px + 5, y - 2, pw - 10, row_h,
                               t->menu_selected[0], t->menu_selected[1], t->menu_selected[2], t->menu_selected[3]);
        char sev = wd_items[i].severity == LSP_DIAG_ERROR ? 'E' :
                   wd_items[i].severity == LSP_DIAG_WARNING ? 'W' : 'I';
        char label[360];
        snprintf(label, sizeof(label), "%c %s:%d:%d", sev, wd_items[i].file,
                 wd_items[i].line + 1, wd_items[i].col + 1);
        float label_r = sev == 'E' ? t->error[0] : (sev == 'W' ? t->warning[0] : t->gutter_fg[0]);
        float label_g = sev == 'E' ? t->error[1] : (sev == 'W' ? t->warning[1] : t->gutter_fg[1]);
        float label_b = sev == 'E' ? t->error[2] : (sev == 'W' ? t->warning[2] : t->gutter_fg[2]);
        float msg_x = px + pw * 0.48f;
        wd_draw_fit(g, r, label, px + 18, msg_x - 16.0f, y, label_r, label_g, label_b, 1);
        wd_draw_fit(g, r, wd_items[i].message, msg_x, px + pw - 18, y,
                    t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], 1);
    }
    renderer_draw_rect(r, px, py + ph - 29, pw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.25f);
    font_draw(&g->font, r, "Enter go  j/k move  PageUp/PageDown jump  Esc close", px + 14, py + ph - 24,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
