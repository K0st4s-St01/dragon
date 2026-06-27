#ifndef DE_PANEL_SPACE_MENU_H
#define DE_PANEL_SPACE_MENU_H

#include "app.h"
#include "gui.h"

void panel_space_menu_open(App *app);
void panel_space_menu_close(App *app);
bool panel_space_menu_is_open(void);
void panel_space_menu_key(App *app, int key);
void panel_space_menu_input(App *app, unsigned int c);
void panel_space_menu_render(Gui *g, App *app);

#endif
