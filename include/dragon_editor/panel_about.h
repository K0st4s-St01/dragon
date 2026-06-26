#ifndef DE_PANEL_ABOUT_H
#define DE_PANEL_ABOUT_H

#include "app.h"
#include "gui.h"

void panel_about_open(App *app);
void panel_about_close(App *app);
bool panel_about_is_open(void);
void panel_about_render(Gui *g, App *app);

#endif
