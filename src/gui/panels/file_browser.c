#include <stdio.h>
#include "panel_file_browser.h"
#include "app.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <string.h>
#include <GLFW/glfw3.h>

static bool fb_open = false;
static char fb_path[512] = {0};
static int  fb_path_len = 0;
static double fb_last_blink = 0;
static bool fb_cursor_visible = true;

void panel_file_browser_open(App *app) {
    (void)app;
    fb_open = true;
    fb_path[0] = '\0';
    fb_path_len = 0;
    fb_last_blink = glfwGetTime();
    fb_cursor_visible = true;
}

void panel_file_browser_close(App *app) {
    (void)app;
    fb_open = false;
}

bool panel_file_browser_is_open(void) {
    return fb_open;
}

void panel_file_browser_render(Gui *g, App *app) {
    if (!fb_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);

    double now = glfwGetTime();
    if (now - fb_last_blink > 0.5) {
        fb_cursor_visible = !fb_cursor_visible;
        fb_last_blink = now;
    }

    float pw = 500, ph = 120;
    float px = (float)w/2 - pw/2;
    float py = 80;

    /* Background */
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py+ph-1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px+pw-1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);

    /* Label */
    font_draw(&g->font, r, "Open File:", px+14, py+10,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);

    /* Input field */
    float input_y = py + 30;
    float input_h = g->font.glyph_h + 8;
    renderer_draw_rect(r, px+14, input_y, pw-28, input_h,
                       t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);

    char display[514];
    snprintf(display, sizeof(display), "%s%s", fb_path, fb_cursor_visible ? "_" : " ");
    font_draw(&g->font, r, display, px+18, input_y+4,
              t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);

    /* Buttons */
    float btn_y = input_y + input_h + 10;
    float btn_w = 80;

    /* Open button */
    renderer_draw_rect(r, px+14, btn_y, btn_w, g->font.glyph_h + 8,
                       t->accent[0], t->accent[1], t->accent[2], 1);
    font_draw(&g->font, r, "Open", px+34, btn_y+4,
              1, 1, 1, 1);

    /* Cancel button */
    renderer_draw_rect(r, px+110, btn_y, btn_w, g->font.glyph_h + 8,
                       t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);
    font_draw(&g->font, r, "Cancel", px+124, btn_y+4,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
}
