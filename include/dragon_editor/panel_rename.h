#ifndef DE_PANEL_RENAME_H
#define DE_PANEL_RENAME_H

#include "app.h"
#include "gui.h"
#include "lsp.h"
#include <stdbool.h>

void panel_rename_open(App *app);
void panel_rename_close(App *app);
bool panel_rename_is_open(void);
void panel_rename_key(App *app, int key);
bool panel_rename_handle_lsp_response(App *app, LSPClient *client, int response_id, const char *response);
void panel_rename_render(Gui *g, App *app);

#endif
