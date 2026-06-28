#include <stdio.h>
#include <string.h>
#include "panel_statusbar.h"
#include "app.h"
#include "document.h"
#include "mode.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"
#include "lsp.h"

extern const char *input_cmd_get(void);

static const char *mode_name(Mode m) {
    switch (m) {
    case MODE_NORMAL:          return "NORMAL";
    case MODE_INSERT:          return "INSERT";
    case MODE_SELECT:          return "SELECT";
    case MODE_VIEW:            return "VIEW";
    case MODE_COMMAND:         return ":";
    case MODE_GOTO:            return "GOTO";
    case MODE_FIND:            return "FIND";
    case MODE_SEARCH:          return "SEARCH";
    default:                   return "?";
    }
}

static const char *file_label(const char *path) {
    if (!path || !*path) return "[No Name]";
    const char *slash = strrchr(path, '/');
    return slash && slash[1] ? slash + 1 : path;
}

static void mode_color(Theme *t, Mode m, float color[4]) {
    color[0] = t->accent[0];
    color[1] = t->accent[1];
    color[2] = t->accent[2];
    color[3] = 1.0f;
    if (m == MODE_INSERT) {
        color[0] = t->string[0];
        color[1] = t->string[1];
        color[2] = t->string[2];
    } else if (m == MODE_SELECT) {
        color[0] = t->warning[0];
        color[1] = t->warning[1];
        color[2] = t->warning[2];
    } else if (m == MODE_COMMAND || m == MODE_SEARCH || m == MODE_FIND) {
        color[0] = t->macro_color[0];
        color[1] = t->macro_color[1];
        color[2] = t->macro_color[2];
    }
}

static float draw_right_label(Gui *g, Renderer *r, const char *text,
                              float right, float y, float gap,
                              float cr, float cg, float cb, float ca) {
    float w = font_text_width(&g->font, text);
    float x = right - w;
    if (x > 0.0f)
        font_draw(&g->font, r, text, x, y, cr, cg, cb, ca);
    return x - gap;
}

