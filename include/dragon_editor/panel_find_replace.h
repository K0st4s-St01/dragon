#ifndef DE_PANEL_FIND_REPLACE_H
#define DE_PANEL_FIND_REPLACE_H

#include "app.h"
#include "document.h"
#include "gui.h"

typedef enum {
    FR_ACTION_FIND,
    FR_ACTION_SELECT,
    FR_ACTION_SPLIT,
    FR_ACTION_KEEP,
    FR_ACTION_REMOVE,
} FindAction;

void panel_find_open(App *app, Document *doc);
void panel_find_open_ex(App *app, Document *doc, FindAction action);
void panel_find_close(App *app);
bool panel_find_is_open(void);
void panel_find_render(Gui *g, App *app, Document *doc);
void panel_find_input(App *app, Document *doc, unsigned int c);
void panel_find_key(App *app, Document *doc, int key);

#endif
