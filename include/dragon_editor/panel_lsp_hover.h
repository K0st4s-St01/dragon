#ifndef DE_PANEL_LSP_HOVER_H
#define DE_PANEL_LSP_HOVER_H

#include "app.h"
#include "gui.h"

void panel_lsp_hover_open(App *app);
void panel_lsp_hover_close(App *app);
bool panel_lsp_hover_is_open(void);
bool panel_lsp_hover_key(App *app, int key, int mods);
void panel_lsp_hover_render(Gui *g, App *app);

#endif
