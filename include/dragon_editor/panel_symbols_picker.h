#ifndef DE_PANEL_SYMBOLS_PICKER_H
#define DE_PANEL_SYMBOLS_PICKER_H

#include "app.h"
#include "gui.h"

void panel_symbols_picker_open(App *app);
void panel_symbols_picker_close(App *app);
bool panel_symbols_picker_is_open(void);
void panel_symbols_picker_key(App *app, int key);
void panel_symbols_picker_render(Gui *g, App *app);

#endif
