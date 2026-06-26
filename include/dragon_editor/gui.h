#ifndef DE_GUI_H
#define DE_GUI_H

#include "app.h"
#include "document.h"
#include "mode.h"
#include "text.h"

typedef struct {
    Font font;
    Font font_bold;
} Gui;

void gui_init(Gui *g);
void gui_free(Gui *g);
void gui_begin(Gui *g);
void gui_end(Gui *g);
void gui_render(Gui *g, App *app, Document *doc, ModeState *mode);

#endif
