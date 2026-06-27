#ifndef DE_PANEL_COMPLETION_H
#define DE_PANEL_COMPLETION_H

#include "app.h"
#include "gui.h"

void panel_completion_open(App *app);
void panel_completion_close(App *app);
bool panel_completion_is_open(void);
void panel_completion_key(App *app, int key, int mods);
void panel_completion_render(Gui *g, App *app);

#endif
