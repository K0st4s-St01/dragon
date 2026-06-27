#ifndef DE_LSP_CONFIG_H
#define DE_LSP_CONFIG_H

#include "lsp.h"

/* Load LSP server configurations from a file */
void lsp_config_load(LSPManager *manager, const char *config_path);

/* Default configurations for common languages */
void lsp_config_load_defaults(LSPManager *manager);

#endif
