#include <stdio.h>
#include "panel_palette.h"
#include "app.h"
#include "command.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <GLFW/glfw3.h>

static bool   palette_open = false;
static char   palette_query[256] = {0};
static int    palette_len = 0;
static int    palette_selected = 0;
#define PALETTE_MAX_RESULTS 128
static int    palette_result_count = 0;

typedef struct {
    const char *label;
    const char *binding;
    const char *category;
    CommandFn   fn;
} PaletteEntry;

static void palette_terminal(App *app) {
    extern void panel_terminal_open(App *);
    panel_terminal_open(app);
}

static PaletteEntry entries[] = {
    /* File */
    {"Open file",           "ctrl+o",    "File", cmd_open},
    {"Save",                "ctrl+s",    "File", cmd_save},
    {"Save as",             "",          "File", cmd_save_as},
    {"Quit",                "ctrl+q",    "File", cmd_quit},
    {"Go to alternate file","ctrl+^",    "File", cmd_goto_alternate},
    {"Open workspace",      ":open-workspace", "File", cmd_open_workspace},
    {"Change directory",    ":cwd",      "File", cmd_change_dir},
    /* Navigation */
    {"Go to top",           "gg",        "Navigation", cmd_goto_top},
    {"Go to bottom",        "G",         "Navigation", cmd_goto_bottom},
    {"Go to line",          ":N",        "Navigation", cmd_goto_line},
    {"Go to start",         "0",         "Navigation", cmd_goto_start},
    {"Go to end",           "$",         "Navigation", cmd_goto_end},
    {"Go to last mod",      "g.",        "Navigation", cmd_goto_last_mod},
    {"Jumplist backward",   "ctrl+o",    "Navigation", cmd_jumplist_backward},
    {"Jumplist forward",    "ctrl+i",    "Navigation", cmd_jumplist_forward},
    /* Selection */
    {"Select all",          "ctrl+a",    "Selection", cmd_select_all},
    {"Select inside word",  "iw",        "Selection", cmd_select_iw},
    {"Select around word",  "aw",        "Selection", cmd_select_aw},
    {"Select inside parens","i(",        "Selection", cmd_select_iparen},
    {"Select inside braces","i{",        "Selection", cmd_select_icurly},
    /* Edit */
    {"Undo",                "u",         "Edit", cmd_undo},
    {"Redo",                "ctrl+y",    "Edit", cmd_redo},
    {"Delete line",         "dd",        "Edit", cmd_delete_line},
    {"Duplicate line",      "",          "Edit", cmd_duplicate_line},
    {"Move line up",        "",          "Edit", cmd_move_line_up},
    {"Move line down",      "",          "Edit", cmd_move_line_down},
    {"Indent",              ">",         "Edit", cmd_indent},
    {"Unindent",            "<",         "Edit", cmd_unindent},
    {"Find",                "/",         "Edit", cmd_find},
    {"Replace",             "",          "Edit", cmd_replace},
    {"Comment toggle (line)","ctrl+c",   "Edit", cmd_comment_toggle},
    {"Comment toggle (block)","ctrl+shift+c", "Edit", cmd_comment_block},
    {"Reflow text",         ":reflow N", "Edit", cmd_reflow},
    {"Convert to tabs",     ":retab",    "Edit", cmd_retab},
    {"Convert to spaces",   ":expandtab","Edit", cmd_expandtab},
    {"Sort lines",          ":sort",     "Edit", cmd_sort},
    {"Format selection",    "=",         "Edit", cmd_format},
    /* Clipboard (System) */
    {"Yank to clipboard",   "Space y",   "Clipboard", cmd_yank_clipboard},
    {"Yank primary sel",    "Space Y",   "Clipboard", cmd_yank_primary_clipboard},
    {"Paste from clipboard","Space p",   "Clipboard", cmd_paste_clipboard},
    {"Paste before (clip)", "Space P",   "Clipboard", cmd_paste_before_clipboard},
    {"Replace with clipboard","Space R", "Clipboard", cmd_replace_clipboard},
    /* Macros */
    {"Record macro",        "q<letter>", "Macros", cmd_macro_record},
    {"Replay macro",        "@<letter>", "Macros", cmd_macro_replay},
    /* Window */
    {"Split vertical",      "Space w v", "Window", cmd_split_v},
    {"Split horizontal",    "Space w h", "Window", cmd_split_h},
    {"Close split",         "Space w q", "Window", cmd_close_split},
    {"Navigate left",       "Space w H", "Window", cmd_win_left},
    {"Navigate right",      "Space w L", "Window", cmd_win_right},
    {"Navigate up",         "Space w k", "Window", cmd_win_up},
    {"Navigate down",       "Space w j", "Window", cmd_win_down},
    {"Maximize split",      "Space w z", "Window", cmd_win_maximize},
    {"Equalize splits",     "Space w e", "Window", cmd_win_equalize},
    {"Next window",         "Space w n", "Window", cmd_win_next},
    {"Previous window",     "Space w p", "Window", cmd_win_prev},
    /* Tools */
    {"Terminal panel",      "ctrl+~",    "Tools", palette_terminal},
    {"Plugin manager",      ":plugins",  "Tools", cmd_plugins},
    {"Reload config",       ":config-reload", "Tools", cmd_config_reload},
    /* Help */
    {"About",               "",          "Help", cmd_about},
    {"Settings",            "",          "Help", cmd_settings},
    {"Tree-sitter subtree", ":ts-subtree", "LSP", cmd_tree_sitter_inspect},
    {"LSP restart",         ":lsp-restart", "LSP", cmd_lsp_restart},
    {"LSP stop",            ":lsp-stop", "LSP", cmd_lsp_stop},
    {"Workspace symbols",   "Space S",   "LSP", cmd_workspace_symbols},
    {"Workspace diagnostics","Space D",  "LSP", cmd_workspace_diagnostics},
};
#define ENTRY_COUNT (int)(sizeof(entries) / sizeof(entries[0]))

