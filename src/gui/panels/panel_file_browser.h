#ifndef DE_PANEL_FILE_BROWSER_H
#define DE_PANEL_FILE_BROWSER_H

#include "app.h"
#include "gui.h"

void panel_file_browser_open(App *app);
void panel_file_browser_open_at(App *app, const char *root);
void panel_file_browser_open_at_home(App *app);
void panel_file_browser_open_save_as(App *app);
void panel_file_browser_open_change_dir(App *app);
void panel_file_browser_open_workspace(App *app);
void panel_file_browser_close(App *app);
bool panel_file_browser_is_open(void);
void panel_file_browser_input(App *app, unsigned int c);
void panel_file_browser_key(App *app, int key);
void panel_file_browser_render(Gui *g, App *app);

#endif
