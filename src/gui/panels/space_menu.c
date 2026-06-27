#include <stdio.h>
#include "panel_space_menu.h"
#include "app.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <string.h>
#include <GLFW/glfw3.h>

static bool sm_open = false;
static int sm_scroll = 0;  /* Scroll offset */

typedef struct {
    const char *key;
    const char *description;
    bool implemented;
} SpaceCommand;

static SpaceCommand commands[] = {
    /* File operations */
    {"f", "File picker (workspace root)", false},
    {"F", "File picker (current dir)", true},
    {"b", "Buffer picker", true},
    {"j", "Jumplist picker", true},
    
    /* Search & Navigation */
    {"/", "Global search", true},
    {"?", "Command palette", true},
    {"g", "Changed files (git)", false},
    
    /* LSP features */
    {"k", "Hover documentation", true},
    {"s", "Document symbols", false},
    {"S", "Workspace symbols", false},
    {"d", "Document diagnostics", true},
    {"D", "Workspace diagnostics", false},
    {"r", "Rename symbol", false},
    {"a", "Code actions", false},
    {"h", "Highlight references", false},
    
    /* Editing */
    {"c", "Comment toggle", true},
    {"C", "Block comment", false},
    
    /* Clipboard */
    {"y", "Yank to clipboard", false},
    {"Y", "Yank primary selection", false},
    {"p", "Paste after", false},
    {"P", "Paste before", false},
    {"R", "Replace with clipboard", false},
    
    /* Modes */
    {"w", "Window mode", false},
};

#define CMD_COUNT (int)(sizeof(commands) / sizeof(commands[0]))

void panel_space_menu_open(App *app) {
    (void)app;
    sm_open = true;
    sm_scroll = 0;
}

void panel_space_menu_close(App *app) {
    (void)app;
    sm_open = false;
    sm_scroll = 0;
}

bool panel_space_menu_is_open(void) {
    return sm_open;
}

void panel_space_menu_key(App *app, int key) {
    (void)app;
    if (!sm_open) return;
    
    int max_scroll = CMD_COUNT - 8;  /* Show ~8 items at a time */
    if (max_scroll < 0) max_scroll = 0;
    
    switch (key) {
        case GLFW_KEY_UP:
            if (sm_scroll > 0) sm_scroll--;
            break;
        case GLFW_KEY_DOWN:
            if (sm_scroll < max_scroll) sm_scroll++;
            break;
        case GLFW_KEY_PAGE_UP:
            sm_scroll -= 5;
            if (sm_scroll < 0) sm_scroll = 0;
            break;
        case GLFW_KEY_PAGE_DOWN:
            sm_scroll += 5;
            if (sm_scroll > max_scroll) sm_scroll = max_scroll;
            break;
    }
}

void panel_space_menu_input(App *app, unsigned int c) {
    if (!sm_open) return;
    
    /* Ignore space key - it just opens/closes the menu */
    if (c == ' ') return;
    
    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);
    
    /* Convert character to key string for matching */
    char cmd_key[2] = {(char)c, '\0'};
    
    /* Handle special key characters */
    const char *key_str = NULL;
    if (c == '/') key_str = "/";
    else if (c == '?') key_str = "?";
    else if (c > 32 && c < 127) {
        key_str = cmd_key;
    }
    
    if (!key_str) {
        panel_space_menu_close(app);
        return;
    }
    
    /* Find and execute matching command */
    for (int i = 0; i < CMD_COUNT; i++) {
        if (strcmp(commands[i].key, key_str) == 0) {
            /* Command found - execute if implemented */
            if (!commands[i].implemented) {
                /* Not implemented */
                panel_space_menu_close(app);
                return;
            }
            
            /* Execute command based on key */
            if (strcmp(key_str, "f") == 0) {
                extern void panel_file_browser_open_at(App *, const char *);
                const char *workspace = app_get_workspace_root(app);
                panel_file_browser_open_at(app, workspace);
            } else if (strcmp(key_str, "F") == 0) {
                extern void panel_file_browser_open(App *);
                panel_file_browser_open(app);
            } else if (strcmp(key_str, "b") == 0) {
                extern void panel_buffer_picker_open(App *);
                panel_buffer_picker_open(app);
            } else if (strcmp(key_str, "j") == 0) {
                extern void panel_jumplist_picker_open(App *);
                panel_jumplist_picker_open(app);
            } else if (strcmp(key_str, "/") == 0) {
                extern void panel_find_open(App *, Document *);
                panel_find_open(app, doc);
            } else if (strcmp(key_str, "?") == 0) {
                extern void panel_palette_open(App *);
                panel_palette_open(app);
                mode_set(mode, MODE_COMMAND_PALETTE);
            } else if (strcmp(key_str, "k") == 0) {
                extern void document_lsp_hover(Document *, void *);
                document_lsp_hover(doc, app_get_lsp_manager(app));
                extern void panel_lsp_hover_open(App *);
                panel_lsp_hover_open(app);
            } else if (strcmp(key_str, "d") == 0) {
                extern void panel_lsp_diagnostics_open(App *);
                panel_lsp_diagnostics_open(app);
            } else if (strcmp(key_str, "c") == 0) {
                extern void document_comment_toggle(Document *);
                document_comment_toggle(doc);
            }
            
            panel_space_menu_close(app);
            mode->pending_len = 0;
            return;
        }
    }
    
    /* Command not found */
    panel_space_menu_close(app);
}