static PaletteEntry *palette_results[PALETTE_MAX_RESULTS];

void panel_palette_open(App *app) {
    (void)app;
    palette_open = true;
    palette_query[0] = '\0';
    palette_len = 0;
    palette_selected = 0;
    palette_result_count = ENTRY_COUNT < PALETTE_MAX_RESULTS ? ENTRY_COUNT : PALETTE_MAX_RESULTS;
    for (int i = 0; i < palette_result_count; i++)
        palette_results[i] = &entries[i];
}

void panel_palette_close(App *app) {
    (void)app;
    palette_open = false;
}

bool panel_palette_is_open(void) {
    return palette_open;
}

static bool palette_text_match(const char *text, const char *query) {
    if (!query || !*query) return true;
    if (!text) return false;

    const char *q = query;
    while (*q) {
        while (*q && isspace((unsigned char)*q)) q++;
        if (!*q) break;

        char token[64];
        int len = 0;
        while (*q && !isspace((unsigned char)*q) && len < (int)sizeof(token) - 1)
            token[len++] = (char)tolower((unsigned char)*q++);
        token[len] = '\0';
        if (!token[0]) continue;

        bool found = false;
        for (const char *p = text; *p; p++) {
            int i = 0;
            while (token[i] && p[i] &&
                   token[i] == (char)tolower((unsigned char)p[i]))
                i++;
            if (!token[i]) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

static bool palette_entry_match(const PaletteEntry *e, const char *query) {
    return palette_text_match(e->label, query) ||
           palette_text_match(e->category, query) ||
           palette_text_match(e->binding, query);
}

static void update_results(void) {
    if (palette_query[0] == '\0') {
        palette_result_count = ENTRY_COUNT < PALETTE_MAX_RESULTS ? ENTRY_COUNT : PALETTE_MAX_RESULTS;
        for (int i = 0; i < palette_result_count; i++)
            palette_results[i] = &entries[i];
    } else {
        palette_result_count = 0;
        for (int i = 0; i < ENTRY_COUNT && palette_result_count < PALETTE_MAX_RESULTS; i++) {
            if (palette_entry_match(&entries[i], palette_query))
                palette_results[palette_result_count++] = &entries[i];
        }
    }
    if (palette_selected >= palette_result_count)
        palette_selected = palette_result_count > 0 ? palette_result_count - 1 : 0;
}

void panel_palette_input(App *app, unsigned int c) {
    (void)app;
    if (!palette_open) return;
    if (c >= 32 && c < 127 && palette_len < 255) {
        palette_query[palette_len++] = (char)c;
        palette_query[palette_len] = '\0';
    }
    update_results();
}

void panel_palette_key(App *app, int key) {
    if (!palette_open) return;
    if (key == GLFW_KEY_BACKSPACE) {
        if (palette_len > 0) {
            palette_query[--palette_len] = '\0';
            update_results();
        }
    } else if (key == GLFW_KEY_UP) {
        if (palette_selected > 0) palette_selected--;
    } else if (key == GLFW_KEY_DOWN) {
        if (palette_selected < palette_result_count - 1) palette_selected++;
    } else if (key == GLFW_KEY_PAGE_UP) {
        palette_selected -= 10;
        if (palette_selected < 0) palette_selected = 0;
    } else if (key == GLFW_KEY_PAGE_DOWN) {
        palette_selected += 10;
        if (palette_selected >= palette_result_count)
            palette_selected = palette_result_count > 1 ? palette_result_count - 1 : 0;
    } else if (key == GLFW_KEY_HOME) {
        palette_selected = 0;
    } else if (key == GLFW_KEY_END) {
        palette_selected = palette_result_count > 1 ? palette_result_count - 1 : 0;
    } else if (key == GLFW_KEY_ENTER) {
        if (palette_result_count > 0) {
            PaletteEntry *e = palette_results[palette_selected];
            if (e->fn) e->fn(app);
        }
        panel_palette_close(app);
    }
}

static void palette_draw_fit(Gui *g, Renderer *r, const char *text,
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

void panel_palette_render(Gui *g, App *app) {
    if (!palette_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);

    float pw = (float)w * 0.56f;
    if (pw < 560.0f) pw = 560.0f;
    if (pw > (float)w - 48.0f) pw = (float)w - 48.0f;
    float ph = (float)h * 0.70f;
    if (ph < 360.0f) ph = 360.0f;
    if (ph > (float)h - 80.0f) ph = (float)h - 80.0f;
    float px = (float)w * 0.5f - pw * 0.5f;
    float py = 42.0f;

    /* Dim the rest of the screen */
    renderer_draw_rect(r, 0, 0, (float)w, (float)h,
                       0.0f, 0.0f, 0.0f, 0.5f);

    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);

    renderer_draw_rect(r, px, py, pw, 2,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py + 42.0f, pw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.28f);

    char title[96];
    snprintf(title, sizeof(title), "Command Palette  %d result%s",
             palette_result_count, palette_result_count == 1 ? "" : "s");
    font_draw(&g->font, r, title, px + 14.0f, py + 12.0f,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);

    /* Search input at top */
    float input_y = py + 52.0f;
    float input_h = g->font.glyph_h + 8;
    renderer_draw_rect(r, px + 14, input_y, pw - 28, input_h,
                       t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);

    char display_buf[260];
    snprintf(display_buf, sizeof(display_buf), "> %s_", palette_query);
    palette_draw_fit(g, r, display_buf, px + 20, px + pw - 20, input_y + 4,
              t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);

    /* Results list */
    float result_y = input_y + input_h + 12.0f;
    float line_h = g->font.glyph_h + 9.0f;
    int max_visible = (int)((ph - input_h - 104.0f) / line_h);
    if (max_visible < 1) max_visible = 1;
    int start = 0;
    if (palette_selected >= max_visible)
        start = palette_selected - max_visible + 1;

    if (palette_result_count == 0) {
        font_draw(&g->font, r, "No commands matched", px + 18.0f, result_y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }

    for (int i = start; i < palette_result_count && (i - start) < max_visible; i++) {
        PaletteEntry *e = palette_results[i];
        float ry = result_y + (i - start) * line_h;
        bool sel = (i == palette_selected);

        if (sel) {
            renderer_draw_rect(r, px + 10, ry - 3, pw - 20, line_h,
                               t->menu_selected[0], t->menu_selected[1],
                               t->menu_selected[2], t->menu_selected[3]);
        }

        palette_draw_fit(g, r, e->category, px + 18, px + 126, ry,
                         t->accent[0], t->accent[1], t->accent[2], 1.0f);

        float binding_left = px + pw - 150.0f;
        palette_draw_fit(g, r, e->label, px + 140, binding_left - 16.0f, ry,
                         t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        if (e->binding[0]) {
            palette_draw_fit(g, r, e->binding, binding_left, px + pw - 18.0f, ry,
                             t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
        }
    }

    float footer_y = py + ph - 26.0f;
    renderer_draw_rect(r, px, footer_y - 5.0f, pw, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.25f);
    font_draw(&g->font, r,
              "Enter run  Up/Down move  PageUp/PageDown jump  Backspace edit  Esc close",
              px + 14.0f, footer_y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
