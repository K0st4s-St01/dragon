#ifndef DE_PANEL_GOTO_H
#define DE_PANEL_GOTO_H

#include "app.h"
#include "document.h"
#include "gui.h"

void panel_goto_open(App *app, Document *doc);
void panel_goto_close(App *app);
bool panel_goto_is_open(void);
void panel_goto_render(Gui *g, App *app, Document *doc);

#endif
