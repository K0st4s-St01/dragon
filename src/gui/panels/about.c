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
    int h = app_get_height(app);

    float pw = (float)w * 0.34f;
    if (pw < 380.0f) pw = 380.0f;
    if (pw > 480.0f) pw = 480.0f;
    if (pw > (float)w - 32.0f) pw = (float)w - 32.0f;
    float ph = 250.0f;
    float px = (float)w/2 - pw/2;
    float py = (float)h/2 - ph/2;

    renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0, 0, 0, 0.35f);
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 2, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py + 36.0f, pw, 1, t->accent[0], t->accent[1], t->accent[2], 0.28f);

    float lh = g->font.glyph_h + 6;
    float y = py + 14;

    font_draw(&g->font, r, "Dragon Editor", px+14, y,
              t->accent[0], t->accent[1], t->accent[2], t->accent[3]);
    y += lh;
    y += 12;
    font_draw(&g->font, r, "A modal text editor", px+14, y,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
    y += lh;
    font_draw(&g->font, r, "OpenGL 3.3 + GLFW", px+14, y,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
    y += lh;
    font_draw(&g->font, r, "Inspired by Helix", px+14, y,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
    y += lh;
    renderer_draw_rect(r, px+14, y, pw-28, 1, t->accent[0], t->accent[1], t->accent[2], 0.25f);
    y += 8;
    font_draw(&g->font, r, "i       insert mode", px+14, y,
              t->keyword[0], t->keyword[1], t->keyword[2], t->keyword[3]);
    y += lh;
    font_draw(&g->font, r, "v       select mode", px+14, y,
              t->keyword[0], t->keyword[1], t->keyword[2], t->keyword[3]);
    y += lh;
    font_draw(&g->font, r, "g g     go to top", px+14, y,
              t->keyword[0], t->keyword[1], t->keyword[2], t->keyword[3]);
    renderer_draw_rect(r, px, py + ph - 29.0f, pw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.25f);
    font_draw(&g->font, r, "Esc close", px + 14, py + ph - 24,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
