#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "panel_notification.h"
#include "app.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#define MAX_NOTIFICATIONS 8
#define NOTIFLifetime 3.0f
#define NOTIF_FADE_TIME 0.5f

typedef struct {
    char     message[256];
    float    lifetime;
    NotifSeverity severity;
} Notification;

static Notification notifications[MAX_NOTIFICATIONS];
static int notification_count = 0;

void notification_push(NotifSeverity severity, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    /* If at capacity, drop oldest */
    if (notification_count >= MAX_NOTIFICATIONS) {
        memmove(&notifications[0], &notifications[1],
                sizeof(Notification) * (MAX_NOTIFICATIONS - 1));
        notification_count = MAX_NOTIFICATIONS - 1;
    }

    Notification *n = &notifications[notification_count++];
    n->severity = severity;
    n->lifetime = NOTIFLifetime;
    vsnprintf(n->message, sizeof(n->message), fmt, args);

    va_end(args);
}

void notification_update(double dt) {
    for (int i = notification_count - 1; i >= 0; i--) {
        notifications[i].lifetime -= (float)dt;
        if (notifications[i].lifetime <= 0.0f) {
            memmove(&notifications[i], &notifications[i + 1],
                    sizeof(Notification) * (notification_count - i - 1));
            notification_count--;
        }
    }
}

static void notification_draw_fit(Gui *g, Renderer *r, const char *text,
                                  float x, float right, float y,
                                  float cr, float cg, float cb, float ca) {
    if (!text || !*text || right <= x) return;
    char clipped[256];
    size_t copy = strlen(text);
    if (copy >= sizeof(clipped)) copy = sizeof(clipped) - 1;
    memcpy(clipped, text, copy);
    clipped[copy] = '\0';
    size_t len = strlen(clipped);
    while (len > 4 && x + font_text_width(&g->font, clipped) > right) {
        clipped[--len] = '\0';
        if (len > 3) {
            clipped[len - 3] = '.';
            clipped[len - 2] = '.';
            clipped[len - 1] = '.';
        }
    }
    font_draw(&g->font, r, clipped, x, y, cr, cg, cb, ca);
}

void panel_notification_render(Gui *g, App *app) {
    if (notification_count == 0) return;

    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);

    float pad = 10.0f;
    float notif_h = g->font.glyph_h + 12.0f;
    float gap = 6.0f;
    float start_y = 40.0f;

    for (int i = 0; i < notification_count; i++) {
        Notification *n = &notifications[i];

        /* Fade out in last 0.5s */
        float alpha = n->lifetime > NOTIF_FADE_TIME
                      ? 1.0f
                      : n->lifetime / NOTIF_FADE_TIME;
        if (alpha < 0.0f) alpha = 0.0f;

        float max_w = (float)w * 0.46f;
        if (max_w < 280.0f) max_w = (float)w - 24.0f;
        if (max_w > 560.0f) max_w = 560.0f;
        float tw = font_text_width(&g->font, n->message);
        float nw = tw + pad * 2 + 6.0f;  /* 6 for left accent */
        if (nw > max_w) nw = max_w;
        if (nw < 220.0f && (float)w > 260.0f) nw = 220.0f;
        float nx = (float)w - nw - 12.0f;
        if (nx < 12.0f) nx = 12.0f;
        float ny = start_y + i * (notif_h + gap);

        /* Background */
        renderer_draw_rect(r, nx, ny, nw, notif_h,
                           t->menu_bg[0], t->menu_bg[1],
                           t->menu_bg[2], t->menu_bg[3] * alpha);

        /* Left accent border — color by severity */
        float *border_c;
        switch (n->severity) {
        case NOTIF_ERROR:   border_c = t->error;   break;
        case NOTIF_WARNING: border_c = t->warning;  break;
        case NOTIF_SUCCESS: border_c = t->function_color; break;
        default:            border_c = t->accent;   break;
        }
        renderer_draw_rect(r, nx, ny, 3, notif_h,
                           border_c[0], border_c[1], border_c[2], alpha);

        /* Text */
        notification_draw_fit(g, r, n->message, nx + pad + 3, nx + nw - pad, ny + 4,
                              t->fg[0], t->fg[1], t->fg[2], alpha);
    }
}
