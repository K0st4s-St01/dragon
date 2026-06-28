#include "dragon_editor/panel_rename.h"
#include "dragon_editor/gui.h"
#include "dragon_editor/app.h"
#include "dragon_editor/document.h"
#include "dragon_editor/lsp.h"
#include "dragon_editor/theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <GLFW/glfw3.h>

static bool rename_open = false;
static char rename_buffer[512] = {0};
static int rename_cursor = 0;
static int rename_line = 0;
static int rename_col = 0;
static char rename_file_uri[1024] = {0};
static char *rename_language_id = NULL;
static LSPClient *rename_pending_client = NULL;  /* Track pending LSP request */
static int rename_pending_id = -1;
static char rename_status[256] = {0};  /* Status message for the user */
static double rename_status_time = 0;  /* Time when status was set */

void panel_rename_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    if (!doc || !doc->filepath) return;
    
    /* Get cursor position from the first cursor */
    rename_line = doc->cursors[0].row;
    rename_col = doc->cursors[0].col;
    
    /* Build file URI from document path */
    snprintf(rename_file_uri, sizeof(rename_file_uri), "file://%s", doc->filepath);
    
    /* Store language ID for later */
    if (rename_language_id) free(rename_language_id);
    rename_language_id = doc->language_id ? strdup(doc->language_id) : NULL;
    
    /* Clear input buffer */
    memset(rename_buffer, 0, sizeof(rename_buffer));
    rename_cursor = 0;
    rename_status[0] = '\0';
    rename_status_time = 0;
    
    rename_open = true;
}

void panel_rename_close(App *app) {
    (void)app;
    rename_open = false;
    memset(rename_buffer, 0, sizeof(rename_buffer));
    rename_cursor = 0;
    rename_pending_client = NULL;
    rename_pending_id = -1;
    rename_status[0] = '\0';
    rename_status_time = 0;
}

bool panel_rename_is_open(void) {
    return rename_open;
}

void panel_rename_key(App *app, int key) {
    if (!rename_open) return;
    
    switch (key) {
    case GLFW_KEY_ENTER:
        if (rename_cursor > 0) {
            /* Send rename request to LSP */
            LSPManager *lsp_manager = app_get_lsp_manager(app);
            if (lsp_manager && rename_language_id) {
                LSPClient *client = lsp_manager_get_client(lsp_manager, rename_language_id);
                if (client && client->status == LSP_STATUS_INITIALIZED) {
                    lsp_client_send_rename_request(client, rename_file_uri, rename_line, rename_col, rename_buffer);
                    rename_pending_client = client;
                    rename_pending_id = client->id - 1;
                    char display_name[128];
                    size_t display_len = strlen(rename_buffer);
                    bool truncated = display_len >= sizeof(display_name);
                    if (truncated)
                        display_len = sizeof(display_name) - 4;
                    memcpy(display_name, rename_buffer, display_len);
                    if (truncated) {
                        memcpy(display_name + display_len, "...", 4);
                    } else {
                        display_name[display_len] = '\0';
                    }
                    snprintf(rename_status, sizeof(rename_status), "Renaming to '%s'...", display_name);
                    rename_status_time = 0;  /* Will be set by render function */
                    /* Keep panel open while waiting for response */
                    return;
                }
            }
            panel_rename_close(app);
        }
        break;
    case GLFW_KEY_ESCAPE:
        panel_rename_close(app);
        break;
    case GLFW_KEY_BACKSPACE:
        if (rename_cursor > 0) {
            rename_cursor--;
            rename_buffer[rename_cursor] = '\0';
        }
        break;
    default:
        /* Handle printable characters */
        if (key >= 32 && key < 127 && rename_cursor < 511) {
            rename_buffer[rename_cursor] = (char)key;
            rename_cursor++;
            rename_buffer[rename_cursor] = '\0';
        }
        break;
    }
}

bool panel_rename_handle_lsp_response(App *app, LSPClient *client, int response_id, const char *response) {
    if (!rename_pending_client || client != rename_pending_client || response_id != rename_pending_id)
        return false;

    LSPWorkspaceEdit *edit = lsp_parse_rename_response(response);
    if (edit && edit->count > 0) {
        Document *doc = (Document *)app_get_doc(app);
        if (doc) {
            document_apply_workspace_edit(doc, edit);
            document_notify_lsp_change(doc, app_get_lsp_manager(app));
            snprintf(rename_status, sizeof(rename_status),
                     "Renamed successfully! (%d locations)", edit->count);
        }
    } else {
        snprintf(rename_status, sizeof(rename_status), "Rename completed (no changes)");
    }

    lsp_free_workspace_edit(edit);
    rename_pending_client = NULL;
    rename_pending_id = -1;
    rename_status_time = 3.0;
    return true;
}

void panel_rename_render(Gui *g, App *app) {
    if (!rename_open || !g) return;

    /* Auto-close after status is shown */
    if (rename_status_time > 0) {
        rename_status_time -= (double)app_get_dt(app);
        if (rename_status_time <= 0 && rename_pending_client == NULL) {
            panel_rename_close(app);
            return;
        }
    }
    
    Theme *theme = theme_get();
    Renderer *r = app_get_renderer(app);
    int w = app_get_width(app);
    int h = app_get_height(app);
    
    float pw = 400.0f;
    float ph = (rename_pending_client || rename_status[0]) ? 130.0f : 100.0f;
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
    if (rename_pending_client) {
        font_draw(&g->font, r, "Waiting for LSP...", px + 14, py + 10,
                  theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    } else {
        font_draw(&g->font, r, "Rename Symbol", px + 14, py + 10,
                  theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    }
    
    /* Input field background */
    renderer_draw_rect(r, px + 10, py + 35, pw - 20, 25,
                       0.1f, 0.1f, 0.1f, 0.8f);
    
    /* Draw text input and cursor (only if not waiting) */
    if (!rename_pending_client) {
        font_draw(&g->font, r, rename_buffer, px + 15, py + 40,
                  theme->menu_fg[0], theme->menu_fg[1], theme->menu_fg[2], 1.0f);
        
        /* Draw cursor */
        float cursor_x = px + 15 + rename_cursor * 8; /* Estimate character width */
        renderer_draw_rect(r, cursor_x, py + 40, 2, 15,
                           theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    }
    
    /* Draw status message if available */
    if (rename_status[0] != '\0') {
        font_draw(&g->font, r, rename_status, px + 15, py + 70,
                  theme->menu_fg[0], theme->menu_fg[1], theme->menu_fg[2], 1.0f);
    }
}
