#ifndef DE_PANEL_BUFFER_PICKER_H
#define DE_PANEL_BUFFER_PICKER_H

#include "app.h"
#include "gui.h"

void panel_buffer_picker_open(App *app);
void panel_buffer_picker_close(App *app);
bool panel_buffer_picker_is_open(void);
void panel_buffer_picker_render(Gui *g, App *app);
void panel_buffer_picker_key(App *app, int key);

#endif
