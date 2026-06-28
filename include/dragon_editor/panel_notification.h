#ifndef DE_PANEL_NOTIFICATION_H
#define DE_PANEL_NOTIFICATION_H

#include "app.h"
#include "gui.h"

typedef enum {
    NOTIF_INFO = 0,
    NOTIF_WARNING = 1,
    NOTIF_ERROR = 2,
    NOTIF_SUCCESS = 3,
} NotifSeverity;

void notification_push(NotifSeverity severity, const char *fmt, ...);
void notification_update(double dt);
void panel_notification_render(Gui *g, App *app);

#endif