void panel_space_menu_render(Gui *g, App *app) {
    if (!sm_open) return;
    
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);
    
    /* Panel dimensions - full height, right side */
    float pw = 620.0f;
    float ph = (float)h;
    float px = (float)w - pw;
    float py = 0.0f;
    
    /* Darker overlay on left side for more contrast */
    renderer_draw_rect(r, 0, 0, px, (float)h,
                       0.0f, 0.0f, 0.0f, 0.5f);
    
    /* Panel background - pure black for evil look */
    renderer_draw_rect(r, px, py, pw, ph,
                       0.00f, 0.00f, 0.00f, 0.99f);
    
    /* Thick evil magenta left accent border */
    renderer_draw_rect(r, px, py, 4, ph,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Top accent border too */
    renderer_draw_rect(r, px, py, pw, 3,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Right border */
    renderer_draw_rect(r, px + pw - 2, py, 2, ph,
                       t->accent[0], t->accent[1], t->accent[2], 0.3f);
    
    /* Title area background */
    renderer_draw_rect(r, px, py, pw, 56,
                       0.05f, 0.05f, 0.08f, 0.9f);
    
    /* Separator after title */
    renderer_draw_rect(r, px + 4, py + 56, pw - 8, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.3f);
    
    /* Title - bigger and more visible */
    font_draw(&g->font, r, "Space Mode Commands", px + 20, py + 14,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Subtitle */
    font_draw(&g->font, r, "Use ↑↓ to scroll, type key, or press Esc", px + 20, py + 34,
              t->accent[0] * 0.8f, t->accent[1] * 0.8f, t->accent[2] * 0.8f, 0.8f);
    
    /* Scrollable list area - between title and footer */
    float list_y = py + 62;
    float list_h = ph - 62 - 40;  /* Leave room for footer (40px) */
    float line_h = g->font.glyph_h + 8;
    int visible_lines = (int)(list_h / line_h);
    
    /* Draw scrollable area boundary */
    renderer_draw_rect(r, px + 8, list_y, pw - 8 - 12, list_h,
                       0.0f, 0.0f, 0.0f, 0.3f);
    
    /* Render visible items */
    const char *current_category = "";
    int line_count = 0;
    float y = list_y;
    int start_idx = 0;
    int rendered = 0;
    
    for (int i = 0; i < CMD_COUNT; i++) {
        /* Count category headers */
        const char *category = "";
        if (i == 0) category = "Files & Buffers";
        else if (i == 4) category = "Search";
        else if (i == 7) category = "LSP Features";
        else if (i == 15) category = "Editing";
        else if (i == 17) category = "Clipboard";
        else if (i == 22) category = "Modes";
        
        if (category[0] && strcmp(category, current_category) != 0) {
            current_category = category;
            line_count++;  /* Category header takes a line */
        }
        
        if (line_count > sm_scroll) {
            start_idx = i;
            break;
        }
        line_count++;
    }
    
    /* Reset for actual rendering */
    current_category = "";
    line_count = 0;
    for (int i = 0; i < start_idx; i++) {
        const char *category = "";
        if (i == 0) category = "Files & Buffers";
        else if (i == 4) category = "Search";
        else if (i == 7) category = "LSP Features";
        else if (i == 15) category = "Editing";
        else if (i == 17) category = "Clipboard";
        else if (i == 22) category = "Modes";
        
        if (category[0] && strcmp(category, current_category) != 0) {
            current_category = category;
        }
    }
    
    /* Render from scroll position */
    y = list_y;
    for (int i = start_idx; i < CMD_COUNT && rendered < visible_lines; i++) {
        SpaceCommand *cmd = &commands[i];
        
        /* Category headers */
        const char *category = "";
        if (i == 0) category = "Files & Buffers";
        else if (i == 4) category = "Search";
        else if (i == 7) category = "LSP Features";
        else if (i == 15) category = "Editing";
        else if (i == 17) category = "Clipboard";
        else if (i == 22) category = "Modes";
        
        if (category[0] && strcmp(category, current_category) != 0) {
            current_category = category;
            y += 8;
            
            /* Category background */
            renderer_draw_rect(r, px + 12, y - 2, pw - 28, line_h - 2,
                               0.08f, 0.00f, 0.08f, 0.4f);
            
            /* Category header */
            font_draw(&g->font, r, category, px + 20, y,
                      t->accent[0], t->accent[1], t->accent[2], 1.0f);
            
            y += line_h + 6;
            rendered++;
            if (rendered >= visible_lines) break;
        }
        
        /* Key binding */
        char key_str[16];
        snprintf(key_str, sizeof(key_str), "Space %s", cmd->key);
        
        float key_color_r = cmd->implemented ? t->accent[0] : 0.50f;
        float key_color_g = cmd->implemented ? t->accent[1] : 0.00f;
        float key_color_b = cmd->implemented ? t->accent[2] : 0.50f;
        
        font_draw(&g->font, r, key_str, px + 30, y,
                  key_color_r, key_color_g, key_color_b, 1.0f);
        
        /* Description */
        float desc_x = px + 180;
        font_draw(&g->font, r, cmd->description, desc_x, y,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], 
                  cmd->implemented ? 1.0f : 0.4f);
        
        /* Status indicator */
        if (!cmd->implemented) {
            const char *status = "[TODO]";
            float status_x = px + pw - 90;
            font_draw(&g->font, r, status, status_x, y,
                      0.50f, 0.00f, 0.50f, 0.7f);
        }
        
        y += line_h;
        rendered++;
    }
    
    /* Scrollbar */
    if (CMD_COUNT > visible_lines) {
        float scrollbar_x = px + pw - 10;
        float scrollbar_h = list_h * visible_lines / CMD_COUNT;
        float scrollbar_y = list_y + (list_h - scrollbar_h) * sm_scroll / (CMD_COUNT - visible_lines);
        
        /* Scrollbar background */
        renderer_draw_rect(r, scrollbar_x - 2, list_y, 8, list_h,
                           0.0f, 0.0f, 0.0f, 0.2f);
        
        /* Scrollbar thumb */
        renderer_draw_rect(r, scrollbar_x - 2, scrollbar_y, 8, scrollbar_h,
                           t->accent[0], t->accent[1], t->accent[2], 0.8f);
    }
    
    /* Footer area - at bottom of panel */
    float footer_y = py + ph - 34;
    
    /* Footer background */
    renderer_draw_rect(r, px, footer_y - 4, pw, 32,
                       0.05f, 0.05f, 0.08f, 0.9f);
    
    /* Separator before footer */
    renderer_draw_rect(r, px + 4, footer_y - 4, pw - 8, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.3f);
    
    /* Footer text */
    font_draw(&g->font, r, "↑↓:Scroll  Type Key:Execute  Esc:Cancel", px + 20, footer_y,
              t->accent[0] * 0.7f, t->accent[1] * 0.7f, t->accent[2] * 0.7f, 1.0f);
}
