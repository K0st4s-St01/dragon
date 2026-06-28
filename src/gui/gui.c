#include "dragon_editor/gui.h"
#include "dragon_editor/renderer.h"
#include "dragon_editor/theme.h"
#include "dragon_editor/document.h"
#include "dragon_editor/buffer.h"
#include "dragon_editor/text.h"
#include "dragon_editor/app.h"
#include "dragon_editor/syntax.h"
#include "dragon_editor/lsp.h"
#include "dragon_editor/config.h"

#include "panel_statusbar.h"
#include "panel_file_browser.h"
#include "panel_find_replace.h"
#include "panel_goto.h"
#include "panel_about.h"
#include "panel_buffer_picker.h"
#include "panel_jumplist_picker.h"
#include "panel_lsp_goto.h"
#include "panel_lsp_hover.h"
#include "panel_lsp_diagnostics.h"
#include "panel_symbols_picker.h"
#include "panel_rename.h"
#include "panel_code_actions.h"
#include "panel_space_menu.h"
#include "panel_palette.h"
#include "panel_settings.h"
#include "panel_treesitter_inspector.h"
#include "panel_workspace_symbols.h"
#include "panel_workspace_diagnostics.h"
#include "panel_completion.h"
#include "panel_notification.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

void gui_init(Gui *g) {
    font_init(&g->font, NULL, 16.0f);
}

void gui_free(Gui *g) {
    font_free(&g->font);
}

void gui_begin(Gui *g) {
    (void)g;
}

void gui_end(Gui *g) {
    (void)g;
}

/* Get theme color for syntax type */
static void get_syntax_color(Theme *t, SyntaxType type,
                             float *r, float *g, float *b, float *a) {
    switch (type) {
    case SYNTAX_KEYWORD:
        *r = t->keyword[0]; *g = t->keyword[1]; *b = t->keyword[2]; *a = t->keyword[3];
        break;
    case SYNTAX_STRING:
        *r = t->string[0]; *g = t->string[1]; *b = t->string[2]; *a = t->string[3];
        break;
    case SYNTAX_NUMBER:
        *r = t->number[0]; *g = t->number[1]; *b = t->number[2]; *a = t->number[3];
        break;
    case SYNTAX_COMMENT:
        *r = t->comment[0]; *g = t->comment[1]; *b = t->comment[2]; *a = t->comment[3];
        break;
    case SYNTAX_FUNCTION:
        *r = t->function_color[0]; *g = t->function_color[1]; *b = t->function_color[2]; *a = t->function_color[3];
        break;
    case SYNTAX_TYPE:
        *r = t->type_color[0]; *g = t->type_color[1]; *b = t->type_color[2]; *a = t->type_color[3];
        break;
    case SYNTAX_VARIABLE:
        *r = t->variable_color[0]; *g = t->variable_color[1]; *b = t->variable_color[2]; *a = t->variable_color[3];
        break;
    case SYNTAX_MACRO:
        *r = t->macro_color[0]; *g = t->macro_color[1]; *b = t->macro_color[2]; *a = t->macro_color[3];
        break;
    case SYNTAX_OPERATOR:
        *r = t->operator_color[0]; *g = t->operator_color[1]; *b = t->operator_color[2]; *a = t->operator_color[3];
        break;
    case SYNTAX_NAMESPACE:
        *r = t->namespace_color[0]; *g = t->namespace_color[1]; *b = t->namespace_color[2]; *a = t->namespace_color[3];
        break;
    case SYNTAX_ERROR:
        *r = t->error[0]; *g = t->error[1]; *b = t->error[2]; *a = t->error[3];
        break;
    case SYNTAX_WARNING:
        *r = t->warning[0]; *g = t->warning[1]; *b = t->warning[2]; *a = t->warning[3];
        break;
    default:
        *r = t->fg[0]; *g = t->fg[1]; *b = t->fg[2]; *a = t->fg[3];
        break;
    }
}

