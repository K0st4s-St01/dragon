#ifndef DE_PANEL_TREESITTER_INSPECTOR_H
#define DE_PANEL_TREESITTER_INSPECTOR_H

#include "app.h"
#include "gui.h"

void panel_treesitter_inspector_open(App *app);
void panel_treesitter_inspector_close(App *app);
bool panel_treesitter_inspector_is_open(void);
void panel_treesitter_inspector_key(App *app, int key);
void panel_treesitter_inspector_render(Gui *g, App *app);

#endif
