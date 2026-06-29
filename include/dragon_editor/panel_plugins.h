#ifndef DE_PANEL_PLUGINS_H
#define DE_PANEL_PLUGINS_H

#include "app.h"
#include "gui.h"

void panel_plugins_open(App *app);
void panel_plugins_close(App *app);
bool panel_plugins_is_open(void);
void panel_plugins_key(App *app, int key);
void panel_plugins_render(Gui *g, App *app);

#endif
