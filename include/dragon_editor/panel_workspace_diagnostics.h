#ifndef DE_PANEL_WORKSPACE_DIAGNOSTICS_H
#define DE_PANEL_WORKSPACE_DIAGNOSTICS_H

#include "app.h"
#include "gui.h"

void panel_workspace_diagnostics_open(App *app);
void panel_workspace_diagnostics_close(App *app);
bool panel_workspace_diagnostics_is_open(void);
void panel_workspace_diagnostics_key(App *app, int key);
void panel_workspace_diagnostics_render(Gui *g, App *app);

#endif
