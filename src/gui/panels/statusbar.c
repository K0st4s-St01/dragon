#include <stdio.h>
#include <string.h>
#include <time.h>
#include "panel_statusbar.h"
#include "app.h"
#include "document.h"
#include "mode.h"
#include "input.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"
#include "lsp.h"

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

static void draw_command_completions(Gui *g, Renderer *r, Theme *t,
                                     int win_w, float bar_y) {
    int count = input_cmd_completion_count();
    if (count <= 0)
        return;

    int selected = input_cmd_completion_selected();
    float row_h = g->font.glyph_h + 6.0f;
    float panel_w = 560.0f;
    if (panel_w > (float)win_w - 24.0f)
        panel_w = (float)win_w - 24.0f;
    float panel_h = row_h * (float)count + 10.0f;
    float x = 12.0f;
    float y = bar_y - panel_h - 4.0f;
    if (y < 4.0f)
        y = 4.0f;

    renderer_draw_rect(r, x, y, panel_w, panel_h,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], 0.98f);
    renderer_draw_rect(r, x, y, panel_w, 1.0f,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, x, y + panel_h - 1.0f, panel_w, 1.0f,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);

    for (int i = 0; i < count; i++) {
        float row_y = y + 5.0f + (float)i * row_h;
        if (i == selected) {
            renderer_draw_rect(r, x + 4.0f, row_y - 2.0f, panel_w - 8.0f, row_h,
                               t->menu_selected[0], t->menu_selected[1],
                               t->menu_selected[2], t->menu_selected[3]);
        }

        const char *name = input_cmd_completion_name(i);
        const char *detail = input_cmd_completion_detail(i);
        if (name) {
            font_draw(&g->font, r, name, x + 12.0f, row_y,
                      t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        }
        if (detail && detail[0]) {
            float detail_w = font_text_width(&g->font, detail);
            float detail_x = x + panel_w - detail_w - 12.0f;
            if (detail_x > x + 180.0f) {
                font_draw(&g->font, r, detail, detail_x, row_y,
                          t->gutter_fg[0], t->gutter_fg[1],
                          t->gutter_fg[2], t->gutter_fg[3]);
            }
        }
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

static float draw_left_label(Gui *g, Renderer *r, const char *text,
                             float left, float right, float y, float gap,
                             float cr, float cg, float cb, float ca) {
    if (!text || !*text) return left;
    float w = font_text_width(&g->font, text);
    if (left + w < right) {
        font_draw(&g->font, r, text, left, y, cr, cg, cb, ca);
        left += w + gap;
    }
    return left;
}

static int selection_length(Document *doc, const Cursor *cur) {
    if (!doc || !cur || !cur->has_selection) return 0;
    int sr, sc, er, ec;
    Cursor copy = *cur;
    cursor_normalize(&copy, &sr, &sc, &er, &ec);
    size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
    size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    return end > start ? (int)(end - start) : 0;
}

static const char *line_ending_label(Document *doc) {
    if (!doc) return "LF";
    return strstr(doc->buffer.text, "\r\n") ? "CRLF" : "LF";
}

static int git_head_path(const char *root, char *out, size_t out_size) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/.git/HEAD", root);
    FILE *f = fopen(path, "r");
    if (f) {
        fclose(f);
        snprintf(out, out_size, "%s", path);
        return 1;
    }

    snprintf(path, sizeof(path), "%s/.git", root);
    f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    if (strncmp(line, "gitdir:", 7) != 0) return 0;

    char *gitdir = line + 7;
    while (*gitdir == ' ' || *gitdir == '\t') gitdir++;
    gitdir[strcspn(gitdir, "\r\n")] = '\0';
    if (gitdir[0] == '/')
        snprintf(out, out_size, "%s/HEAD", gitdir);
    else
        snprintf(out, out_size, "%s/%s/HEAD", root, gitdir);
    return 1;
}

static const char *git_branch_label(App *app) {
    static char cached_root[512];
    static char cached_branch[128];
    static time_t cached_at = 0;

    const char *root = app_get_workspace_root(app);
    if (!root || !*root) return NULL;
    time_t now = time(NULL);
    if (cached_at == now && strcmp(cached_root, root) == 0)
        return cached_branch[0] ? cached_branch : NULL;

    cached_at = now;
    snprintf(cached_root, sizeof(cached_root), "%s", root);
    cached_branch[0] = '\0';

    char head_path[1024];
    if (!git_head_path(root, head_path, sizeof(head_path))) return NULL;

    FILE *f = fopen(head_path, "r");
    if (!f) return NULL;
    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return NULL;
    }
    fclose(f);
    line[strcspn(line, "\r\n")] = '\0';

    const char *prefix = "ref: refs/heads/";
    if (strncmp(line, prefix, strlen(prefix)) == 0) {
        snprintf(cached_branch, sizeof(cached_branch), "%.*s",
                 (int)sizeof(cached_branch) - 1, line + strlen(prefix));
    } else if (line[0]) {
        snprintf(cached_branch, sizeof(cached_branch), "%.7s", line);
    }
    return cached_branch[0] ? cached_branch : NULL;
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
        draw_command_completions(g, r, t, w, y);
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
    size_t line_count = buffer_line_count(&doc->buffer);
    int pct = line_count > 1 ? (int)(((cur->row + 1) * 100) / line_count) : 100;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    char pct_buf[16];
    snprintf(pct_buf, sizeof(pct_buf), "%d%%", pct);

    float right = (float)w - 10.0f;
    right = draw_right_label(g, r, count_buf, right, text_y, 18.0f,
                             t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    right = draw_right_label(g, r, pct_buf, right, text_y, 18.0f,
                             t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    right = draw_right_label(g, r, pos_buf, right, text_y, 18.0f,
                             t->fg[0], t->fg[1], t->fg[2], t->fg[3]);

    char selection_buf[48];
    int primary_len = selection_length(doc, cur);
    if (doc->cursor_count > 1 || primary_len > 0) {
        snprintf(selection_buf, sizeof(selection_buf), "Sel %d Len %d", doc->cursor_count, primary_len);
        right = draw_right_label(g, r, selection_buf, right, text_y, 18.0f,
                                 t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }

    const char *branch = git_branch_label(app);
    char branch_buf[160];
    if (branch) {
        snprintf(branch_buf, sizeof(branch_buf), "git:%s", branch);
        right = draw_right_label(g, r, branch_buf, right, text_y, 18.0f,
                                 t->namespace_color[0], t->namespace_color[1],
                                 t->namespace_color[2], 1.0f);
    }

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
    left = draw_left_label(g, r, fname, left, right, text_y, 10.0f,
                           t->fg[0], t->fg[1], t->fg[2], t->fg[3]);

    if (doc->dirty && left + font_text_width(&g->font, "+") < right) {
        font_draw(&g->font, r, "+", left, text_y,
                  t->warning[0], t->warning[1], t->warning[2], t->warning[3]);
        left += font_text_width(&g->font, "+") + 12.0f;
    }

    if (doc->language_id && doc->language_id[0])
        left = draw_left_label(g, r, doc->language_id, left, right, text_y, 10.0f,
                               t->type_color[0], t->type_color[1], t->type_color[2], 1.0f);
    left = draw_left_label(g, r, line_ending_label(doc), left, right, text_y, 10.0f,
                           t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    left = draw_left_label(g, r, "UTF-8", left, right, text_y, 12.0f,
                           t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);

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
