#ifndef DE_PANEL_PALETTE_H
#define DE_PANEL_PALETTE_H

#include "app.h"
#include "gui.h"

void panel_palette_open(App *app);
void panel_palette_close(App *app);
bool panel_palette_is_open(void);
void panel_palette_render(Gui *g, App *app);

#endif
