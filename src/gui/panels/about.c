#include <stdio.h>
#include "panel_about.h"
#include "app.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

static bool about_open = false;

void panel_about_open(App *app) {
    (void)app;
    about_open = true;
}

void panel_about_close(App *app) {
    (void)app;
    about_open = false;
}

bool panel_about_is_open(void) {
    return about_open;
}

void panel_about_render(Gui *g, App *app) {
    if (!about_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);

    float pw = 400, ph = 220;
    float px = (float)w/2 - pw/2;
    float py = 150;

    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py+ph-1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px+pw-1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);

    float lh = g->font.glyph_h + 6;
    float y = py + 14;

    font_draw(&g->font, r, "Dragon Editor", px+14, y,
              t->accent[0], t->accent[1], t->accent[2], t->accent[3]);
    y += lh;
    renderer_draw_rect(r, px+14, y, pw-28, 1, t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    y += 8;
    font_draw(&g->font, r, "A modal text editor", px+14, y,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
    y += lh;
    font_draw(&g->font, r, "OpenGL 3.3 + GLFW", px+14, y,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
    y += lh;
    font_draw(&g->font, r, "Inspired by Helix", px+14, y,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
    y += lh;
    renderer_draw_rect(r, px+14, y, pw-28, 1, t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    y += 8;
    font_draw(&g->font, r, "SPACE   command palette", px+14, y,
              t->keyword[0], t->keyword[1], t->keyword[2], t->keyword[3]);
    y += lh;
    font_draw(&g->font, r, "i       insert mode", px+14, y,
              t->keyword[0], t->keyword[1], t->keyword[2], t->keyword[3]);
    y += lh;
    font_draw(&g->font, r, "v       select mode", px+14, y,
              t->keyword[0], t->keyword[1], t->keyword[2], t->keyword[3]);
    y += lh;
    font_draw(&g->font, r, "g g     go to top", px+14, y,
              t->keyword[0], t->keyword[1], t->keyword[2], t->keyword[3]);
    y += lh + 4;

    /* Close button */
    float btn_w = 80;
    renderer_draw_rect(r, px+pw/2-btn_w/2, y, btn_w, g->font.glyph_h+8,
                       t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);
    font_draw(&g->font, r, "Close", px+pw/2-14, y+4,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
}
