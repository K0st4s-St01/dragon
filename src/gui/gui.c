#include "dragon_editor/gui.h"
#include "dragon_editor/renderer.h"
#include "dragon_editor/theme.h"
#include "dragon_editor/document.h"
#include "dragon_editor/buffer.h"
#include "dragon_editor/text.h"
#include "dragon_editor/app.h"
#include "dragon_editor/syntax.h"
#include "dragon_editor/lsp.h"

#include "panel_statusbar.h"
#include "panel_palette.h"
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

static void render_editor(Gui *g, App *app, Document *doc, ModeState *mode) {
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int win_w = app_get_width(app);
    int win_h = app_get_height(app);

    float gutter_w = 60.0f;
    float line_h = g->font.glyph_h + 6.0f;  /* Better spacing */
    float text_x = gutter_w + 12.0f;
    float text_y = 32.0f;
    float editor_h = (float)win_h - 32.0f;

    Cursor *cur = &doc->cursors[0];
    size_t lines = buffer_line_count(&doc->buffer);

    /* Background */
    renderer_draw_rect(r, 0, 0, (float)win_w, (float)win_h,
                       t->bg[0], t->bg[1], t->bg[2], t->bg[3]);

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
            renderer_draw_rect(r, 0, y, (float)win_w, line_h,
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
                float x1 = text_x + sel_start * g->font.glyph_w;
                float x2 = text_x + sel_end * g->font.glyph_w;
                renderer_draw_rect(r, x1, y, x2-x1, line_h,
                                   t->selection_bg[0], t->selection_bg[1],
                                   t->selection_bg[2], t->selection_bg[3]);
            }
        }

        /* Gutter background */
        renderer_draw_rect(r, 0, y, gutter_w, line_h,
                           t->gutter_bg[0], t->gutter_bg[1],
                           t->gutter_bg[2], t->gutter_bg[3]);

        /* Gutter separator line */
        renderer_draw_rect(r, gutter_w, y, 1, line_h,
                           t->gutter_fg[0] * 0.3f, t->gutter_fg[1] * 0.3f, 
                           t->gutter_fg[2] * 0.3f, 0.3f);

        /* Line number - right aligned */
        char num[16];
        snprintf(num, sizeof(num), "%4d", line_num + 1);
        font_draw(&g->font, r, num, 10, y + 3,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
        
        /* Diagnostic markers on gutter */
        if (doc->diagnostics) {
            LSPDiagnostics *diag = (LSPDiagnostics *)doc->diagnostics;
            for (int d = 0; d < diag->count; d++) {
                if (diag->items[d].start_line == line_num) {
                    /* Draw diagnostic indicator */
                    float diag_x = 6;
                    float diag_y = y + 3;
                    char indicator = (diag->items[d].severity == LSP_DIAG_ERROR) ? 'E' : 'W';
                    float color_r = (diag->items[d].severity == LSP_DIAG_ERROR) ? 1.0f : 1.0f;
                    float color_g = (diag->items[d].severity == LSP_DIAG_ERROR) ? 0.0f : 1.0f;
                    float color_b = (diag->items[d].severity == LSP_DIAG_ERROR) ? 0.0f : 0.0f;
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
            for (int col = 0; col < line_len; col++) {
                SyntaxType syntax_type = syntax_get_type_at(&doc->syntax, line_num, col);
                float sr, sg, sb, sa;
                get_syntax_color(t, syntax_type, &sr, &sg, &sb, &sa);
                
                char ch[2] = {line[col], '\0'};
                float x = text_x + col * g->font.glyph_w;
                font_draw(&g->font, r, ch, x, y + 2, sr, sg, sb, sa);
            }
        }
    }

    /* Cursors */
    if (mode_is(mode, MODE_INSERT) || mode_is(mode, MODE_NORMAL)) {
        for (int ci = 0; ci < doc->cursor_count; ci++) {
            Cursor *c = &doc->cursors[ci];
            int csr = c->row - first_line;
            if (csr >= 0 && csr < visible_lines) {
                float cx = text_x + c->col * g->font.glyph_w;
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
}

void gui_render(Gui *g, App *app, Document *doc, ModeState *mode) {
    render_editor(g, app, doc, mode);
     panel_statusbar(g, app, doc, mode);
     panel_palette_render(g, app);
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
}
