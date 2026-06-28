#ifndef DE_PANEL_COMPLETION_H
#define DE_PANEL_COMPLETION_H

#include "app.h"
#include "gui.h"
#include "lsp.h"

void panel_completion_open(App *app);
void panel_completion_close(App *app);
bool panel_completion_is_open(void);
void panel_completion_key(App *app, int key, int mods);
bool panel_completion_handle_lsp_response(LSPClient *client, int response_id, const char *response);
void panel_completion_render(Gui *g, App *app);

#endif
