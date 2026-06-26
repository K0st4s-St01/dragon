#include <stdio.h>
#include "panel_space_menu.h"
#include "app.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <string.h>
#include <GLFW/glfw3.h>

static bool sm_open = false;

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
    {"k", "Hover documentation", false},
    {"s", "Document symbols", false},
    {"S", "Workspace symbols", false},
    {"d", "Document diagnostics", false},
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
}

void panel_space_menu_close(App *app) {
    (void)app;
    sm_open = false;
}

bool panel_space_menu_is_open(void) {
    return sm_open;
}

void panel_space_menu_render(Gui *g, App *app) {
    if (!sm_open) return;
    
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);
    
    float pw = 400.0f;
    float ph = 500.0f;
    float px = (float)w - pw - 20;
    float py = 40.0f;
    
    /* Semi-transparent overlay on left side */
    renderer_draw_rect(r, 0, 0, (float)w - pw - 20, (float)h,
                       0.0f, 0.0f, 0.0f, 0.3f);
    
    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    
    /* Left accent border */
    renderer_draw_rect(r, px, py, 2, ph,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Title */
    font_draw(&g->font, r, "Space Mode", px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);
    
    /* Subtitle */
    font_draw(&g->font, r, "Press a key to execute command", px + 14, py + 30,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    
    /* Commands list */
    float list_y = py + 55;
    float line_h = g->font.glyph_h + 6;
    
    const char *current_category = "";
    float y = list_y;
    
    for (int i = 0; i < CMD_COUNT; i++) {
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
            font_draw(&g->font, r, category, px + 14, y,
                      t->accent[0] * 0.8f, t->accent[1] * 0.8f, t->accent[2] * 0.8f, 1.0f);
            y += line_h + 2;
        }
        
        /* Key binding */
        char key_str[8];
        snprintf(key_str, sizeof(key_str), "Space %s", cmd->key);
        
        float key_color_r = cmd->implemented ? t->menu_fg[0] : t->gutter_fg[0];
        float key_color_g = cmd->implemented ? t->menu_fg[1] : t->gutter_fg[1];
        float key_color_b = cmd->implemented ? t->menu_fg[2] : t->gutter_fg[2];
        
        font_draw(&g->font, r, key_str, px + 24, y,
                  key_color_r, key_color_g, key_color_b, 1.0f);
        
        /* Description */
        float desc_x = px + 110;
        font_draw(&g->font, r, cmd->description, desc_x, y,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], 
                  cmd->implemented ? 1.0f : 0.5f);
        
        /* Status indicator */
        if (!cmd->implemented) {
            const char *status = "[NYI]";
            float status_x = px + pw - 60;
            font_draw(&g->font, r, status, status_x, y,
                      t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], 0.7f);
        }
        
        y += line_h;
        
        if (y > py + ph - 40) break; /* Don't overflow panel */
    }
    
    /* Footer */
    float footer_y = py + ph - 24;
    renderer_draw_rect(r, px, footer_y - 4, pw, 1,
                       t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], 0.3f);
    font_draw(&g->font, r, "Press Esc to cancel", px + 14, footer_y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
