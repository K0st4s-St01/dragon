#ifndef DE_PANEL_CHANGED_FILES_H
#define DE_PANEL_CHANGED_FILES_H

#include "app.h"
#include "gui.h"
#include <stdbool.h>

void panel_changed_files_open(App *app);
void panel_changed_files_close(App *app);
bool panel_changed_files_is_open(void);
void panel_changed_files_key(App *app, int key);
void panel_changed_files_render(Gui *g, App *app);

#endif
