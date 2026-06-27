#include <stdio.h>
#include "panel_statusbar.h"
#include "app.h"
#include "document.h"
#include "mode.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

extern const char *input_cmd_get(void);

static const char *mode_name(Mode m) {
    switch (m) {
    case MODE_NORMAL:          return " NORMAL ";
    case MODE_INSERT:          return " INSERT ";
    case MODE_SELECT:          return " SELECT ";
    case MODE_VIEW:            return " VIEW ";
    case MODE_COMMAND:         return " : ";
    case MODE_GOTO:            return " GOTO ";
    case MODE_FIND:            return " FIND ";
    case MODE_SEARCH:          return " SEARCH ";
    default:                   return " ? ";
    }
}

void panel_statusbar(Gui *g, App *app, Document *doc, ModeState *mode) {
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);
    float bar_h = g->font.glyph_h + 8.0f;
    float y = (float)h - bar_h;

    /* Background */
    renderer_draw_rect(r, 0, y, (float)w, bar_h,
                       t->status_bg[0], t->status_bg[1],
                       t->status_bg[2], t->status_bg[3]);

    /* In command mode, show the command being typed */
    if (mode_is(mode, MODE_COMMAND)) {
        const char *cmd = input_cmd_get();
        char cmd_display[66];
        snprintf(cmd_display, sizeof(cmd_display), ":%s", cmd);
        font_draw(&g->font, r, cmd_display, 10, y + 4,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        return;
    }

    /* Mode */
    const char *mn = mode_name(mode_get(mode));
    font_draw(&g->font, r, mn, 10, y + 4,
              t->accent[0], t->accent[1], t->accent[2], t->accent[3]);

    /* File name */
    const char *fname = doc->filepath ? doc->filepath : "[No Name]";
    float file_x = 10 + font_text_width(&g->font, mn) + 20;
    font_draw(&g->font, r, fname, file_x, y + 4,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);

    /* Dirty indicator */
    if (doc->dirty) {
        float dirty_x = file_x + font_text_width(&g->font, fname) + 10;
        font_draw(&g->font, r, "[+]", dirty_x, y + 4,
                  t->warning[0], t->warning[1], t->warning[2], t->warning[3]);
    }

    /* Right side: position */
    Cursor *cur = &doc->cursors[0];
    char pos_buf[64];
    snprintf(pos_buf, sizeof(pos_buf), "Ln %d, Col %d", cur->row + 1, cur->col + 1);
    float pos_x = (float)w - font_text_width(&g->font, pos_buf) - 120;
    font_draw(&g->font, r, pos_buf, pos_x, y + 4,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);

    /* Line count */
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%zu lines", buffer_line_count(&doc->buffer));
    float count_x = (float)w - font_text_width(&g->font, count_buf) - 10;
    font_draw(&g->font, r, count_buf, count_x, y + 4,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
