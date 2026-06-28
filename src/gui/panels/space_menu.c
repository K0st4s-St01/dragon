#include <stdio.h>
#include "panel_space_menu.h"
#include "app.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"
#include "document.h"

#include <string.h>
#include <GLFW/glfw3.h>

static bool sm_open = false;
static int sm_scroll = 0;
static bool sm_submenu = false;  /* true when viewing window submenu */

typedef struct {
    const char *key;
    const char *description;
    bool implemented;
} SpaceCommand;

/* ---- Main menu commands ---- */
static SpaceCommand commands[] = {
    /* Files & Buffers */
    {"f", "File picker (workspace root)", true},
    {"F", "File picker (current dir)", true},
    {"o", "File picker ($HOME)", true},
    {"b", "Buffer picker", true},
    {"j", "Jumplist picker", true},

    /* Search */
    {"/", "Global search", true},
    {"?", "Command palette", true},

    /* LSP Features */
    {"k", "Hover documentation", true},
    {"s", "Document symbols", true},
    {"S", "Workspace symbols", true},
    {"d", "Document diagnostics", true},
    {"D", "Workspace diagnostics", true},
    {"r", "Rename symbol", true},
    {"a", "Code actions", true},
    {"h", "Select references", true},
    {"t", "Tree-sitter node", true},
    {"T", "Terminal panel", true},

    /* Editing */
    {"c", "Comment toggle (line)", true},
    {"C", "Comment toggle (block)", true},

    /* Clipboard (System) */
    {"y", "Yank selection to clipboard", true},
    {"Y", "Yank main selection to clipboard", true},
    {"p", "Paste from clipboard (after)", true},
    {"P", "Paste from clipboard (before)", true},
    {"R", "Replace selection with clipboard", true},

    /* Window */
    {"w", "Window management ...", true},
};

#define CMD_COUNT (int)(sizeof(commands) / sizeof(commands[0]))

/* ---- Window submenu commands ---- */
static SpaceCommand window_cmds[] = {
    {"v", "Split vertical",         true},
    {"h", "Split horizontal",       true},
    {"q", "Close split",            true},
    {"j", "Navigate down",          true},
    {"k", "Navigate up",            true},
    {"H", "Navigate left",          true},
    {"L", "Navigate right",         true},
    {"z", "Maximize split",         true},
    {"e", "Equalize splits",        true},
    {"n", "Next window",            true},
    {"p", "Previous window",        true},
};

#define WIN_CMD_COUNT (int)(sizeof(window_cmds) / sizeof(window_cmds[0]))

static const char *get_category(int idx) {
    if (idx == 0)  return "Files & Buffers";
    if (idx == 5)  return "Search";
    if (idx == 7)  return "LSP Features";
    if (idx == 17) return "Editing";
    if (idx == 19) return "Clipboard (System)";
    if (idx == 24) return "Window";
    return "";
}

void panel_space_menu_open(App *app) {
    (void)app;
    sm_open = true;
    sm_scroll = 0;
    sm_submenu = false;
}

void panel_space_menu_close(App *app) {
    (void)app;
    sm_open = false;
    sm_scroll = 0;
    sm_submenu = false;
}

bool panel_space_menu_is_open(void) {
    return sm_open;
}

void panel_space_menu_key(App *app, int key) {
    (void)app;
    if (!sm_open) return;

    int count = sm_submenu ? WIN_CMD_COUNT : CMD_COUNT;
    int max_scroll = count - 8;
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
        case GLFW_KEY_BACKSPACE:
            /* Go back from submenu to main menu */
            if (sm_submenu) {
                sm_submenu = false;
                sm_scroll = 0;
            }
            break;
    }
}

