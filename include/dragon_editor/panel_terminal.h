#ifndef DE_PANEL_TERMINAL_H
#define DE_PANEL_TERMINAL_H

#include "app.h"
#include "gui.h"
#include <stdbool.h>

void panel_terminal_open(App *app);
void panel_terminal_close(App *app);
bool panel_terminal_is_open(void);
void panel_terminal_key(App *app, int key, int mods);
void panel_terminal_input(App *app, unsigned int c);
void panel_terminal_render(Gui *g, App *app);

#endif
