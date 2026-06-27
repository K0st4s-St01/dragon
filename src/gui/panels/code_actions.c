#include "dragon_editor/panel_code_actions.h"
#include "dragon_editor/gui.h"
#include "dragon_editor/app.h"
#include "dragon_editor/document.h"
#include "dragon_editor/lsp.h"
#include "dragon_editor/theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <GLFW/glfw3.h>

static bool code_actions_open = false;
static int code_actions_selected = 0;
static int code_actions_scroll = 0;

typedef struct {
    char *title;
    char *kind;
    void *edit;  /* LSPWorkspaceEdit* */
} CodeAction;

static CodeAction *code_actions = NULL;
static int code_actions_count = 0;
static int code_actions_capacity = 0;
static LSPClient *code_actions_pending_client = NULL;

void panel_code_actions_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    if (!doc || !doc->language_id) return;
    
    /* Clear previous actions */
    for (int i = 0; i < code_actions_count; i++) {
        free(code_actions[i].title);
    }
    code_actions_count = 0;
    code_actions_scroll = 0;
    code_actions_selected = 0;
    
    /* Get current cursor position for code action range */
    int line = doc->cursors[0].row;
    int col = doc->cursors[0].col;
    
    /* Send code action request to LSP */
    LSPManager *lsp_manager = app_get_lsp_manager(app);
    if (lsp_manager) {
        LSPClient *client = lsp_manager_get_client(lsp_manager, doc->language_id);
        if (client) {
            char file_uri[1024];
            snprintf(file_uri, sizeof(file_uri), "file://%s", doc->filepath);
            
            /* Request code actions for the current position (or selection if present) */
            int start_line = line, start_col = col;
            int end_line = line, end_col = col;
            
            if (doc->cursors[0].has_selection) {
                int sel_start_row, sel_start_col, sel_end_row, sel_end_col;
                cursor_normalize(&doc->cursors[0], &sel_start_row, &sel_start_col, &sel_end_row, &sel_end_col);
                start_line = sel_start_row;
                start_col = sel_start_col;
                end_line = sel_end_row;
                end_col = sel_end_col;
            }
            
             lsp_client_send_code_action_request(client, file_uri, start_line, start_col, end_line, end_col);
             code_actions_pending_client = client;
        }
    }
    
    code_actions_open = true;
}

void panel_code_actions_close(App *app) {
    (void)app;
    code_actions_open = false;
    code_actions_pending_client = NULL;
    
    /* Free code actions */
    for (int i = 0; i < code_actions_count; i++) {
        free(code_actions[i].title);
        free(code_actions[i].kind);
        if (code_actions[i].edit) {
            lsp_free_workspace_edit((LSPWorkspaceEdit *)code_actions[i].edit);
        }
    }
    free(code_actions);
    code_actions = NULL;
    code_actions_count = 0;
    code_actions_capacity = 0;
}

bool panel_code_actions_is_open(void) {
    return code_actions_open;
}

void panel_code_actions_key(App *app, int key) {
    if (!code_actions_open) return;
    
    switch (key) {
    case GLFW_KEY_DOWN:
    case GLFW_KEY_J:
    case GLFW_KEY_TAB:
        if (code_actions_selected < code_actions_count - 1) {
            code_actions_selected++;
            if (code_actions_selected >= code_actions_scroll + 10) {
                code_actions_scroll++;
            }
        }
        break;
    case GLFW_KEY_UP:
    case GLFW_KEY_K:
        if (code_actions_selected > 0) {
            code_actions_selected--;
            if (code_actions_selected < code_actions_scroll) {
                code_actions_scroll--;
            }
        }
        break;
     case GLFW_KEY_ENTER:
         if (code_actions_selected >= 0 && code_actions_selected < code_actions_count) {
             /* Apply the selected code action's edit if available */
             if (code_actions[code_actions_selected].edit) {
                 Document *doc = (Document *)app_get_doc(app);
                 if (doc) {
                     LSPWorkspaceEdit *edit = (LSPWorkspaceEdit *)code_actions[code_actions_selected].edit;
                     document_apply_workspace_edit(doc, edit);
                 }
             }
             panel_code_actions_close(app);
         }
         break;
    case GLFW_KEY_ESCAPE:
        panel_code_actions_close(app);
        break;
    default:
        break;
    }
}

