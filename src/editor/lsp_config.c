#include "dragon_editor/lsp_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void lsp_config_load_defaults(LSPManager *manager) {
    /* C/C++ - using clangd */
    const char *clangd_args[] = {
        "--log=error",
        "--clang-tidy",
        "--header-insertion=never",
        "--function-arg-placeholders=true",
        "--enable-config"
    };
    lsp_manager_add_server(manager, "c", "clangd", clangd_args, 5);
    lsp_manager_add_server(manager, "cpp", "clangd", clangd_args, 5);
    
    /* Objective-C/Objective-C++ - using clangd */
    lsp_manager_add_server(manager, "objc", "clangd", clangd_args, 5);
    lsp_manager_add_server(manager, "objcpp", "clangd", clangd_args, 5);
    
    /* CUDA - using clangd */
    lsp_manager_add_server(manager, "cuda", "clangd", clangd_args, 5);
    
    /* Rust - using rust-analyzer */
    const char *rust_args[] = {};
    lsp_manager_add_server(manager, "rust", "rust-analyzer", rust_args, 0);
    
    /* Python - using pylsp */
    const char *python_args[] = {};
    lsp_manager_add_server(manager, "python", "pylsp", python_args, 0);
    
    /* Go - using gopls */
    const char *go_args[] = {};
    lsp_manager_add_server(manager, "go", "gopls", go_args, 0);
    
    /* JavaScript/TypeScript - using typescript-language-server */
    const char *ts_args[] = {"--stdio"};
    lsp_manager_add_server(manager, "typescript", "typescript-language-server", ts_args, 1);
    lsp_manager_add_server(manager, "javascript", "typescript-language-server", ts_args, 1);
}

void lsp_config_load(LSPManager *manager, const char *config_path) {
    /* TODO: Parse JSON config file and load server configurations
       For now, just load defaults */
    (void)config_path;  /* Unused parameter */
    lsp_config_load_defaults(manager);
}