void panel_statusbar(Gui *g, App *app, Document *doc, ModeState *mode) {
    static int spinner_tick = 0;
    static const char spinner[] = "|/-\\";
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);
    float bar_h = g->font.glyph_h + 8.0f;
    float y = (float)h - bar_h;
    float text_y = y + 4.0f;

    renderer_draw_rect(r, 0, y, (float)w, bar_h,
                       t->status_bg[0], t->status_bg[1],
                       t->status_bg[2], t->status_bg[3]);

    Mode current_mode = mode_get(mode);
    const char *mn = mode_name(current_mode);
    float mc[4];
    mode_color(t, current_mode, mc);
    float mode_w = font_text_width(&g->font, mn) + 18.0f;
    if (mode_w < 34.0f) mode_w = 34.0f;
    renderer_draw_rect(r, 0.0f, y, mode_w, bar_h, mc[0], mc[1], mc[2], 0.95f);
    font_draw(&g->font, r, mn, 9.0f, text_y,
              t->bg[0], t->bg[1], t->bg[2], 1.0f);

    if (mode_is(mode, MODE_COMMAND)) {
        const char *cmd = input_cmd_get();
        char cmd_display[192];
        snprintf(cmd_display, sizeof(cmd_display), ":%s", cmd);
        font_draw(&g->font, r, cmd_display, mode_w + 10.0f, text_y,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        return;
    }

    float left = mode_w + 10.0f;
    if (macro_is_recording(&doc->macros)) {
        font_draw(&g->font, r, "REC", left, text_y,
                  t->error[0], t->error[1], t->error[2], t->error[3]);
        left += font_text_width(&g->font, "REC") + 12.0f;
    }

    Cursor *cur = &doc->cursors[0];
    char pos_buf[64];
    snprintf(pos_buf, sizeof(pos_buf), "Ln %d, Col %d", cur->row + 1, cur->col + 1);

    int errors = 0, warnings = 0, info = 0;
    const LSPDiagnostic *line_diag = NULL;
    if (doc->diagnostics) {
        LSPDiagnostics *diag = (LSPDiagnostics *)doc->diagnostics;
        for (int i = 0; i < diag->count; i++) {
            if (diag->items[i].severity == LSP_DIAG_ERROR) errors++;
            else if (diag->items[i].severity == LSP_DIAG_WARNING) warnings++;
            else info++;
            if (!line_diag && diag->items[i].start_line == cur->row)
                line_diag = &diag->items[i];
        }
    }

    int ws_errors = 0, ws_warnings = 0;
    int buffers = app_get_buffer_count(app);
    for (int b = 0; b < buffers; b++) {
        Document *other = (Document *)app_get_doc_at(app, b);
        if (!other || !other->diagnostics) continue;
        LSPDiagnostics *diag = (LSPDiagnostics *)other->diagnostics;
        for (int i = 0; i < diag->count; i++) {
            if (diag->items[i].severity == LSP_DIAG_ERROR) ws_errors++;
            else if (diag->items[i].severity == LSP_DIAG_WARNING) ws_warnings++;
        }
    }

    int lsp_ready = 0, lsp_connecting = 0, lsp_errors = 0;
    lsp_manager_status_counts((LSPManager *)app_get_lsp_manager(app),
                              &lsp_ready, &lsp_connecting, &lsp_errors);
    char lsp_buf[96];
    char spin = lsp_connecting ? spinner[(spinner_tick++ / 8) % 4] : ' ';
    snprintf(lsp_buf, sizeof(lsp_buf), "LSP%c%d%s E%d W%d I%d  WS E%d W%d",
             spin,
             lsp_ready,
             lsp_errors ? "!" : "",
             errors, warnings, info, ws_errors, ws_warnings);

    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%zu lines", buffer_line_count(&doc->buffer));

    float right = (float)w - 10.0f;
    right = draw_right_label(g, r, count_buf, right, text_y, 18.0f,
                             t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    right = draw_right_label(g, r, pos_buf, right, text_y, 18.0f,
                             t->fg[0], t->fg[1], t->fg[2], t->fg[3]);

    float lsp_r = lsp_errors || errors ? t->error[0] : (warnings ? t->warning[0] : t->gutter_fg[0]);
    float lsp_g = lsp_errors || errors ? t->error[1] : (warnings ? t->warning[1] : t->gutter_fg[1]);
    float lsp_b = lsp_errors || errors ? t->error[2] : (warnings ? t->warning[2] : t->gutter_fg[2]);
    float lsp_w = font_text_width(&g->font, lsp_buf);
    float lsp_x = right - lsp_w;
    if (lsp_x > left + 140.0f) {
        font_draw(&g->font, r, lsp_buf, lsp_x, text_y, lsp_r, lsp_g, lsp_b, 1.0f);
        right = lsp_x - 18.0f;
    }

    const char *fname = file_label(doc->filepath);
    float file_w = font_text_width(&g->font, fname);
    if (left + file_w < right) {
        font_draw(&g->font, r, fname, left, text_y,
                  t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
        left += file_w + 10.0f;
    }

    if (doc->dirty && left + font_text_width(&g->font, "+") < right) {
        font_draw(&g->font, r, "+", left, text_y,
                  t->warning[0], t->warning[1], t->warning[2], t->warning[3]);
        left += font_text_width(&g->font, "+") + 12.0f;
    }

    if (line_diag && line_diag->message) {
        char diag_buf[180];
        char sev = line_diag->severity == LSP_DIAG_ERROR ? 'E' :
                   line_diag->severity == LSP_DIAG_WARNING ? 'W' : 'I';
        snprintf(diag_buf, sizeof(diag_buf), "%c: %s", sev, line_diag->message);
        float diag_w = font_text_width(&g->font, diag_buf);
        if (left + diag_w + 12.0f < right) {
            float *color = line_diag->severity == LSP_DIAG_ERROR ? t->error :
                           line_diag->severity == LSP_DIAG_WARNING ? t->warning :
                           t->gutter_fg;
            font_draw(&g->font, r, diag_buf, left, text_y,
                      color[0], color[1], color[2], 1.0f);
        }
    }
}