void panel_code_actions_render(Gui *g, App *app) {
    if (!code_actions_open || !g) return;
    
    /* Poll for LSP response if we have a pending request */
    if (code_actions_pending_client && code_actions_count == 0) {
        char *response = lsp_client_read_response(code_actions_pending_client);
        if (response) {
            /* Parse the code actions response */
            LSPCodeActions *actions = lsp_parse_code_actions_response(response);
            if (actions && actions->count > 0) {
                /* Store actions for display */
                code_actions_count = actions->count;
                code_actions_capacity = actions->count;
                code_actions = (CodeAction *)malloc(code_actions_capacity * sizeof(CodeAction));
                
                for (int i = 0; i < actions->count; i++) {
                    code_actions[i].title = actions->actions[i].title;
                    code_actions[i].kind = actions->actions[i].kind;
                    code_actions[i].edit = actions->actions[i].edit;
                    /* Prevent double-free by clearing the actions struct */
                    actions->actions[i].title = NULL;
                    actions->actions[i].kind = NULL;
                    actions->actions[i].edit = NULL;
                }
            }
            
            if (actions) lsp_free_code_actions(actions);
            free(response);
            code_actions_pending_client = NULL;
        }
    }
    
    /* Show loading state while waiting */
    if (code_actions_count == 0) {
        Theme *theme = theme_get();
        Renderer *r = app_get_renderer(app);
        int w = app_get_width(app);
        int h = app_get_height(app);
        
        float pw = 400.0f;
        float ph = 80.0f;
        float px = (float)w / 2 - pw / 2;
        float py = (float)h / 2 - ph / 2;
        
        renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0.0f, 0.0f, 0.0f, 0.5f);
        renderer_draw_rect(r, px, py, pw, ph,
                           theme->menu_bg[0], theme->menu_bg[1], theme->menu_bg[2], theme->menu_bg[3]);
        renderer_draw_rect(r, px, py, pw, 2, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
        renderer_draw_rect(r, px, py+ph-2, pw, 2, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
        renderer_draw_rect(r, px, py, 2, ph, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
        renderer_draw_rect(r, px+pw-2, py, 2, ph, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
        
        const char *status = code_actions_pending_client ? "Loading code actions..." : "No code actions available";
        font_draw(&g->font, r, status, px + 14, py + 25,
                  theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
        return;
    }
    
    Theme *theme = theme_get();
    Renderer *r = app_get_renderer(app);
    int w = app_get_width(app);
    int h = app_get_height(app);
    
    float pw = 600.0f;
    float ph = 400.0f;
    float px = (float)w / 2 - pw / 2;
    float py = (float)h / 2 - ph / 2;
    
    /* Dim the rest of the screen */
    renderer_draw_rect(r, 0, 0, (float)w, (float)h,
                       0.0f, 0.0f, 0.0f, 0.5f);
    
    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       theme->menu_bg[0], theme->menu_bg[1], theme->menu_bg[2], theme->menu_bg[3]);
    
    /* Border */
    renderer_draw_rect(r, px, py, pw, 2, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    renderer_draw_rect(r, px, py+ph-2, pw, 2, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    renderer_draw_rect(r, px, py, 2, ph, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    renderer_draw_rect(r, px+pw-2, py, 2, ph, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    
    /* Title */
    font_draw(&g->font, r, "Code Actions", px + 14, py + 10,
              theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    
    /* Items */
    int max_items = 15;
    float item_y = py + 35;
    float item_height = 20;
    
    for (int i = code_actions_scroll; i < code_actions_count && i < code_actions_scroll + max_items; i++) {
        float y = item_y + (i - code_actions_scroll) * item_height;
        
        if (y + item_height > py + ph - 10) break;
        
        /* Highlight selected item */
        if (i == code_actions_selected) {
            renderer_draw_rect(r, px + 5, y, pw - 10, item_height,
                              theme->menu_selected[0], theme->menu_selected[1], 
                              theme->menu_selected[2], theme->menu_selected[3]);
        }
        
        /* Draw action title */
        font_draw(&g->font, r, code_actions[i].title, px + 20, y + 3,
                  theme->menu_fg[0], theme->menu_fg[1], theme->menu_fg[2], 1.0f);
    }
}
