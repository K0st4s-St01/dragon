#include "dragon_editor/panel_symbols_picker.h"
#include "dragon_editor/gui.h"
#include "dragon_editor/app.h"
#include "dragon_editor/document.h"
#include "dragon_editor/treesitter.h"
#include "dragon_editor/theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <GLFW/glfw3.h>

static bool symbols_picker_open = false;
static int symbols_selected = 0;
static int symbols_scroll = 0;

typedef struct {
    char *name;
    int line;
    const char *kind;  /* "function", "class", "variable", etc */
} Symbol;

static Symbol *symbols = NULL;
static int symbols_count = 0;
static int symbols_capacity = 0;

void panel_symbols_picker_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    if (!doc || !doc->language_id) return;
    
    /* Clear previous symbols */
    for (int i = 0; i < symbols_count; i++) {
        free(symbols[i].name);
    }
    symbols_count = 0;
    symbols_scroll = 0;
    symbols_selected = 0;
    
    /* Try to extract symbols using TreeSitter */
    void *ts_manager_void = app_get_treesitter_manager(app);
    if (ts_manager_void) {
        TreeSitterManager *ts_manager = (TreeSitterManager *)ts_manager_void;
        TreeSitterLanguage *lang = treesitter_get_language(ts_manager, doc->language_id);
        if (lang) {
            TreeSitterSymbols syms = treesitter_extract_symbols(lang);
            
            /* Add symbols to our picker list */
            for (uint32_t i = 0; i < syms.count && i < 1000; i++) {
                if (symbols_count >= symbols_capacity) {
                    symbols_capacity = (symbols_capacity == 0) ? 64 : symbols_capacity * 2;
                    symbols = (Symbol *)realloc(symbols, symbols_capacity * sizeof(Symbol));
                }
                
                symbols[symbols_count].name = strdup(syms.symbols[i].name);
                symbols[symbols_count].line = syms.symbols[i].start_row;
                symbols[symbols_count].kind = syms.symbols[i].kind;
                symbols_count++;
            }
            
            treesitter_symbols_free(&syms);
        }
    }
    
    symbols_picker_open = true;
}

void panel_symbols_picker_close(App *app) {
    (void)app;
    symbols_picker_open = false;
}

bool panel_symbols_picker_is_open(void) {
    return symbols_picker_open;
}

void panel_symbols_picker_key(App *app, int key) {
    if (!symbols_picker_open) return;
    
    switch (key) {
    case GLFW_KEY_DOWN:
    case GLFW_KEY_J:
    case GLFW_KEY_TAB:
        if (symbols_selected < symbols_count - 1) {
            symbols_selected++;
            if (symbols_selected >= symbols_scroll + 10) {
                symbols_scroll++;
            }
        }
        break;
    case GLFW_KEY_UP:
    case GLFW_KEY_K:
        if (symbols_selected > 0) {
            symbols_selected--;
            if (symbols_selected < symbols_scroll) {
                symbols_scroll--;
            }
        }
        break;
    case GLFW_KEY_ENTER:
        if (symbols_selected >= 0 && symbols_selected < symbols_count) {
            Document *doc = (Document *)app_get_doc(app);
            document_cursor_to(doc, symbols[symbols_selected].line, 0);
            document_sync_viewport_to_cursor(doc);
            panel_symbols_picker_close(app);
        }
        break;
    case GLFW_KEY_ESCAPE:
        panel_symbols_picker_close(app);
        break;
    default:
        break;
    }
}

void panel_symbols_picker_render(Gui *g, App *app) {
    if (!symbols_picker_open || !g) return;
    if (symbols_count == 0) return;
    
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
    font_draw(&g->font, r, "Document Symbols", px + 14, py + 10,
              theme->accent[0], theme->accent[1], theme->accent[2], 1.0f);
    
    /* Items */
    int max_items = 15;
    float item_y = py + 35;
    float item_height = 20;
    
    for (int i = symbols_scroll; i < symbols_count && i < symbols_scroll + max_items; i++) {
        float y = item_y + (i - symbols_scroll) * item_height;
        
        if (y + item_height > py + ph - 10) break;
        
        /* Highlight selected item */
        if (i == symbols_selected) {
            renderer_draw_rect(r, px + 5, y, pw - 10, item_height,
                              theme->menu_selected[0], theme->menu_selected[1], 
                              theme->menu_selected[2], theme->menu_selected[3]);
        }
        
        /* Draw symbol kind indicator and name */
        char kind_char = ' ';
        if (strcmp(symbols[i].kind, "function") == 0) kind_char = 'f';
        else if (strcmp(symbols[i].kind, "class") == 0) kind_char = 'c';
        else if (strcmp(symbols[i].kind, "variable") == 0) kind_char = 'v';
        else if (strcmp(symbols[i].kind, "method") == 0) kind_char = 'm';
        else if (strcmp(symbols[i].kind, "field") == 0) kind_char = 'd';
        
        char buf[256];
        snprintf(buf, sizeof(buf), "[%c] %s", kind_char, symbols[i].name);
        
        font_draw(&g->font, r, buf, px + 20, y + 3,
                  theme->menu_fg[0], theme->menu_fg[1], theme->menu_fg[2], 1.0f);
    }
}
