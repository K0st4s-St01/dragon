#include <stdio.h>
#include "panel_goto.h"
#include "app.h"
#include "document.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GLFW/glfw3.h>

static bool goto_open = false;
static char goto_buf[32] = {0};
static int  goto_len = 0;
static double goto_last_blink = 0;
static bool goto_cursor_visible = true;

void panel_goto_open(App *app, Document *doc) {
    (void)doc; (void)app;
    goto_open = true;
    goto_buf[0] = '\0';
    goto_len = 0;
    goto_last_blink = glfwGetTime();
    goto_cursor_visible = true;
}

void panel_goto_close(App *app) {
    (void)app;
    goto_open = false;
}

bool panel_goto_is_open(void) {
    return goto_open;
}

void panel_goto_render(Gui *g, App *app, Document *doc) {
    (void)doc;
    if (!goto_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);

    double now = glfwGetTime();
    if (now - goto_last_blink > 0.5) {
        goto_cursor_visible = !goto_cursor_visible;
        goto_last_blink = now;
    }

    float pw = 300, ph = 70;
    float px = (float)w/2 - pw/2;
    float py = 120;

    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py+ph-1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px+pw-1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);

    font_draw(&g->font, r, "Go to line:", px+14, py+10,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);

    renderer_draw_rect(r, px+100, py+6, pw-114, g->font.glyph_h+8,
                       t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);

    char display[34];
    snprintf(display, sizeof(display), "%s%s", goto_buf, goto_cursor_visible ? "_" : " ");
    font_draw(&g->font, r, display, px+104, py+10,
              t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
}
