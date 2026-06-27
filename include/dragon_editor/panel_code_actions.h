#ifndef DE_PANEL_CODE_ACTIONS_H
#define DE_PANEL_CODE_ACTIONS_H

#include "app.h"
#include "gui.h"
#include <stdbool.h>

void panel_code_actions_open(App *app);
void panel_code_actions_close(App *app);
bool panel_code_actions_is_open(void);
void panel_code_actions_key(App *app, int key);
void panel_code_actions_render(Gui *g, App *app);

#endif
