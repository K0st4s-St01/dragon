#include <stdio.h>
#include "panel_palette.h"
#include "app.h"
#include "command.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <string.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>

static bool   palette_open = false;
static char   palette_query[256] = {0};
static int    palette_len = 0;
static int    palette_selected = 0;
#define PALETTE_MAX_RESULTS 128
static Command *palette_results[PALETTE_MAX_RESULTS];
static int    palette_result_count = 0;

typedef struct {
    const char *label;
    const char *binding;
    const char *category;
    CommandFn   fn;
} PaletteEntry;

static PaletteEntry entries[] = {
    /* File */
    {"Open file",           "ctrl+o",    "File", cmd_open},
    {"Save",                "ctrl+s",    "File", cmd_save},
    {"Save as",             "",          "File", cmd_save_as},
    {"Quit",                "ctrl+q",    "File", cmd_quit},
    {"Go to alternate file","ctrl+^",    "File", cmd_goto_alternate},
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

void panel_palette_open(App *app) {
    (void)app;
    palette_open = true;
    palette_query[0] = '\0';
    palette_len = 0;
    palette_selected = 0;
    palette_result_count = ENTRY_COUNT < PALETTE_MAX_RESULTS ? ENTRY_COUNT : PALETTE_MAX_RESULTS;
    for (int i = 0; i < palette_result_count; i++)
        palette_results[i] = (Command *)&entries[i];
}

void panel_palette_close(App *app) {
    (void)app;
    palette_open = false;
}

bool panel_palette_is_open(void) {
    return palette_open;
}

static void update_results(void) {
    if (palette_query[0] == '\0') {
        palette_result_count = ENTRY_COUNT < PALETTE_MAX_RESULTS ? ENTRY_COUNT : PALETTE_MAX_RESULTS;
        for (int i = 0; i < palette_result_count; i++)
            palette_results[i] = (Command *)&entries[i];
    } else {
        palette_result_count = 0;
        for (int i = 0; i < ENTRY_COUNT && palette_result_count < PALETTE_MAX_RESULTS; i++) {
            if (strstr(entries[i].label, palette_query) ||
                strstr(entries[i].category, palette_query)) {
                palette_results[palette_result_count++] = (Command *)&entries[i];
            }
        }
    }
    if (palette_selected >= palette_result_count)
        palette_selected = palette_result_count > 0 ? palette_result_count - 1 : 0;
}

void panel_palette_input(App *app, unsigned int c) {
    (void)app;
    if (!palette_open) return;
    if (c == 8 && palette_len > 0) {
        palette_query[--palette_len] = '\0';
    } else if (c >= 32 && c < 127 && palette_len < 255) {
        palette_query[palette_len++] = (char)c;
        palette_query[palette_len] = '\0';
    }
    update_results();
}

void panel_palette_key(App *app, int key) {
    (void)app;
    if (!palette_open) return;
    if (key == GLFW_KEY_UP) {
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
            PaletteEntry *e = (PaletteEntry *)palette_results[palette_selected];
            if (e->fn) e->fn(app);
        }
        panel_palette_close(app);
    }
}

void panel_palette_render(Gui *g, App *app) {
    if (!palette_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);

    float pw = 320.0f;
    float ph = (float)h - 60.0f;
    float px = (float)w - pw;
    float py = 30.0f;

    /* Dim the rest of the screen */
    renderer_draw_rect(r, 0, 0, (float)w, (float)h,
                       0.0f, 0.0f, 0.0f, 0.5f);

    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);

    /* Left border accent line */
    renderer_draw_rect(r, px, py, 1, ph,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);

    /* Search input at top */
    float input_y = py + 10;
    float input_h = g->font.glyph_h + 8;
    renderer_draw_rect(r, px + 8, input_y, pw - 16, input_h,
                       t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);

    char display_buf[260];
    snprintf(display_buf, sizeof(display_buf), "> %s_", palette_query);
    font_draw(&g->font, r, display_buf, px + 12, input_y + 4,
              t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);

    /* Results list */
    float result_y = input_y + input_h + 6;
    float line_h = g->font.glyph_h + 4;
    int max_visible = (int)((ph - input_h - 20) / line_h);
    int start = 0;
    if (palette_selected >= max_visible)
        start = palette_selected - max_visible + 1;

    const char *last_cat = "";

    for (int i = start; i < palette_result_count && (i - start) < max_visible; i++) {
        PaletteEntry *e = (PaletteEntry *)palette_results[i];
        float ry = result_y + (i - start) * line_h;
        bool sel = (i == palette_selected);

        /* Category header */
        if (strcmp(e->category, last_cat) != 0) {
            ry += 4;
            font_draw(&g->font, r, e->category, px + 12, ry,
                      t->accent[0], t->accent[1], t->accent[2], 1.0f);
            ry += line_h;
            last_cat = e->category;
        }

        /* Selection highlight */
        if (sel) {
            renderer_draw_rect(r, px + 4, ry - 1, pw - 8, line_h,
                               t->menu_selected[0], t->menu_selected[1],
                               t->menu_selected[2], t->menu_selected[3]);
        }

        /* Label */
        font_draw(&g->font, r, e->label, px + 12, ry,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);

        /* Keybinding on the right */
        if (e->binding[0]) {
            float bw = font_text_width(&g->font, e->binding);
            font_draw(&g->font, r, e->binding, px + pw - bw - 14, ry,
                      t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
        }
    }
}