/* Calculate display column position accounting for tabs */
static int display_col(const char *line, int col, int tab_width) {
    if (tab_width <= 0) tab_width = 4;
    int display = 0;
    for (int i = 0; i < col && line[i]; i++) {
        if (line[i] == '\t') {
            display = ((display / tab_width) + 1) * tab_width;
        } else {
            display++;
        }
    }
    return display;
}

static int utf8_char_len(const char *s, int remaining) {
    if (!s || remaining <= 0) return 0;
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0)
        return remaining >= 2 && ((unsigned char)s[1] & 0xC0) == 0x80 ? 2 : 1;
    if ((c & 0xF0) == 0xE0)
        return remaining >= 3 &&
               ((unsigned char)s[1] & 0xC0) == 0x80 &&
               ((unsigned char)s[2] & 0xC0) == 0x80 ? 3 : 1;
    if ((c & 0xF8) == 0xF0)
        return remaining >= 4 &&
               ((unsigned char)s[1] & 0xC0) == 0x80 &&
               ((unsigned char)s[2] & 0xC0) == 0x80 &&
               ((unsigned char)s[3] & 0xC0) == 0x80 ? 4 : 1;
    return 1;
}

static bool diagnostic_contains_cursor(const LSPDiagnostic *diag, int row, int col) {
    if (!diag) return false;
    if (row < diag->start_line || row > diag->end_line)
        return false;
    if (diag->start_line == diag->end_line) {
        int end_col = diag->end_col > diag->start_col ? diag->end_col : diag->start_col + 1;
        return col >= diag->start_col && col <= end_col;
    }
    if (row == diag->start_line)
        return col >= diag->start_col;
    if (row == diag->end_line)
        return col <= diag->end_col;
    return true;
}

static const LSPDiagnostic *diagnostic_at_cursor(Document *doc) {
    if (!doc || !doc->diagnostics || doc->cursor_count <= 0)
        return NULL;

    Cursor *cur = &doc->cursors[0];
    LSPDiagnostics *diagnostics = (LSPDiagnostics *)doc->diagnostics;
    const LSPDiagnostic *line_diag = NULL;

    for (int i = 0; i < diagnostics->count; i++) {
        const LSPDiagnostic *diag = &diagnostics->items[i];
        if (!diag->message || !diag->message[0])
            continue;
        if (diagnostic_contains_cursor(diag, cur->row, cur->col))
            return diag;
        if (!line_diag && diag->start_line == cur->row)
            line_diag = diag;
    }

    return line_diag;
}

static int wrap_text_line(const char *text, int start, int max_chars, char *out, int out_size) {
    int len = (int)strlen(text);
    if (start >= len) {
        if (out_size > 0) out[0] = '\0';
        return len;
    }

    int end = start + max_chars;
    if (end > len) end = len;
    int break_at = end;
    if (end < len) {
        for (int i = end; i > start + 8; i--) {
            if (text[i] == ' ' || text[i] == '\t') {
                break_at = i;
                break;
            }
        }
    }

    int copy = break_at - start;
    if (copy >= out_size) copy = out_size - 1;
    if (copy < 0) copy = 0;
    memcpy(out, text + start, (size_t)copy);
    out[copy] = '\0';

    while (break_at < len && (text[break_at] == ' ' || text[break_at] == '\t'))
        break_at++;
    return break_at;
}

