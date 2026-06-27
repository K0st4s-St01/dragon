#ifndef DE_PANEL_LSP_DIAGNOSTICS_H
#define DE_PANEL_LSP_DIAGNOSTICS_H

#include "app.h"
#include "gui.h"

void panel_lsp_diagnostics_open(App *app);
void panel_lsp_diagnostics_close(App *app);
bool panel_lsp_diagnostics_is_open(void);
void panel_lsp_diagnostics_key(App *app, int key);
void panel_lsp_diagnostics_render(Gui *g, App *app);

#endif