void panel_space_menu_input(App *app, unsigned int c) {
    if (!sm_open) return;
    if (c == ' ') return;

    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);

    char cmd_key[2] = {(char)c, '\0'};
    const char *key_str = NULL;
    if (c == '/') key_str = "/";
    else if (c == '?') key_str = "?";
    else if (c > 32 && c < 127) key_str = cmd_key;

    if (!key_str) {
        panel_space_menu_close(app);
        return;
    }

    /* ---- Submenu mode ---- */
    if (sm_submenu) {
        for (int i = 0; i < WIN_CMD_COUNT; i++) {
            if (strcmp(window_cmds[i].key, key_str) == 0) {
                if (strcmp(key_str, "v") == 0) { app_split_vertical(app); }
                else if (strcmp(key_str, "h") == 0) { app_split_horizontal(app); }
                else if (strcmp(key_str, "q") == 0) { app_close_split(app); }
                else if (strcmp(key_str, "j") == 0) { app_goto_window_down(app); }
                else if (strcmp(key_str, "k") == 0) { app_goto_window_up(app); }
                else if (strcmp(key_str, "H") == 0) { app_goto_window_left(app); }
                else if (strcmp(key_str, "L") == 0) { app_goto_window_right(app); }
                else if (strcmp(key_str, "z") == 0) { app_maximize_window(app); }
                else if (strcmp(key_str, "e") == 0) { app_equalize_windows(app); }
                else if (strcmp(key_str, "n") == 0) { app_next_window(app); }
                else if (strcmp(key_str, "p") == 0) { app_prev_window(app); }

                panel_space_menu_close(app);
                mode->pending_len = 0;
                return;
            }
        }
        panel_space_menu_close(app);
        return;
    }

    /* ---- Main menu mode ---- */
    for (int i = 0; i < CMD_COUNT; i++) {
        if (strcmp(commands[i].key, key_str) == 0) {
            if (!commands[i].implemented) {
                panel_space_menu_close(app);
                return;
            }

            if (strcmp(key_str, "f") == 0) {
                extern void panel_file_browser_open_at(App *, const char *);
                panel_file_browser_open_at(app, app_get_workspace_root(app));
            } else if (strcmp(key_str, "F") == 0) {
                extern void panel_file_browser_open(App *);
                panel_file_browser_open(app);
            } else if (strcmp(key_str, "o") == 0) {
                extern void panel_file_browser_open_at_home(App *);
                panel_file_browser_open_at_home(app);
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
            } else if (strcmp(key_str, "k") == 0) {
                extern void document_lsp_hover(Document *, void *);
                extern void panel_lsp_hover_open(App *);
                document_lsp_hover(doc, app_get_lsp_manager(app));
                panel_lsp_hover_open(app);
            } else if (strcmp(key_str, "s") == 0) {
                extern void panel_symbols_picker_open(App *);
                panel_symbols_picker_open(app);
            } else if (strcmp(key_str, "S") == 0) {
                extern void panel_workspace_symbols_open(App *);
                panel_workspace_symbols_open(app);
            } else if (strcmp(key_str, "d") == 0) {
                extern void panel_lsp_diagnostics_open(App *);
                panel_lsp_diagnostics_open(app);
            } else if (strcmp(key_str, "D") == 0) {
                extern void panel_workspace_diagnostics_open(App *);
                panel_workspace_diagnostics_open(app);
            } else if (strcmp(key_str, "r") == 0) {
                extern void panel_rename_open(App *);
                panel_rename_open(app);
            } else if (strcmp(key_str, "a") == 0) {
                extern void panel_code_actions_open(App *);
                panel_code_actions_open(app);
            } else if (strcmp(key_str, "h") == 0) {
                document_lsp_select_references(doc, app_get_lsp_manager(app));
            } else if (strcmp(key_str, "t") == 0) {
                extern void panel_treesitter_inspector_open(App *);
                panel_treesitter_inspector_open(app);
            } else if (strcmp(key_str, "T") == 0) {
                extern void panel_terminal_open(App *);
                panel_terminal_open(app);
            } else if (strcmp(key_str, "c") == 0) {
                document_comment_toggle(doc);
            } else if (strcmp(key_str, "C") == 0) {
                const LanguageSettings *ls = language_settings_get(doc->language_id);
                if (ls && ls->comment_open && ls->comment_open[0])
                    document_comment_toggle_block(doc, ls->comment_open, ls->comment_close);
                else
                    document_comment_toggle_block(doc, "/*", "*/");
            } else if (strcmp(key_str, "y") == 0) {
                document_yank_to_system_clipboard(doc);
            } else if (strcmp(key_str, "Y") == 0) {
                document_yank_main_to_system_clipboard(doc);
            } else if (strcmp(key_str, "p") == 0) {
                document_paste_from_system_clipboard(doc);
            } else if (strcmp(key_str, "P") == 0) {
                document_paste_before_from_system_clipboard(doc);
            } else if (strcmp(key_str, "R") == 0) {
                document_replace_selection_from_system_clipboard(doc);
            } else if (strcmp(key_str, "w") == 0) {
                /* Open window submenu */
                sm_submenu = true;
                sm_scroll = 0;
                mode->pending_len = 0;
                return;
            }

            panel_space_menu_close(app);
            mode->pending_len = 0;
            return;
        }
    }

    panel_space_menu_close(app);
}

