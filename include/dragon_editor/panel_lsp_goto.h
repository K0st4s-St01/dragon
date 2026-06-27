#ifndef DE_PANEL_LSP_GOTO_H
#define DE_PANEL_LSP_GOTO_H

#include "app.h"
#include "gui.h"

void panel_lsp_goto_open(App *app);
void panel_lsp_goto_close(App *app);
bool panel_lsp_goto_is_open(void);
void panel_lsp_goto_key(App *app, int key);
void panel_lsp_goto_render(Gui *g, App *app);

#endif
