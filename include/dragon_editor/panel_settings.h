#ifndef DE_PANEL_SETTINGS_H
#define DE_PANEL_SETTINGS_H

#include "app.h"
#include "gui.h"

void panel_settings_open(App *app);
void panel_settings_close(App *app);
bool panel_settings_is_open(void);
void panel_settings_key(App *app, int key);
void panel_settings_render(Gui *g, App *app);

#endif