static void render_diagnostic_popup(Gui *g, App *app, Document *doc,
                                    float vx, float vy, float vw, float vh,
                                    float text_x, float text_y, float line_h,
                                    int first_line, int visible_lines,
                                    int tab_width) {
    const LSPDiagnostic *diag = diagnostic_at_cursor(doc);
    if (!diag || diag->severity != LSP_DIAG_ERROR || !diag->message || !diag->message[0])
        return;

    Cursor *cur = &doc->cursors[0];
    int screen_row = cur->row - first_line;
    if (screen_row < 0 || screen_row >= visible_lines)
        return;

    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int line_len = (int)buffer_line_len(&doc->buffer, cur->row);
    while (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r'))
        line_len--;

    int col = cur->col;
    if (col < 0) col = 0;
    if (col > line_len) col = line_len;

    float padding = 8.0f;
    float title_h = g->font.glyph_h + 4.0f;
    float msg_line_h = g->font.glyph_h + 3.0f;
    float max_w = vw - 32.0f;
    if (max_w > 520.0f) max_w = 520.0f;
    if (max_w < 220.0f) max_w = vw - 12.0f;
    if (max_w < 120.0f) return;

    int max_chars = (int)((max_w - padding * 2.0f) / g->font.glyph_w);
    if (max_chars < 12) max_chars = 12;
    if (max_chars > 96) max_chars = 96;

    char lines[4][192];
    int line_count = 0;
    int pos = 0;
    while (diag->message[pos] && line_count < 4) {
        pos = wrap_text_line(diag->message, pos, max_chars, lines[line_count], sizeof(lines[line_count]));
        if (lines[line_count][0])
            line_count++;
    }
    if (diag->message[pos] && line_count > 0) {
        int n = (int)strlen(lines[line_count - 1]);
        if (n > 3) {
            lines[line_count - 1][n - 3] = '.';
            lines[line_count - 1][n - 2] = '.';
            lines[line_count - 1][n - 1] = '.';
        }
    }
    if (line_count == 0) return;

    float content_w = font_text_width(&g->font, "Error");
    for (int i = 0; i < line_count; i++) {
        float w = font_text_width(&g->font, lines[i]);
        if (w > content_w) content_w = w;
    }

    float popup_w = content_w + padding * 2.0f;
    if (popup_w > max_w) popup_w = max_w;
    float popup_h = title_h + (float)line_count * msg_line_h + padding * 2.0f;

    float cursor_x = text_x + display_col(line, col, tab_width) * g->font.glyph_w;
    float cursor_y = text_y + (float)screen_row * line_h;
    float popup_x = cursor_x + g->font.glyph_w;
    float popup_y = cursor_y + line_h + 4.0f;

    if (popup_x + popup_w > vx + vw - 8.0f)
        popup_x = vx + vw - popup_w - 8.0f;
    if (popup_x < vx + 8.0f)
        popup_x = vx + 8.0f;
    if (popup_y + popup_h > vy + vh - 8.0f)
        popup_y = cursor_y - popup_h - 4.0f;
    if (popup_y < vy + 8.0f)
        popup_y = vy + 8.0f;

    renderer_draw_rect(r, popup_x, popup_y, popup_w, popup_h,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], 0.97f);
    renderer_draw_rect(r, popup_x, popup_y, popup_w, 2.0f,
                       t->error[0], t->error[1], t->error[2], 1.0f);
    renderer_draw_rect(r, popup_x, popup_y + popup_h - 1.0f, popup_w, 1.0f,
                       t->error[0], t->error[1], t->error[2], 0.75f);
    renderer_draw_rect(r, popup_x, popup_y, 1.0f, popup_h,
                       t->error[0], t->error[1], t->error[2], 0.75f);
    renderer_draw_rect(r, popup_x + popup_w - 1.0f, popup_y, 1.0f, popup_h,
                       t->error[0], t->error[1], t->error[2], 0.75f);

    font_draw(&g->font, r, "Error", popup_x + padding, popup_y + padding,
              t->error[0], t->error[1], t->error[2], 1.0f);
    float y = popup_y + padding + title_h;
    for (int i = 0; i < line_count; i++) {
        font_draw(&g->font, r, lines[i], popup_x + padding, y,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        y += msg_line_h;
    }
}

static void render_editor_view(Gui *g, App *app, Document *doc, ModeState *mode,
                               float vx, float vy, float vw, float vh, bool active) {
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    Config *cfg = app_get_config(app);
    int tab_width = (cfg && cfg->tab_width > 0) ? cfg->tab_width : 4;

    float gutter_w = 60.0f;
    float line_h = g->font.glyph_h + 6.0f;  /* Better spacing */
    float text_x = vx + gutter_w + 12.0f;
    float text_y = vy + 32.0f;
    float editor_h = vh - 32.0f;
    if (!doc || vw <= 0 || vh <= 0) return;

    Cursor *cur = &doc->cursors[0];
    size_t lines = buffer_line_count(&doc->buffer);

    /* Background */
    renderer_draw_rect(r, vx, vy, vw, vh,
                       t->bg[0], t->bg[1], t->bg[2], t->bg[3]);
    if (active) {
        renderer_draw_rect(r, vx, vy, vw, 2, t->accent[0], t->accent[1], t->accent[2], 1.0f);
        renderer_draw_rect(r, vx, vy + vh - 2, vw, 2, t->accent[0], t->accent[1], t->accent[2], 1.0f);
        renderer_draw_rect(r, vx, vy, 2, vh, t->accent[0], t->accent[1], t->accent[2], 1.0f);
        renderer_draw_rect(r, vx + vw - 2, vy, 2, vh, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    }

    int first_line = doc->scroll_y;
    int visible_lines = (int)(editor_h / line_h) + 1;
    if (first_line + visible_lines > (int)lines)
        first_line = (int)lines - visible_lines;
    if (first_line < 0) first_line = 0;

    for (int i = 0; i < visible_lines && (first_line + i) < (int)lines; i++) {
        int line_num = first_line + i;
        float y = text_y + (float)i * line_h;

        /* Line highlight - current line gets subtle highlight */
        if (line_num == cur->row) {
            renderer_draw_rect(r, vx, y, vw, line_h,
                               t->line_highlight[0], t->line_highlight[1],
                               t->line_highlight[2], t->line_highlight[3]);
        }

        /* Selection highlight */
        if (cur->has_selection) {
            int sr, sc, er, ec;
            cursor_normalize(cur, &sr, &sc, &er, &ec);
            if (line_num >= sr && line_num <= er) {
                int len = (int)buffer_line_len(&doc->buffer, line_num);
                int sel_start = (line_num == sr) ? sc : 0;
                int sel_end = (line_num == er) ? ec : len;
                if (sel_end > len) sel_end = len;
                const char *sel_line = buffer_line_ptr(&doc->buffer, line_num);
                int sel_dcol_start = display_col(sel_line, sel_start, 4);
                int sel_dcol_end = display_col(sel_line, sel_end, 4);
                float x1 = text_x + sel_dcol_start * g->font.glyph_w;
                float x2 = text_x + sel_dcol_end * g->font.glyph_w;
                renderer_draw_rect(r, x1, y, x2-x1, line_h,
                                   t->selection_bg[0], t->selection_bg[1],
                                   t->selection_bg[2], t->selection_bg[3]);
            }
        }

        /* Gutter background */
        renderer_draw_rect(r, vx, y, gutter_w, line_h,
                           t->gutter_bg[0], t->gutter_bg[1],
                           t->gutter_bg[2], t->gutter_bg[3]);

        /* Gutter separator line */
        renderer_draw_rect(r, vx + gutter_w, y, 1, line_h,
                           t->gutter_fg[0] * 0.3f, t->gutter_fg[1] * 0.3f, 
                           t->gutter_fg[2] * 0.3f, 0.3f);

        /* Line number - right aligned */
        char num[16];
        snprintf(num, sizeof(num), "%4d", line_num + 1);
        font_draw(&g->font, r, num, vx + 10, y + 3,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
        
        /* Diagnostic markers on gutter */
        if (doc->diagnostics) {
            LSPDiagnostics *diag = (LSPDiagnostics *)doc->diagnostics;
            for (int d = 0; d < diag->count; d++) {
                if (diag->items[d].start_line == line_num) {
                    /* Draw diagnostic indicator */
                    float diag_x = vx + 6;
                    float diag_y = y + 3;
                    bool is_error = diag->items[d].severity == LSP_DIAG_ERROR;
                    bool is_warning = diag->items[d].severity == LSP_DIAG_WARNING;
                    char indicator = is_error ? 'E' : (is_warning ? 'W' : 'I');
                    float color_r = is_error ? t->error[0] : (is_warning ? t->warning[0] : t->accent[0]);
                    float color_g = is_error ? t->error[1] : (is_warning ? t->warning[1] : t->accent[1]);
                    float color_b = is_error ? t->error[2] : (is_warning ? t->warning[2] : t->accent[2]);
                    char ind_str[2] = {indicator, '\0'};
                    font_draw(&g->font, r, ind_str, diag_x, diag_y, color_r, color_g, color_b, 1.0f);
                    break;  /* Only show first diagnostic per line */
                }
            }
        }

        /* Line text with syntax highlighting */
        const char *line = buffer_line_ptr(&doc->buffer, line_num);
        int line_len = (int)buffer_line_len(&doc->buffer, line_num);
        while (line_len > 0 && (line[line_len-1] == '\n' || line[line_len-1] == '\r'))
            line_len--;

        /* Render text with syntax highlighting */
        if (line_len > 0) {
            int col = 0;
            while (col < line_len) {
                SyntaxType syntax_type = syntax_get_type_at(&doc->syntax, line_num, col);
                float sr, sg, sb, sa;
                get_syntax_color(t, syntax_type, &sr, &sg, &sb, &sa);
                
                int dcol = display_col(line, col, tab_width);
                
                if (line[col] == '\t') {
                    /* Tab - skip to next tab stop (no visible rendering) */
                } else {
                    int char_len = utf8_char_len(line + col, line_len - col);
                    char ch[5] = {0};
                    memcpy(ch, line + col, (size_t)char_len);
                    float x = text_x + dcol * g->font.glyph_w;
                    font_draw(&g->font, r, ch, x, y + 2, sr, sg, sb, sa);
                }
                
                col += utf8_char_len(line + col, line_len - col);
            }
        }

        if (doc->diagnostics) {
            LSPDiagnostics *diag = (LSPDiagnostics *)doc->diagnostics;
            for (int d = 0; d < diag->count; d++) {
                if (diag->items[d].start_line != line_num) continue;
                float dr = diag->items[d].severity == LSP_DIAG_ERROR ? t->error[0] : t->warning[0];
                float dg = diag->items[d].severity == LSP_DIAG_ERROR ? t->error[1] : t->warning[1];
                float db = diag->items[d].severity == LSP_DIAG_ERROR ? t->error[2] : t->warning[2];
                if (diag->items[d].severity != LSP_DIAG_ERROR &&
                    diag->items[d].severity != LSP_DIAG_WARNING) {
                    dr = t->accent[0];
                    dg = t->accent[1];
                    db = t->accent[2];
                }
                int start_col = diag->items[d].start_col < line_len ? diag->items[d].start_col : line_len;
                int end_col = diag->items[d].end_line == line_num && diag->items[d].end_col > start_col ?
                    diag->items[d].end_col : start_col + 1;
                if (end_col > line_len) end_col = line_len;
                if (end_col <= start_col) end_col = start_col + 1;
                float dx = text_x + display_col(line, start_col, tab_width) * g->font.glyph_w;
                float underline_w = (float)(display_col(line, end_col, tab_width) -
                                            display_col(line, start_col, tab_width)) * g->font.glyph_w;
                if (underline_w < g->font.glyph_w) underline_w = g->font.glyph_w;
                renderer_draw_rect(r, dx, y + line_h - 4, underline_w, 2, dr, dg, db, 0.85f);
                break;
            }
        }
    }

    /* Cursors */
    if (mode_is(mode, MODE_INSERT) || mode_is(mode, MODE_NORMAL)) {
        for (int ci = 0; ci < doc->cursor_count; ci++) {
            Cursor *c = &doc->cursors[ci];
            int csr = c->row - first_line;
            if (csr >= 0 && csr < visible_lines) {
                const char *cur_line = buffer_line_ptr(&doc->buffer, c->row);
                int cur_dcol = display_col(cur_line, c->col, tab_width);
                float cx = text_x + cur_dcol * g->font.glyph_w;
                float cy = text_y + (float)csr * line_h;
                float cw = mode_is(mode, MODE_INSERT) ? 2.0f : g->font.glyph_w;
                float ch = line_h;
                float cr = t->cursor_color[0];
                float cg = t->cursor_color[1];
                float cb = t->cursor_color[2];
                float ca = t->cursor_color[3];
                
                if (ci > 0) { cr *= 0.6f; cg *= 0.6f; cb *= 0.6f; }
                
                /* Main cursor block */
                renderer_draw_rect(r, cx, cy, cw, ch, cr, cg, cb, ca);
                
                /* Insert mode: thin line with highlight */
                if (mode_is(mode, MODE_INSERT) && ci == 0) {
                    renderer_draw_rect(r, cx - 1, cy, cw + 2, ch, 
                                      cr, cg, cb, ca * 0.4f);  /* Glow effect */
                }
            }
        }
    }

    if (active) {
        render_diagnostic_popup(g, app, doc, vx, vy, vw, vh,
                                text_x, text_y, line_h,
                                first_line, visible_lines, tab_width);
    }
}

void gui_render(Gui *g, App *app, Document *doc, ModeState *mode) {
    WindowManager *wm = app_get_window_manager(app);
    int win_w = app_get_width(app);
    int win_h = app_get_height(app);
    float content_h = (float)win_h - 24.0f;
    if (content_h < 40.0f) content_h = (float)win_h;

    if (wm && wm->count > 0) {
        for (int i = 0; i < wm->count; i++) {
            Window *w = &wm->windows[i];
            if (!w->visible) continue;
            Document *wdoc = (Document *)app_get_doc_at(app, w->doc_index);
            if (!wdoc) continue;
            float vx = (float)w->x * (float)win_w / 100.0f;
            float vy = (float)w->y * content_h / 40.0f;
            float vw = (float)w->width * (float)win_w / 100.0f;
            float vh = (float)w->height * content_h / 40.0f;
            glEnable(GL_SCISSOR_TEST);
            glScissor((int)vx, win_h - (int)(vy + vh), (int)vw, (int)vh);
            render_editor_view(g, app, wdoc, mode, vx, vy, vw, vh, i == wm->active);
            glDisable(GL_SCISSOR_TEST);
        }
    } else {
        render_editor_view(g, app, doc, mode, 0, 0, (float)win_w, content_h, true);
    }

     panel_statusbar(g, app, doc, mode);
     panel_file_browser_render(g, app);
     panel_find_render(g, app, doc);
     panel_goto_render(g, app, doc);
     panel_about_render(g, app);
     panel_buffer_picker_render(g, app);
     panel_jumplist_picker_render(g, app);
      panel_lsp_goto_render(g, app);
      panel_lsp_diagnostics_render(g, app);
      panel_lsp_hover_render(g, app);
      panel_symbols_picker_render(g, app);
      panel_rename_render(g, app);
      panel_code_actions_render(g, app);
      panel_space_menu_render(g, app);
      panel_palette_render(g, app);
      panel_settings_render(g, app);
      panel_treesitter_inspector_render(g, app);
      panel_workspace_symbols_render(g, app);
      panel_workspace_diagnostics_render(g, app);
      panel_completion_render(g, app);
      panel_notification_render(g, app);
}
