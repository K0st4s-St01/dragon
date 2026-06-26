#include "dragon_editor/gui.h"
#include "dragon_editor/renderer.h"
#include "dragon_editor/theme.h"
#include "dragon_editor/document.h"
#include "dragon_editor/buffer.h"
#include "dragon_editor/text.h"
#include "dragon_editor/app.h"

#include "panel_statusbar.h"
#include "panel_palette.h"
#include "panel_file_browser.h"
#include "panel_find_replace.h"
#include "panel_goto.h"
#include "panel_about.h"
#include "panel_buffer_picker.h"
#include "panel_jumplist_picker.h"
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

static void render_editor(Gui *g, App *app, Document *doc, ModeState *mode) {
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int win_w = app_get_width(app);
    int win_h = app_get_height(app);

    float gutter_w = 60.0f;
    float line_h = g->font.glyph_h + 4.0f;
    float text_x = gutter_w + 10.0f;
    float text_y = 30.0f;
    float editor_h = (float)win_h - 30.0f;

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

        /* Line highlight */
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

        /* Line number */
        char num[16];
        snprintf(num, sizeof(num), "%4d", line_num + 1);
        font_draw(&g->font, r, num, 8, y + 2,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);

        /* Line text */
        const char *line = buffer_line_ptr(&doc->buffer, line_num);
        int line_len = (int)buffer_line_len(&doc->buffer, line_num);
        while (line_len > 0 && (line[line_len-1] == '\n' || line[line_len-1] == '\r'))
            line_len--;

        char line_buf[4096];
        if (line_len >= (int)sizeof(line_buf)) line_len = (int)sizeof(line_buf) - 1;
        memcpy(line_buf, line, line_len);
        line_buf[line_len] = '\0';

        font_draw(&g->font, r, line_buf, text_x, y + 2,
                  t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
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
                float cr = t->cursor_color[0];
                float cg = t->cursor_color[1];
                float cb = t->cursor_color[2];
                float ca = t->cursor_color[3];
                if (ci > 0) { cr *= 0.7f; cg *= 0.7f; cb *= 0.7f; }
                renderer_draw_rect(r, cx, cy, cw, line_h, cr, cg, cb, ca);
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
    panel_space_menu_render(g, app);
}