static void render_menu_list(Gui *g, App *app, SpaceCommand *cmds, int count,
                             const char *title, const char *subtitle) {
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);

    float pw = 620.0f;
    float ph = (float)h;
    float px = (float)w - pw;
    float py = 0.0f;

    /* Dim overlay */
    renderer_draw_rect(r, 0, 0, px, (float)h,
                       0.0f, 0.0f, 0.0f, 0.5f);

    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       0.00f, 0.00f, 0.00f, 0.99f);

    /* Borders */
    renderer_draw_rect(r, px, py, 4, ph,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py, pw, 3,
                       t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px + pw - 2, py, 2, ph,
                       t->accent[0], t->accent[1], t->accent[2], 0.3f);

    /* Title area */
    renderer_draw_rect(r, px, py, pw, 56,
                       0.05f, 0.05f, 0.08f, 0.9f);
    renderer_draw_rect(r, px + 4, py + 56, pw - 8, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.3f);

    font_draw(&g->font, r, title, px + 20, py + 14,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);
    font_draw(&g->font, r, subtitle, px + 20, py + 34,
              t->accent[0] * 0.8f, t->accent[1] * 0.8f, t->accent[2] * 0.8f, 0.8f);

    /* Scrollable list */
    float list_y = py + 62;
    float list_h = ph - 62 - 40;
    float line_h = g->font.glyph_h + 8;
    int visible_lines = (int)(list_h / line_h);

    renderer_draw_rect(r, px + 8, list_y, pw - 8 - 12, list_h,
                       0.0f, 0.0f, 0.0f, 0.3f);

    /* Clamp scroll */
    int max_scroll = count - visible_lines;
    if (max_scroll < 0) max_scroll = 0;
    if (sm_scroll > max_scroll) sm_scroll = max_scroll;
    if (sm_scroll < 0) sm_scroll = 0;

    int start_idx = sm_scroll;
    int rendered = 0;

    for (int i = start_idx; i < count && rendered < visible_lines; i++) {
        SpaceCommand *cmd = &cmds[i];
        float y = list_y + rendered * line_h;

        /* Key binding */
        char key_display[16];
        snprintf(key_display, sizeof(key_display), "%s%s",
                 sm_submenu ? "Space w " : "Space ", cmd->key);

        float kr = cmd->implemented ? t->accent[0] : 0.50f;
        float kg = cmd->implemented ? t->accent[1] : 0.00f;
        float kb = cmd->implemented ? t->accent[2] : 0.50f;

        font_draw(&g->font, r, key_display, px + 30, y,
                  kr, kg, kb, 1.0f);

        /* Description */
        font_draw(&g->font, r, cmd->description, px + 200, y,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2],
                  cmd->implemented ? 1.0f : 0.4f);

        if (!cmd->implemented) {
            font_draw(&g->font, r, "[TODO]", px + pw - 90, y,
                      0.50f, 0.00f, 0.50f, 0.7f);
        }

        rendered++;
    }

    /* Scrollbar */
    if (count > visible_lines) {
        float sb_x = px + pw - 10;
        float sb_h = list_h * visible_lines / count;
        float sb_y = list_y + (list_h - sb_h) * sm_scroll / (count - visible_lines);

        renderer_draw_rect(r, sb_x - 2, list_y, 8, list_h,
                           0.0f, 0.0f, 0.0f, 0.2f);
        renderer_draw_rect(r, sb_x - 2, sb_y, 8, sb_h,
                           t->accent[0], t->accent[1], t->accent[2], 0.8f);
    }

    /* Footer */
    float footer_y = py + ph - 34;
    renderer_draw_rect(r, px, footer_y - 4, pw, 32,
                       0.05f, 0.05f, 0.08f, 0.9f);
    renderer_draw_rect(r, px + 4, footer_y - 4, pw - 8, 1,
                       t->accent[0], t->accent[1], t->accent[2], 0.3f);

    const char *footer = sm_submenu
        ? "Up/Down:Scroll  Type Key:Execute  Backspace:Back  Esc:Cancel"
        : "Up/Down:Scroll  Type Key:Execute  Esc:Cancel";
    font_draw(&g->font, r, footer, px + 20, footer_y,
              t->accent[0] * 0.7f, t->accent[1] * 0.7f, t->accent[2] * 0.7f, 1.0f);
}

void panel_space_menu_render(Gui *g, App *app) {
    if (!sm_open) return;

    if (sm_submenu) {
        render_menu_list(g, app, window_cmds, WIN_CMD_COUNT,
                         "Window Management",
                         "Split, navigate, and manage windows");
    } else {
        render_menu_list(g, app, commands, CMD_COUNT,
                         "Space Mode Commands",
                         "Type a key or use arrows to scroll");
    }
}
