#ifndef DE_PANEL_WORKSPACE_SYMBOLS_H
#define DE_PANEL_WORKSPACE_SYMBOLS_H

#include "app.h"
#include "gui.h"

void panel_workspace_symbols_open(App *app);
void panel_workspace_symbols_close(App *app);
bool panel_workspace_symbols_is_open(void);
void panel_workspace_symbols_key(App *app, int key);
void panel_workspace_symbols_render(Gui *g, App *app);

#endif
