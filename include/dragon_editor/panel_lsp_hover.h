#ifndef DE_PANEL_LSP_HOVER_H
#define DE_PANEL_LSP_HOVER_H

#include "app.h"
#include "gui.h"
#include "lsp.h"

void panel_lsp_hover_request(App *app);
void panel_lsp_hover_open(App *app);
void panel_lsp_hover_close(App *app);
bool panel_lsp_hover_is_open(void);
bool panel_lsp_hover_key(App *app, int key, int mods);
bool panel_lsp_hover_handle_lsp_response(App *app, LSPClient *client, int response_id, const char *response);
void panel_lsp_hover_render(Gui *g, App *app);

#endif
