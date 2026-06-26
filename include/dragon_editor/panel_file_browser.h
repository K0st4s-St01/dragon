#ifndef DE_PANEL_FILE_BROWSER_H
#define DE_PANEL_FILE_BROWSER_H

#include "app.h"
#include "gui.h"

void panel_file_browser_open(App *app);
void panel_file_browser_close(App *app);
bool panel_file_browser_is_open(void);
void panel_file_browser_render(Gui *g, App *app);

#endif
