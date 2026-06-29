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
static int code_actions_pending_id = -1;

static void clear_code_actions(void) {
    for (int i = 0; i < code_actions_count; i++) {
        free(code_actions[i].title);
        free(code_actions[i].kind);
        if (code_actions[i].edit)
            lsp_free_workspace_edit((LSPWorkspaceEdit *)code_actions[i].edit);
    }
    free(code_actions);
    code_actions = NULL;
    code_actions_count = 0;
    code_actions_capacity = 0;
}

void panel_code_actions_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    if (!doc || !doc->language_id) return;

    clear_code_actions();
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
             code_actions_pending_id = client->id - 1;
        }
    }
    
    code_actions_open = true;
}

void panel_code_actions_close(App *app) {
    (void)app;
    code_actions_open = false;
    code_actions_pending_client = NULL;
    code_actions_pending_id = -1;
    clear_code_actions();
}

bool panel_code_actions_is_open(void) {
    return code_actions_open;
}

static void code_actions_draw_fit(Gui *g, Renderer *r, const char *text,
                                  float x, float right, float y,
                                  float cr, float cg, float cb, float ca) {
    if (!text || !*text || right <= x) return;
    char clipped[256];
    size_t copy = strlen(text);
    if (copy >= sizeof(clipped)) copy = sizeof(clipped) - 1;
    memcpy(clipped, text, copy);
    clipped[copy] = '\0';
    size_t len = strlen(clipped);
    while (len > 4 && x + font_text_width(&g->font, clipped) > right) {
        clipped[--len] = '\0';
        if (len > 3) {
            clipped[len - 3] = '.';
            clipped[len - 2] = '.';
            clipped[len - 1] = '.';
        }
    }
    font_draw(&g->font, r, clipped, x, y, cr, cg, cb, ca);
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
                     document_notify_lsp_change(doc, app_get_lsp_manager(app));
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

bool panel_code_actions_handle_lsp_response(LSPClient *client, int response_id, const char *response) {
    if (!code_actions_pending_client || client != code_actions_pending_client ||
        response_id != code_actions_pending_id) {
        return false;
    }

    LSPCodeActions *actions = lsp_parse_code_actions_response(response);
    if (actions && actions->count > 0) {
        code_actions_count = actions->count;
        code_actions_capacity = actions->count;
        code_actions = (CodeAction *)calloc((size_t)code_actions_capacity, sizeof(CodeAction));
        if (code_actions) {
            for (int i = 0; i < actions->count; i++) {
                code_actions[i].title = actions->actions[i].title;
                code_actions[i].kind = actions->actions[i].kind;
                code_actions[i].edit = actions->actions[i].edit;
                actions->actions[i].title = NULL;
                actions->actions[i].kind = NULL;
                actions->actions[i].edit = NULL;
            }
        } else {
            code_actions_count = 0;
            code_actions_capacity = 0;
        }
    }
    if (actions) lsp_free_code_actions(actions);
    code_actions_pending_client = NULL;
    code_actions_pending_id = -1;
    return true;
}

void panel_code_actions_render(Gui *g, App *app) {
    if (!code_actions_open || !g) return;

    Theme *theme = theme_get();
    Renderer *r = app_get_renderer(app);
    int w = app_get_width(app);
    int h = app_get_height(app);

    float available_w = (float)w - 48.0f;
    if (available_w < 260.0f) available_w = (float)w - 16.0f;
    if (available_w < 120.0f) available_w = 120.0f;
    
    /* Show loading state while waiting */
    if (code_actions_count == 0) {
        float pw = 420.0f;
        if (pw > available_w) pw = available_w;
        float ph = 80.0f;
        float px = (float)w / 2 - pw / 2;
        float py = (float)h / 2 - ph / 2;
        
        renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0.0f, 0.0f, 0.0f, 0.5f);
        renderer_draw_rect(r, px, py, pw, ph,
                           theme->menu_bg[0], theme->menu_bg[1], theme->menu_bg[2], theme->menu_bg[3]);
        renderer_draw_rect(r, px, py, pw, 2, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
        renderer_draw_rect(r, px, py + 34.0f, pw, 1.0f,
                           theme->accent[0], theme->accent[1], theme->accent[2], 0.28f);
        
        const char *status = code_actions_pending_client ? "Loading code actions..." : "No code actions available";
        code_actions_draw_fit(g, r, status, px + 14, px + pw - 14, py + 25,
                              theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
        return;
    }

    float pw = (float)w * 0.52f;
    if (pw < 460.0f) pw = 460.0f;
    if (pw > 700.0f) pw = 700.0f;
    if (pw > available_w) pw = available_w;
    float ph = (float)h * 0.54f;
    if (ph < 260.0f) ph = 260.0f;
    if (ph > (float)h - 80.0f) ph = (float)h - 80.0f;
    if (ph < 140.0f) ph = (float)h - 24.0f;
    float px = (float)w / 2 - pw / 2;
    float py = (float)h / 2 - ph / 2;
    
    /* Dim the rest of the screen */
    renderer_draw_rect(r, 0, 0, (float)w, (float)h,
                       0.0f, 0.0f, 0.0f, 0.5f);
    
    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       theme->menu_bg[0], theme->menu_bg[1], theme->menu_bg[2], theme->menu_bg[3]);
    
    renderer_draw_rect(r, px, py, pw, 2, theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    renderer_draw_rect(r, px, py + 36.0f, pw, 1,
                       theme->accent[0], theme->accent[1], theme->accent[2], 0.28f);
    
    /* Title */
    font_draw(&g->font, r, "Code Actions", px + 14, py + 10,
              theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    
    /* Items */
    float item_y = py + 48;
    float item_height = 20;
    int max_items = (int)((ph - 86.0f) / item_height);
    if (max_items < 1) max_items = 1;
    
    for (int i = code_actions_scroll; i < code_actions_count && i < code_actions_scroll + max_items; i++) {
        float y = item_y + (i - code_actions_scroll) * item_height;
        
        if (y + item_height > py + ph - 34) break;
        
        /* Highlight selected item */
        if (i == code_actions_selected) {
            renderer_draw_rect(r, px + 5, y, pw - 10, item_height,
                              theme->menu_selected[0], theme->menu_selected[1], 
                              theme->menu_selected[2], theme->menu_selected[3]);
        }
        
        /* Draw action title */
        code_actions_draw_fit(g, r, code_actions[i].title, px + 20, px + pw - 20, y + 3,
                              theme->menu_fg[0], theme->menu_fg[1], theme->menu_fg[2], 1.0f);
    }

    float help_y = py + ph - 24.0f;
    renderer_draw_rect(r, px, help_y - 5.0f, pw, 1,
                       theme->accent[0], theme->accent[1], theme->accent[2], 0.25f);
    font_draw(&g->font, r, "Enter apply  Esc close  Up/Down move", px + 14, help_y,
              theme->gutter_fg[0], theme->gutter_fg[1], theme->gutter_fg[2], theme->gutter_fg[3]);
}
