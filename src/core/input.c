#include "dragon_editor/input.h"
#include "dragon_editor/app.h"
#include "dragon_editor/document.h"
#include "dragon_editor/mode.h"
#include "dragon_editor/command.h"
#include "panel_file_browser.h"
#include "panel_find_replace.h"
#include "panel_goto.h"
#include "panel_about.h"
#include "panel_buffer_picker.h"
#include "panel_jumplist_picker.h"
#include "panel_lsp_goto.h"
#include "panel_lsp_hover.h"
#include "panel_lsp_diagnostics.h"
#include "panel_space_menu.h"
#include "panel_symbols_picker.h"
#include "panel_rename.h"
#include "panel_code_actions.h"
#include "panel_palette.h"
#include "panel_settings.h"
#include "panel_treesitter_inspector.h"
#include "panel_workspace_symbols.h"
#include "panel_workspace_diagnostics.h"
#include "panel_completion.h"
#include <GLFW/glfw3.h>
#include <string.h>
#include <stdlib.h>

#define CMD_BUF_MAX 1024

static char cmd_buf[CMD_BUF_MAX] = {0};
static int  cmd_len = 0;

void input_cmd_reset(void) {
    cmd_buf[0] = '\0';
    cmd_len = 0;
}

const char *input_cmd_get(void) {
    return cmd_buf;
}

static void handle_normal_key(App *app, int key, int action, int mods);
static void handle_insert_key(App *app, int key, int action, int mods);
static void handle_select_key(App *app, int key, int action, int mods);
static void handle_command_key(App *app, int key, int action, int mods);

void input_handle_key(App *app, int key, int scancode, int action, int mods) {
    (void)scancode;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    ModeState *mode = (ModeState *)app_get_mode(app);

    /* Global escape */
    if (key == GLFW_KEY_ESCAPE) {
        if (panel_file_browser_is_open()) { panel_file_browser_close(app); return; }
        if (panel_find_is_open()) { panel_find_close(app); return; }
        if (panel_goto_is_open()) { panel_goto_close(app); return; }
        if (panel_about_is_open()) { panel_about_close(app); return; }
        if (panel_buffer_picker_is_open()) { panel_buffer_picker_close(app); return; }
        if (panel_jumplist_picker_is_open()) { panel_jumplist_picker_close(app); return; }
        if (panel_lsp_goto_is_open()) { panel_lsp_goto_close(app); return; }
        if (panel_lsp_diagnostics_is_open()) { panel_lsp_diagnostics_close(app); return; }
        if (panel_lsp_hover_is_open()) { panel_lsp_hover_close(app); return; }
        if (panel_symbols_picker_is_open()) { panel_symbols_picker_close(app); return; }
        if (panel_rename_is_open()) { panel_rename_close(app); return; }
        if (panel_code_actions_is_open()) { panel_code_actions_close(app); return; }
        if (panel_space_menu_is_open()) { panel_space_menu_close(app); return; }
        if (panel_palette_is_open()) { panel_palette_close(app); return; }
        if (panel_settings_is_open()) { panel_settings_close(app); return; }
        if (panel_treesitter_inspector_is_open()) { panel_treesitter_inspector_close(app); return; }
        if (panel_workspace_symbols_is_open()) { panel_workspace_symbols_close(app); return; }
        if (panel_workspace_diagnostics_is_open()) { panel_workspace_diagnostics_close(app); return; }
        if (panel_completion_is_open()) { panel_completion_close(app); return; }
        if (!mode_is(mode, MODE_NORMAL)) {
            if (mode_is(mode, MODE_INSERT)) {
                Document *doc = (Document *)app_get_doc(app);
                Cursor *cur = &doc->cursors[0];
                document_jumplist_push(doc, cur->row, cur->col);
            }
            input_cmd_reset();
            mode_set(mode, MODE_NORMAL);
            return;
        }
        /* Exit sticky view mode on Escape */
        if (mode->view_mode_sticky) {
            mode->view_mode_sticky = false;
            mode->pending_key = 0;
            return;
        }
    }

    /* Route keys to find panel when open */
    if (panel_find_is_open()) {
        Document *doc = (Document *)app_get_doc(app);
        panel_find_key(app, doc, key);
        return;
    }

    /* Route keys to file browser when open */
    if (panel_file_browser_is_open()) {
        panel_file_browser_key(app, key);
        return;
    }

    if (panel_settings_is_open()) {
        panel_settings_key(app, key);
        return;
    }

    if (panel_treesitter_inspector_is_open()) {
        panel_treesitter_inspector_key(app, key);
        return;
    }

    if (panel_workspace_symbols_is_open()) {
        panel_workspace_symbols_key(app, key);
        return;
    }

    if (panel_workspace_diagnostics_is_open()) {
        panel_workspace_diagnostics_key(app, key);
        return;
    }

    if (panel_completion_is_open()) {
        panel_completion_key(app, key, mods);
        return;
    }

    /* Route keys to buffer picker when open */
    if (panel_buffer_picker_is_open()) {
        panel_buffer_picker_key(app, key);
        return;
    }

    /* Route keys to jumplist picker when open */
    if (panel_jumplist_picker_is_open()) {
        panel_jumplist_picker_key(app, key);
        return;
    }

    if (panel_lsp_hover_is_open()) {
        if (panel_lsp_hover_key(app, key, mods))
            return;
    }

    /* Route keys to LSP goto picker when open */
    if (panel_lsp_goto_is_open()) {
        panel_lsp_goto_key(app, key);
        return;
    }
    
    /* Route keys to LSP diagnostics picker when open */
    if (panel_lsp_diagnostics_is_open()) {
        panel_lsp_diagnostics_key(app, key);
        return;
    }
    
    /* Route keys to symbols picker when open */
    if (panel_symbols_picker_is_open()) {
        panel_symbols_picker_key(app, key);
        return;
    }
    
    /* Route keys to rename panel when open */
    if (panel_rename_is_open()) {
        panel_rename_key(app, key);
        return;
    }
    
    /* Route keys to code actions panel when open */
    if (panel_code_actions_is_open()) {
        panel_code_actions_key(app, key);
        return;
    }
    
    /* Route keys to space menu when open */
    if (panel_space_menu_is_open()) {
        panel_space_menu_key(app, key);
        return;
    }
    
    /* Route keys to palette when open */
    if (panel_palette_is_open()) {
        panel_palette_key(app, key);
        return;
    }

    switch (mode_get(mode)) {
    case MODE_NORMAL:          handle_normal_key(app, key, action, mods); break;
    case MODE_INSERT:          handle_insert_key(app, key, action, mods); break;
    case MODE_SELECT:          handle_select_key(app, key, action, mods); break;
    case MODE_COMMAND:         handle_command_key(app, key, action, mods); break;
    default: break;
    }
}

void input_handle_char(App *app, unsigned int c) {
    ModeState *mode = (ModeState *)app_get_mode(app);
    
    if (panel_completion_is_open()) {
        panel_completion_close(app);
    }
    if (panel_find_is_open()) {
        Document *doc = (Document *)app_get_doc(app);
        panel_find_input(app, doc, c);
        return;
    }
    if (panel_file_browser_is_open()) {
        panel_file_browser_input(app, c);
        return;
    }
    if (panel_space_menu_is_open()) {
        panel_space_menu_input(app, c);
        return;
    }
    if (panel_palette_is_open()) {
        panel_palette_input(app, c);
        return;
    }
    
    /* Suppress character if mode was just changed (prevents 'i' from inserting 'i') */
    if (mode->suppress_next_char) {
        mode->suppress_next_char = false;
        return;
    }
    
    if (mode_is(mode, MODE_INSERT)) {
        if (c >= 32 && c < 127) {
            Document *doc = (Document *)app_get_doc(app);
            if (doc->cursor_count > 1)
                document_insert_char_multi(doc, (char)c);
            else
                document_insert_char(doc, (char)c);
            if (mode->last_insert_len < 4095) {
                mode->last_insert_text[mode->last_insert_len++] = (char)c;
                mode->last_insert_text[mode->last_insert_len] = '\0';
                mode->last_insert_cursor_delta++;
            }
        }
    } else if (mode_is(mode, MODE_COMMAND)) {
        if (c >= 32 && c < 127 && cmd_len < CMD_BUF_MAX - 1 && c != ':') {
            cmd_buf[cmd_len++] = (char)c;
            cmd_buf[cmd_len] = '\0';
        }
    }
}

static void handle_normal_key(App *app, int key, int action, int mods) {
    (void)action;
    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);

    if ((mods & GLFW_MOD_ALT) && (key == GLFW_KEY_O || key == GLFW_KEY_UP)) {
        document_select_treesitter_parent(doc, app_get_treesitter_manager(app));
        return;
    }
    if ((mods & GLFW_MOD_ALT) && (mods & GLFW_MOD_SHIFT) &&
        (key == GLFW_KEY_I || key == GLFW_KEY_DOWN)) {
        document_select_treesitter_all_children(doc, app_get_treesitter_manager(app));
        return;
    }
    if ((mods & GLFW_MOD_ALT) && (key == GLFW_KEY_I || key == GLFW_KEY_DOWN)) {
        document_select_treesitter_child(doc, app_get_treesitter_manager(app));
        return;
    }
    if ((mods & GLFW_MOD_ALT) && (key == GLFW_KEY_P || key == GLFW_KEY_LEFT)) {
        document_select_treesitter_sibling(doc, app_get_treesitter_manager(app), -1);
        return;
    }
    if ((mods & GLFW_MOD_ALT) && (key == GLFW_KEY_N || key == GLFW_KEY_RIGHT)) {
        document_select_treesitter_sibling(doc, app_get_treesitter_manager(app), 1);
        return;
    }
    if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_A) {
        document_select_treesitter_all_siblings(doc, app_get_treesitter_manager(app));
        return;
    }
    if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_E) {
        document_move_to_treesitter_parent_edge(doc, app_get_treesitter_manager(app), true);
        return;
    }
    if ((mods & GLFW_MOD_ALT) && key == GLFW_KEY_B) {
        document_move_to_treesitter_parent_edge(doc, app_get_treesitter_manager(app), false);
        return;
    }

    /* Count accumulation for digit prefix */
    if (!(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT) && !(mods & GLFW_MOD_SHIFT)) {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
            mode->count = mode->count * 10 + (key - GLFW_KEY_0);
            return;
        }
        if (key == GLFW_KEY_0 && mode->count > 0) {
            mode->count = mode->count * 10;
            return;
        }
    }

    /* . (period) - Repeat last insert */
    if (key == GLFW_KEY_PERIOD && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        if (mode->has_last_insert && mode->last_insert_len > 0) {
            Cursor *cur = &doc->cursors[0];
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
            buffer_insert(&doc->buffer, pos, mode->last_insert_text, mode->last_insert_len);
            history_push_insert(&doc->history, pos, mode->last_insert_text, mode->last_insert_len,
                                cur->row, cur->col);
            cur->col += mode->last_insert_cursor_delta;
            document_mark_dirty(doc);
        }
        mode->count = 0;
        return;
    }

    /* Get repeat count (default 1) */
    int count = mode->count > 0 ? mode->count : 1;
    mode->count = 0;

    /* Pending key handling */
    if (mode->pending_key) {
        char pk = mode->pending_key;
        mode->pending_key = 0;

        if (pk == 'r') {
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
                c = (char)((mods & GLFW_MOD_SHIFT) ? 'A' : 'a') + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
                c = (char)('0' + (key - GLFW_KEY_0));
            else if (key == GLFW_KEY_SPACE) c = ' ';
            if (c) document_replace_selection_char(doc, c);
            return;
        }
        if (pk == 'f') {
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_SPACE) c = ' ';
            if (c) {
                document_find_char_forward(doc, c);
                mode->last_motion_type = 'f';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'F') {
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_SPACE) c = ' ';
            if (c) {
                document_find_char_backward(doc, c);
                mode->last_motion_type = 'F';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 't') {
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_SPACE) c = ' ';
            if (c) {
                document_till_char_forward(doc, c);
                mode->last_motion_type = 't';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'T') {
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_SPACE) c = ' ';
            if (c) {
                document_till_char_backward(doc, c);
                mode->last_motion_type = 'T';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'z') {
            bool handled = false;
            if (key == GLFW_KEY_Z) { document_scroll_center(doc); handled = true; }
            else if (key == GLFW_KEY_C) { document_scroll_center(doc); handled = true; }
            else if (key == GLFW_KEY_M) { 
                /* zm - horizontally center/middle align cursor */
                document_scroll_horizontal_center(doc); 
                handled = true; 
            }
            else if (key == GLFW_KEY_T) { document_scroll_top(doc, doc->viewport_lines); handled = true; }
            else if (key == GLFW_KEY_B && !(mods & GLFW_MOD_CONTROL)) { document_scroll_bottom(doc, doc->viewport_lines); handled = true; }
            else if (key == GLFW_KEY_J) { document_scroll_down(doc); handled = true; }
            else if (key == GLFW_KEY_K) { document_scroll_up(doc); handled = true; }
            else if (key == GLFW_KEY_F) { document_view_page_down(doc); handled = true; }
            else if (key == GLFW_KEY_B && (mods & GLFW_MOD_CONTROL)) { document_view_page_up(doc); handled = true; }
            else if (key == GLFW_KEY_D) { document_view_half_page_down(doc); handled = true; }
            else if (key == GLFW_KEY_U) { document_view_half_page_up(doc); handled = true; }
            
            /* If not sticky, clear pending_key after command */
            if (handled && !mode->view_mode_sticky) {
                mode->pending_key = 0;
            }
            return;
        }
        if (pk == 'm') {
            if (key == GLFW_KEY_S) {
                mode->pending_key = 'm';
                mode->pending_keys[0] = 's';
                mode->pending_len = 1;
                return;
            }
            if (key == GLFW_KEY_D) {
                mode->pending_key = 'm';
                mode->pending_keys[0] = 'd';
                mode->pending_len = 1;
                return;
            }
            if (key == GLFW_KEY_R) {
                mode->pending_key = 'm';
                mode->pending_keys[0] = 'r';
                mode->pending_len = 1;
                return;
            }
            if (key == GLFW_KEY_M) {
                /* mm = go to matching bracket */
                document_match_bracket(doc);
                return;
            }
            return;
        }
        if (pk == 's' && mode->pending_len == 1 && mode->pending_keys[0] == 'm') {
            /* ms<char> = surround with char */
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_SPACE) c = ' ';
            else if (key == GLFW_KEY_9) c = '(';
            else if (key == GLFW_KEY_0) c = ')';
            else if (key == GLFW_KEY_MINUS) c = '_';
            else if (key == GLFW_KEY_COMMA) c = '<';
            else if (key == GLFW_KEY_PERIOD) c = '>';
            else if (key == GLFW_KEY_LEFT_BRACKET) c = '[';
            else if (key == GLFW_KEY_RIGHT_BRACKET) c = ']';
            else if (key == GLFW_KEY_MINUS) c = '_';
            if (c) document_surround(doc, c);
            mode->pending_len = 0;
            return;
        }
        if (pk == 'd' && mode->pending_len == 1 && mode->pending_keys[0] == 'm') {
            /* md<char> = delete surrounding char */
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_9) c = '(';
            else if (key == GLFW_KEY_0) c = ')';
            else if (key == GLFW_KEY_LEFT_BRACKET) c = '[';
            else if (key == GLFW_KEY_RIGHT_BRACKET) c = ']';
            if (c) document_delete_surround(doc, c);
            mode->pending_len = 0;
            return;
        }
        if (pk == 'r' && mode->pending_len == 1 && mode->pending_keys[0] == 'm') {
            /* mr<from><to> = replace surrounding delimiter */
            char from = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) from = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) from = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_9) from = '(';
            else if (key == GLFW_KEY_0) from = ')';
            else if (key == GLFW_KEY_LEFT_BRACKET) from = '[';
            else if (key == GLFW_KEY_RIGHT_BRACKET) from = ']';
            else if (key == GLFW_KEY_MINUS) from = '_';
            if (from) {
                mode->pending_keys[0] = 'r';
                mode->pending_keys[1] = from;
                mode->pending_len = 2;
                return;
            }
            mode->pending_len = 0;
            return;
        }
        if (pk == 'r' && mode->pending_len == 2 && mode->pending_keys[0] == 'r') {
            /* mr<from><to> second char */
            char from = mode->pending_keys[1];
            char to = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) to = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) to = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_9) to = '(';
            else if (key == GLFW_KEY_0) to = ')';
            else if (key == GLFW_KEY_LEFT_BRACKET) to = '[';
            else if (key == GLFW_KEY_RIGHT_BRACKET) to = ']';
            else if (key == GLFW_KEY_MINUS) to = '_';
            if (to) document_replace_surround(doc, from, to);
            mode->pending_len = 0;
            return;
        }
        if (pk == 'i') {
            /* Text object: i<a/w/(/)/[/]/{/}/</>/< /"/'/`/p> */
            char obj = mode->pending_text_obj;  /* 'i' or 'a' */
            bool inner = (obj == 'i');
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key == GLFW_KEY_9) c = '(';
            else if (key == GLFW_KEY_0) c = ')';
            else if (key == GLFW_KEY_LEFT_BRACKET) c = (mods & GLFW_MOD_SHIFT) ? '{' : '[';
            else if (key == GLFW_KEY_RIGHT_BRACKET) c = (mods & GLFW_MOD_SHIFT) ? '}' : ']';
            else if (key == GLFW_KEY_COMMA) c = '<';
            else if (key == GLFW_KEY_PERIOD) c = '>';
            else if (key == GLFW_KEY_MINUS) c = '_';
            else if (key == GLFW_KEY_BACKSLASH) c = '\\';
            else if (key == GLFW_KEY_P) c = 'p';  /* paragraph */

            if (c) {
                /* Ensure cursor has a selection anchor */
                Cursor *cur = &doc->cursors[0];
                if (!cur->has_selection) cursor_select_start(cur);

                switch (c) {
                case 'w': inner ? document_select_inside_word(doc) : document_select_around_word(doc); break;
                case '(': case ')': inner ? document_select_inside_paren(doc) : document_select_around_paren(doc); break;
                case '[': case ']': inner ? document_select_inside_bracket(doc) : document_select_around_bracket(doc); break;
                case '{': case '}': inner ? document_select_inside_curly(doc) : document_select_around_curly(doc); break;
                case '<': inner ? document_select_inside_angle(doc) : document_select_around_angle(doc); break;
                case '"': inner ? document_select_inside_quote(doc) : document_select_around_quote(doc); break;
                case '`': inner ? document_select_inside_backtick(doc) : document_select_around_backtick(doc); break;
                case 'p': inner ? document_select_inside_paragraph(doc) : document_select_around_paragraph(doc); break;
                default: break;
                }

                /* Apply pending operator if any */
                if (mode->pending_operator) {
                    char op = mode->pending_operator;
                    mode->pending_operator = 0;
                    if (op == 'd') {
                        document_delete_selection(doc);
                    } else if (op == 'c') {
                        document_delete_selection(doc);
                        mode_set(mode, MODE_INSERT);
                    } else if (op == 'y') {
                        document_yank(doc);
                        document_collapse_selection(doc);
                    }
                } else {
                    /* No operator: enter select mode with the selection */
                    mode_set(mode, MODE_SELECT);
                }
            } else {
                mode->pending_operator = 0;
            }
            return;
        }
        if (pk == '[') {
            /* [ bracket mode - navigate to previous */
            if (key == GLFW_KEY_D) {
                /* [d - previous diagnostic */
                extern void document_goto_prev_diagnostic(Document *);
                document_goto_prev_diagnostic(doc);
                return;
            }
            if (key == GLFW_KEY_D && (mods & GLFW_MOD_SHIFT)) {
                /* [D - first diagnostic */
                extern void document_goto_first_diagnostic(Document *);
                document_goto_first_diagnostic(doc);
                return;
            }
            return;
        }
        if (pk == 'q') {
            /* q<letter> - start recording macro to register */
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                int slot = key - GLFW_KEY_A;
                macro_start_record(&doc->macros, slot);
            }
            return;
        }
        if (pk == '@') {
            /* @<letter> - replay macro */
            if (mode->pending_len == 1 && mode->pending_keys[0] == '@') {
                /* @@ - replay last macro: find last recorded */
                macro_replay(&doc->macros, -1);
                mode->pending_len = 0;
                return;
            }
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
                int slot = key - GLFW_KEY_A;
                macro_replay(&doc->macros, slot);
            }
            return;
        }
        if (pk == ']') {
            /* ] bracket mode - navigate to next */
            if (key == GLFW_KEY_D) {
                /* ]d - next diagnostic */
                extern void document_goto_next_diagnostic(Document *);
                document_goto_next_diagnostic(doc);
                return;
            }
            if (key == GLFW_KEY_D && (mods & GLFW_MOD_SHIFT)) {
                /* ]D - last diagnostic */
                extern void document_goto_last_diagnostic(Document *);
                document_goto_last_diagnostic(doc);
                return;
            }
            return;
        }
        if (pk == ' ') {
            /* Standalone modifier presses keep the menu open */
            if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT ||
                key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL ||
                key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT ||
                key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER) {
                mode->pending_key = ' ';
                return;
            }

            /* Space mode sub-commands - close menu after command */
            panel_space_menu_close(app);
            
            if (key == GLFW_KEY_F) {
                /* Space F - file browser at current directory */
                if (mods & GLFW_MOD_SHIFT) {
                    panel_file_browser_open(app);
                } else {
                    /* Space f - file browser at workspace root */
                    const char *workspace = app_get_workspace_root(app);
                    extern void panel_file_browser_open_at(App *, const char *);
                    panel_file_browser_open_at(app, workspace);
                }
                mode->pending_len = 0;
                return;
            }
            if (key == GLFW_KEY_B) {
                /* Space b - buffer picker */
                panel_buffer_picker_open(app);
                mode->pending_len = 0;
                return;
            }
            if (key == GLFW_KEY_J) {
                /* Space j - jumplist picker */
                panel_jumplist_picker_open(app);
                mode->pending_len = 0;
                return;
            }
             if (key == GLFW_KEY_C) {
                 document_comment_toggle(doc);
                 mode->pending_len = 0;
                 return;
             }
             if (key == GLFW_KEY_K) {
                 /* Space k - hover documentation */
                 extern void document_lsp_hover(Document *, void *);
                 document_lsp_hover(doc, app_get_lsp_manager(app));
                 panel_lsp_hover_open(app);
                 mode->pending_len = 0;
                 return;
             }
              if (key == GLFW_KEY_D) {
                  if (mods & GLFW_MOD_SHIFT) {
                      panel_workspace_diagnostics_open(app);
                      mode->pending_len = 0;
                      return;
                  }
                  /* Space d / D - diagnostics picker */
                  panel_lsp_diagnostics_open(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_S) {
                  if (mods & GLFW_MOD_SHIFT) {
                      panel_workspace_symbols_open(app);
                      mode->pending_len = 0;
                      return;
                  }
                  /* Space s / S - symbols picker */
                  panel_symbols_picker_open(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_H) {
                  /* Space h - select references under cursor */
                  document_lsp_select_references(doc, app_get_lsp_manager(app));
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_T) {
                  /* Space t - tree-sitter node inspector */
                  panel_treesitter_inspector_open(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_A) {
                  /* Space a - code actions */
                  panel_code_actions_open(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_Y) {
                  /* Space y - yank selection to system clipboard */
                  if (mods & GLFW_MOD_SHIFT) {
                      /* Space Y - yank main selection to system clipboard */
                      document_yank_main_to_system_clipboard(doc);
                  } else {
                      document_yank_to_system_clipboard(doc);
                  }
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_P) {
                  if (mods & GLFW_MOD_SHIFT) {
                      /* Space P - paste before from system clipboard */
                      document_paste_before_from_system_clipboard(doc);
                  } else {
                      /* Space p - paste from system clipboard after cursor */
                      document_paste_from_system_clipboard(doc);
                  }
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_O) {
                  /* Space o - file picker at $HOME */
                  extern void panel_file_browser_open_at_home(App *);
                  panel_file_browser_open_at_home(app);
                  mode->pending_len = 0;
                  return;
              }
              if (key == GLFW_KEY_R) {
                  if (mods & GLFW_MOD_SHIFT) {
                      /* Space R - replace selection with system clipboard */
                      document_replace_selection_from_system_clipboard(doc);
                  } else {
                      /* Space r - rename symbol */
                      panel_rename_open(app);
                  }
                  mode->pending_len = 0;
                  return;
              }
              mode->pending_len = 0;
              return;
        }
        return;
    }
    if (mode->g_pending) {
        mode->g_pending = false;
        if (key == GLFW_KEY_G)
            document_cursor_doc_start(doc);
        else if (key == GLFW_KEY_E)
            document_cursor_doc_end(doc);
        else if (key == GLFW_KEY_S)
            document_cursor_first_non_blank(doc);
        else if (key == GLFW_KEY_H)
            document_goto_line_start(doc);
        else if (key == GLFW_KEY_L)
            document_goto_line_end(doc);
        else if (key == GLFW_KEY_T)
            document_goto_view_top(doc);
        else if (key == GLFW_KEY_C)
            document_goto_view_center(doc);
        else if (key == GLFW_KEY_B)
            document_goto_view_bottom(doc);
        else if (key == GLFW_KEY_F)
            document_go_to_file(doc);
        else if (key == GLFW_KEY_J)
            document_move_cursor(doc, 1, 0);
        else if (key == GLFW_KEY_K)
            document_move_cursor(doc, -1, 0);
        else if (key == GLFW_KEY_N)
            app_next_buffer(app); /* gn - go to next buffer */
        else if (key == GLFW_KEY_P)
            app_prev_buffer(app); /* gp - go to previous buffer */
        else if (key == GLFW_KEY_D) {
            /* gd - go to definition (LSP) */
            extern void document_lsp_goto_definition(Document *, void *);
            document_lsp_goto_definition(doc, app_get_lsp_manager(app));
            panel_lsp_goto_open(app);
        }
        else if (key == GLFW_KEY_Y) {
            /* gy - go to type definition (LSP) */
            extern void document_lsp_goto_type_definition(Document *, void *);
            document_lsp_goto_type_definition(doc, app_get_lsp_manager(app));
            panel_lsp_goto_open(app);
        }
        else if (key == GLFW_KEY_R) {
            /* gr - go to references (LSP) */
            extern void document_lsp_goto_references(Document *, void *);
            document_lsp_goto_references(doc, app_get_lsp_manager(app));
            panel_lsp_goto_open(app);
        }
        else if (key == GLFW_KEY_I) {
            /* gi - go to implementation (LSP) */
            extern void document_lsp_goto_implementation(Document *, void *);
            document_lsp_goto_implementation(doc, app_get_lsp_manager(app));
            panel_lsp_goto_open(app);
        }
        else if (key == GLFW_KEY_PERIOD)
            document_goto_last_modification(doc); /* g. - go to last modification */
        else if (key == GLFW_KEY_BACKSLASH) {
            /* g| - go to column number (count required) */
            if (mode->count > 0) {
                Cursor *cur = &doc->cursors[0];
                int target = mode->count - 1;
                int len = (int)buffer_line_len(&doc->buffer, cur->row);
                cur->col = target < len ? target : len;
            }
            mode->count = 0;
        }
        return;
    }

    /* Alt combinations */
    if (mods & GLFW_MOD_ALT) {
        /* Alt-Shift-C - Copy selection above */
        if (key == GLFW_KEY_C && (mods & GLFW_MOD_SHIFT)) {
            document_copy_selection_above(doc);
            return;
        }
        switch (key) {
        case GLFW_KEY_SEMICOLON:
            document_flip_cursor_anchor(doc);
            return;
        case GLFW_KEY_D:
            document_delete_selection(doc);
            return;
        case GLFW_KEY_C:
            document_delete_selection(doc);
            mode_set(mode, MODE_INSERT);
            return;
        case GLFW_KEY_S:
            document_split_selection_newlines(doc);
            return;
        case GLFW_KEY_MINUS:
            document_merge_selections(doc);
            return;
        case GLFW_KEY_J:
            document_join_lines_with_space(doc);
            return;
        case GLFW_KEY_GRAVE_ACCENT:
            document_uppercase(doc);
            return;
        case GLFW_KEY_PERIOD:
            /* Alt-. - Repeat last motion */
            if (mode->last_motion_type) {
                char c = mode->last_motion_char;
                switch (mode->last_motion_type) {
                case 'f': document_find_char_forward(doc, c); break;
                case 'F': document_find_char_backward(doc, c); break;
                case 't': document_till_char_forward(doc, c); break;
                case 'T': document_till_char_backward(doc, c); break;
                }
            }
            return;
        case GLFW_KEY_9:
            /* Alt-( - Rotate selection contents backward */
            if (mods & GLFW_MOD_SHIFT) {
                document_rotate_selection_contents_backward(doc);
            }
            return;
        case GLFW_KEY_0:
            /* Alt-) - Rotate selection contents forward */
            if (mods & GLFW_MOD_SHIFT) {
                document_rotate_selection_contents_forward(doc);
            }
            return;
        case GLFW_KEY_U:
            /* Alt-U - Move forward in history tree (redo) */
            if (mods & GLFW_MOD_SHIFT) {
                document_redo(doc);
            } else {
                /* Alt-u - Move backward in history tree (undo) */
                document_undo(doc);
            }
            return;
        default: break;
        }
    }

    /* Ctrl combinations */
    if (mods & GLFW_MOD_CONTROL) {
        switch (key) {
        case GLFW_KEY_D:
            document_half_page_down(doc, doc->viewport_lines);
            return;
        case GLFW_KEY_U:
            document_half_page_up(doc, doc->viewport_lines);
            return;
        case GLFW_KEY_C:
            document_comment_toggle(doc);
            return;
        case GLFW_KEY_A:
            for (int i = 0; i < count; i++) document_increment_number(doc);
            return;
        case GLFW_KEY_X:
            for (int i = 0; i < count; i++) document_decrement_number(doc);
            return;
        case GLFW_KEY_O:
            document_jumplist_backward(doc);
            return;
        case GLFW_KEY_I:
            document_jumplist_forward(doc);
            return;
        case GLFW_KEY_S: {
            Cursor *cur = &doc->cursors[0];
            document_jumplist_push(doc, cur->row, cur->col);
            return;
        }
        case GLFW_KEY_6:
            /* Ctrl-^ - go to alternate file */
            if (mods & GLFW_MOD_SHIFT) {
                document_goto_alternate(doc);
            }
            return;
        default: break;
        }
    }

    /* W/B/E (Shift) - WORD motions */
    if (key == GLFW_KEY_W && (mods & GLFW_MOD_SHIFT)) {
        for (int i = 0; i < count; i++) document_cursor_WORD_forward(doc);
        return;
    }
    if (key == GLFW_KEY_B && (mods & GLFW_MOD_SHIFT)) {
        for (int i = 0; i < count; i++) document_cursor_WORD_backward(doc);
        return;
    }
    if (key == GLFW_KEY_E && (mods & GLFW_MOD_SHIFT)) {
        for (int i = 0; i < count; i++) document_cursor_WORD_end(doc);
        return;
    }

    /* Alt-. - Repeat last motion (stub) */
    if (key == GLFW_KEY_PERIOD && (mods & GLFW_MOD_ALT)) {
        return;
    }

    /* : (Shift+semicolon) - Command mode */
    if (key == GLFW_KEY_SEMICOLON && (mods & GLFW_MOD_SHIFT)) {
        mode_set(mode, MODE_COMMAND);
        input_cmd_reset();
        return;
    }

    /* Alt-; - Flip cursor and anchor */
    if (key == GLFW_KEY_SEMICOLON && (mods & GLFW_MOD_ALT)) {
        document_flip_cursor_anchor(doc);
        return;
    }

    /* ? (Shift+/) - Search backward */
    if (key == GLFW_KEY_SLASH && (mods & GLFW_MOD_SHIFT)) {
        Document *doc2 = (Document *)app_get_doc(app);
        panel_find_open(app, doc2);
        return;
    }

    /* * (Shift+8) - Use selection/word as search pattern */
    if (key == GLFW_KEY_8 && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_ALT)) {
        document_search_word(doc, true);
        return;
    }

    /* Alt-* - Search without word boundaries */
    if (key == GLFW_KEY_8 && (mods & GLFW_MOD_SHIFT) && (mods & GLFW_MOD_ALT)) {
        document_search_word(doc, false);
        return;
    }

    /* Alt-: - Force selection direction forward */
    if (key == GLFW_KEY_SEMICOLON && (mods & GLFW_MOD_ALT)) {
        document_force_selection_forward(doc);
        return;
    }

    /* ( ) - Rotate selections */
    if (key == GLFW_KEY_9 && !(mods & GLFW_MOD_SHIFT)) {
        document_rotate_selections_backward(doc);
        return;
    }
    if (key == GLFW_KEY_0 && (mods & GLFW_MOD_SHIFT)) {
        document_rotate_selections_forward(doc);
        return;
    }

    /* G - Go to line number or end of file */
    if (key == GLFW_KEY_G && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        mode->g_pending = true;
        return;
    }

    /* Shift+G - Go to end of file */
    if (key == GLFW_KEY_G && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        document_cursor_doc_end(doc);
        return;
    }

    /* [ - Bracket mode (navigate to previous) */
    if (key == GLFW_KEY_LEFT_BRACKET && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode->pending_key = '[';
        return;
    }

    /* ] - Bracket mode (navigate to next) */
    if (key == GLFW_KEY_RIGHT_BRACKET && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode->pending_key = ']';
        return;
    }

    /* P - Paste before */
    if (key == GLFW_KEY_P && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        document_paste_before(doc);
        return;
    }

    /* o / O - Open line below/above and enter insert */
    if (key == GLFW_KEY_O && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        document_open_line_below(doc);
        mode_set(mode, MODE_INSERT);
        return;
    }
    if (key == GLFW_KEY_O && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL)) {
        document_open_line_above(doc);
        mode_set(mode, MODE_INSERT);
        return;
    }

    /* I - Insert at line start */
    if (key == GLFW_KEY_I && (mods & GLFW_MOD_SHIFT)) {
        document_cursor_home(doc);
        mode_set(mode, MODE_INSERT);
        return;
    }

    /* A - Insert at line end */
    if (key == GLFW_KEY_A && (mods & GLFW_MOD_SHIFT)) {
        Cursor *cur = &doc->cursors[0];
        cur->col = (int)buffer_line_len(&doc->buffer, cur->row);
        mode_set(mode, MODE_INSERT);
        return;
    }

    /* Alt-d - Delete selection without yanking */
    if (key == GLFW_KEY_D && (mods & GLFW_MOD_ALT)) {
        document_delete_selection(doc);
        return;
    }

    /* Alt-c - Change selection without yanking */
    if (key == GLFW_KEY_C && (mods & GLFW_MOD_ALT)) {
        document_delete_selection(doc);
        mode_set(mode, MODE_INSERT);
        return;
    }

    /* > (Shift+.) - Indent selection */
    if (key == GLFW_KEY_PERIOD && (mods & GLFW_MOD_SHIFT)) {
        document_indent_selection(doc);
        return;
    }

    /* < (Shift+,) - Dedent selection */
    if (key == GLFW_KEY_COMMA && (mods & GLFW_MOD_SHIFT)) {
        document_dedent_selection(doc);
        return;
    }

    /* s - Select all matches of selection text within selection */
    if (key == GLFW_KEY_S && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_SELECT);
        }
        return;
    }

    /* S - Split selections on matches of selection text */
    if (key == GLFW_KEY_S && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_SPLIT);
        }
        return;
    }

    /* K - Keep selections matching */
    if (key == GLFW_KEY_K && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_KEEP);
        return;
    }

    /* Alt-K - Remove selections matching */
    if (key == GLFW_KEY_K && (mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_REMOVE);
        return;
    }

    /* Shell commands */
    /* | - Pipe selection through shell command */
    if (key == GLFW_KEY_BACKSLASH && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_PIPE);
        }
        return;
    }

    /* Alt-| - Pipe selection to command (ignore output) */
    if (key == GLFW_KEY_BACKSLASH && (mods & GLFW_MOD_SHIFT) && (mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_PIPE_TO);
        }
        return;
    }

    /* ! - Insert command output at cursor */
    if (key == GLFW_KEY_1 && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_INSERT_OUTPUT);
        return;
    }

    /* Alt-! - Append command output to end of line */
    if (key == GLFW_KEY_1 && (mods & GLFW_MOD_SHIFT) && (mods & GLFW_MOD_ALT)) {
        panel_find_open_ex(app, doc, FR_ACTION_APPEND_OUTPUT);
        return;
    }

    /* $ - Filter selection through shell command (without alt) or end of line (without selection) */
    if (key == GLFW_KEY_4 && (mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->has_selection) {
            panel_find_open_ex(app, doc, FR_ACTION_FILTER);
            return;
        }
        /* Fall through to normal $ behavior below */
    }

    switch (key) {
    /* Mode switching */
    case GLFW_KEY_I: mode_set(mode, MODE_INSERT); break;
    case GLFW_KEY_A: {
        Cursor *cur = &doc->cursors[0];
        cur->col = (int)buffer_line_len(&doc->buffer, cur->row);
        mode_set(mode, MODE_INSERT);
        break;
    }
    case GLFW_KEY_V: mode_set(mode, MODE_SELECT);
                     cursor_select_start(&doc->cursors[0]); break;
    case GLFW_KEY_SPACE:
        mode->pending_key = ' ';
        panel_space_menu_open(app);
        break;
    case GLFW_KEY_SLASH: {
        Document *doc2 = (Document *)app_get_doc(app);
        panel_find_open(app, doc2);
        break;
    }
    /* G - handled before switch */

    /* Navigation (with count) */
    case GLFW_KEY_H: case GLFW_KEY_LEFT:  for (int i = 0; i < count; i++) document_move_cursor(doc, 0, -1); break;
    case GLFW_KEY_DOWN:  for (int i = 0; i < count; i++) document_move_cursor(doc, 1, 0); break;
    case GLFW_KEY_K: case GLFW_KEY_UP:    for (int i = 0; i < count; i++) document_move_cursor(doc, -1, 0); break;
    case GLFW_KEY_L: case GLFW_KEY_RIGHT: for (int i = 0; i < count; i++) document_move_cursor(doc, 0, 1); break;
    case GLFW_KEY_HOME:  document_cursor_home(doc); break;
    case GLFW_KEY_END:   document_cursor_end(doc); break;
    case GLFW_KEY_PAGE_UP:   document_cursor_page_up(doc); break;
    case GLFW_KEY_PAGE_DOWN: document_cursor_page_down(doc); break;

    /* Word motions (with count) */
    case GLFW_KEY_W: for (int i = 0; i < count; i++) document_cursor_word_forward(doc); break;
    case GLFW_KEY_B: for (int i = 0; i < count; i++) document_cursor_word_backward(doc); break;
    case GLFW_KEY_E: for (int i = 0; i < count; i++) document_cursor_word_end(doc); break;

    /* WORD motions (Shift+W/B/E) */
    /* Handled before switch with shift check */

    /* Line start/end */
    case GLFW_KEY_0: document_cursor_home(doc); break;
    case GLFW_KEY_4: if (mods & GLFW_MOD_SHIFT) document_cursor_end(doc); break;

    /* n/N - Search next/prev */
    case GLFW_KEY_N:
        if (mods & GLFW_MOD_SHIFT)
            document_search_prev(doc);
        else
            document_search_next(doc);
        break;

    /* x/X - Select line / extend / shrink to line bounds */
    case GLFW_KEY_X:
        if (mods & GLFW_MOD_SHIFT)
            document_shrink_to_line_bounds(doc);
        else {
            Cursor *cur = &doc->cursors[0];
            if (!cur->has_selection) {
                document_select_line(doc);
            } else {
                int sr, sc, er, ec;
                cursor_normalize(cur, &sr, &sc, &er, &ec);
                int next_line = er + 1;
                size_t total_lines = buffer_line_count(&doc->buffer);
                if (next_line < (int)total_lines) {
                    cur->anchor_col = 0;
                    cur->has_selection = true;
                    cursor_move_to(cur, next_line, 0);
                    int next_len = (int)buffer_line_len(&doc->buffer, next_line);
                    cur->col = next_len;
                }
            }
        }
        break;

    /* J - Join lines inside selection */
    case GLFW_KEY_J:
        document_join_lines_selection(doc);
        break;

    /* % - Select entire file */
    case GLFW_KEY_5:
        document_select_all(doc);
        break;

    /* p - Paste after */
    case GLFW_KEY_P: document_paste(doc); break;

    /* u/U - Undo/Redo */
    case GLFW_KEY_U:
        if (mods & GLFW_MOD_SHIFT)
            document_redo(doc);
        else
            document_undo(doc);
        break;

    default: break;
    }

    /* Keys that conflict with switch cases handled here */
    switch (key) {
    case GLFW_KEY_D:
        if (doc->cursors[0].has_selection) {
            document_delete_selection(doc);
        } else {
            mode->pending_operator = 'd';
        }
        break;
    case GLFW_KEY_C:
        if (doc->cursors[0].has_selection) {
            document_delete_selection(doc);
            mode_set(mode, MODE_INSERT);
        } else {
            mode->pending_operator = 'c';
        }
        break;
    case GLFW_KEY_Y:
        if (doc->cursors[0].has_selection) {
            document_yank(doc);
        } else {
            mode->pending_operator = 'y';
        }
        break;
    case GLFW_KEY_R:
        if (mods & GLFW_MOD_SHIFT)
            document_replace_selection_yanked(doc);
        else
            mode->pending_key = 'r';
        break;
    case GLFW_KEY_SEMICOLON:
        document_collapse_selection(doc);
        break;
    case GLFW_KEY_GRAVE_ACCENT:
        if (mods & GLFW_MOD_SHIFT)
            document_toggle_case(doc);
        else
            document_lowercase(doc);
        break;
    case GLFW_KEY_COMMA:
        document_keep_primary_selection(doc);
        break;
    case GLFW_KEY_F:
        mode->pending_key = (mods & GLFW_MOD_SHIFT) ? 'F' : 'f';
        break;
    case GLFW_KEY_T:
        mode->pending_key = (mods & GLFW_MOD_SHIFT) ? 'T' : 't';
        break;
    case GLFW_KEY_I:
        mode->pending_text_obj = 'i';
        mode->pending_key = 'i';
        break;
    case GLFW_KEY_A:
        mode->pending_text_obj = 'a';
        mode->pending_key = 'i';
        break;
    case GLFW_KEY_Z:
        if (mods & GLFW_MOD_SHIFT) {
            /* Z - sticky view mode */
            mode->pending_key = 'z';
            mode->view_mode_sticky = true;
        } else {
            /* z - normal view mode */
            mode->pending_key = 'z';
            mode->view_mode_sticky = false;
        }
        break;
    case GLFW_KEY_MINUS:
        document_trim_whitespace(doc);
        break;
    case GLFW_KEY_EQUAL:
        document_format_selection(doc);
        break;
    case GLFW_KEY_M:
        mode->pending_key = 'm';
        break;
    case GLFW_KEY_7:
        document_align_selections(doc);
        break;
    case GLFW_KEY_Q:
        /* q - toggle macro recording */
        if (macro_is_recording(&doc->macros)) {
            macro_stop_record(&doc->macros);
        } else {
            mode->pending_key = 'q';
        }
        break;
    case GLFW_KEY_2:
        /* @ - replay macro */
        if (mods & GLFW_MOD_SHIFT) {
            mode->pending_key = '@';
        } else {
            /* @@ - replay last macro (handled via pending) */
            mode->pending_key = '@';
            mode->pending_keys[0] = '@';
            mode->pending_len = 1;
        }
        break;
    default: break;
    }

    /* Ctrl-Shift-C - block comment toggle (handled before normal switch) */
    if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT) && key == GLFW_KEY_C) {
        /* Use language settings for block comment */
        const LanguageSettings *ls = language_settings_get(doc->language_id);
        if (ls && ls->comment_open && ls->comment_open[0]) {
            document_comment_toggle_block(doc, ls->comment_open, ls->comment_close);
        } else {
            document_comment_toggle_block(doc, "/*", "*/");
        }
        return;
    }

    /* Record key if macro is recording */
    if (macro_is_recording(&doc->macros)) {
        macro_record_key(&doc->macros, key);
    }
}

static void handle_insert_key(App *app, int key, int action, int mods) {
    (void)action;
    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);

    /* Ctrl-w / Alt-Backspace - delete previous word */
    if (key == GLFW_KEY_X && (mods & GLFW_MOD_CONTROL)) {
        panel_completion_open(app);
        return;
    }

    /* Ctrl-w / Alt-Backspace - delete previous word */
    if (key == GLFW_KEY_W && (mods & GLFW_MOD_CONTROL)) {
        Cursor *cur = &doc->cursors[0];
        const char *line = buffer_line_ptr(&doc->buffer, cur->row);
        int col = cur->col;
        if (col > 0) {
            int start = col - 1;
            while (start > 0 && (line[start] == ' ' || line[start] == '\t'))
                start--;
            while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t'
                   && line[start - 1] != '\n')
                start--;
            int del = col - start;
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, start);
            buffer_delete(&doc->buffer, pos, del);
            cur->col = start;
            document_mark_dirty(doc);
        }
        return;
    }
    if (key == GLFW_KEY_BACKSPACE && (mods & GLFW_MOD_ALT)) {
        Cursor *cur = &doc->cursors[0];
        const char *line = buffer_line_ptr(&doc->buffer, cur->row);
        int col = cur->col;
        if (col > 0) {
            int start = col - 1;
            while (start > 0 && (line[start] == ' ' || line[start] == '\t'))
                start--;
            while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t'
                   && line[start - 1] != '\n')
                start--;
            int del = col - start;
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, start);
            buffer_delete(&doc->buffer, pos, del);
            cur->col = start;
            document_mark_dirty(doc);
        }
        return;
    }

    /* Ctrl-d / Delete - delete next char */
    if (key == GLFW_KEY_D && (mods & GLFW_MOD_CONTROL)) {
        document_delete_char_at_cursor(doc);
        return;
    }
    if (key == GLFW_KEY_DELETE) {
        document_delete_char_at_cursor(doc);
        return;
    }

    /* Alt-d / Alt-Delete - delete next word */
    if (key == GLFW_KEY_D && (mods & GLFW_MOD_ALT)) {
        document_delete_word_forward(doc);
        return;
    }
    if (key == GLFW_KEY_DELETE && (mods & GLFW_MOD_ALT)) {
        document_delete_word_forward(doc);
        return;
    }

    /* Ctrl-u - delete to line start */
    if (key == GLFW_KEY_U && (mods & GLFW_MOD_CONTROL)) {
        Cursor *cur = &doc->cursors[0];
        if (cur->col > 0) {
            size_t start = buffer_pos_from_row_col(&doc->buffer, cur->row, 0);
            size_t end = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
            buffer_delete(&doc->buffer, start, end - start);
            cur->col = 0;
            document_mark_dirty(doc);
        }
        return;
    }

    /* Ctrl-k - delete to line end */
    if (key == GLFW_KEY_K && (mods & GLFW_MOD_CONTROL)) {
        Cursor *cur = &doc->cursors[0];
        int len = (int)buffer_line_len(&doc->buffer, cur->row);
        if (cur->col < len) {
            size_t pos = buffer_pos_from_row_col(&doc->buffer, cur->row, cur->col);
            size_t line_end = buffer_pos_from_row_col(&doc->buffer, cur->row, len);
            buffer_delete(&doc->buffer, pos, line_end - pos);
            document_mark_dirty(doc);
        }
        return;
    }

    /* Ctrl-z - undo */
    if (key == GLFW_KEY_Z && (mods & GLFW_MOD_CONTROL)) {
        document_undo(doc);
        return;
    }

    /* Ctrl-y - redo */
    if (key == GLFW_KEY_Y && (mods & GLFW_MOD_CONTROL)) {
        document_redo(doc);
        return;
    }

    /* Ctrl-r - insert register content */
    if (key == GLFW_KEY_R && (mods & GLFW_MOD_CONTROL)) {
        if (doc->clipboard && doc->clipboard_len > 0) {
            for (size_t i = 0; i < doc->clipboard_len; i++) {
                if (doc->clipboard[i] == '\n')
                    document_newline(doc);
                else
                    document_insert_char(doc, doc->clipboard[i]);
            }
        }
        return;
    }

    switch (key) {
    case GLFW_KEY_ESCAPE:
        document_clear_cursors(doc);
        mode_set(mode, MODE_NORMAL);
        break;
    case GLFW_KEY_BACKSPACE:
        if (doc->cursor_count > 1)
            document_delete_char_multi(doc);
        else
            document_delete_char(doc);
        if (mode->last_insert_len > 0) {
            mode->last_insert_len--;
            mode->last_insert_cursor_delta--;
        }
        break;
    case GLFW_KEY_ENTER:
        if (doc->cursor_count > 1)
            document_newline_multi(doc);
        else
            document_newline(doc);
        if (mode->last_insert_len < 4095) {
            mode->last_insert_text[mode->last_insert_len++] = '\n';
            mode->last_insert_text[mode->last_insert_len] = '\0';
            mode->last_insert_cursor_delta = 0;
        }
        break;
    case GLFW_KEY_TAB:
        if (doc->cursor_count > 1)
            document_insert_char_multi(doc, '\t');
        else
            document_insert_char(doc, '\t');
        if (mode->last_insert_len < 4095) {
            mode->last_insert_text[mode->last_insert_len++] = '\t';
            mode->last_insert_text[mode->last_insert_len] = '\0';
            mode->last_insert_cursor_delta++;
        }
        break;
    case GLFW_KEY_HOME:
        document_cursor_home(doc);
        break;
    case GLFW_KEY_END:
        document_cursor_end(doc);
        break;
    case GLFW_KEY_UP:
        document_move_cursor(doc, -1, 0);
        break;
    case GLFW_KEY_DOWN:
        document_move_cursor(doc, 1, 0);
        break;
    case GLFW_KEY_LEFT:
        document_move_cursor(doc, 0, -1);
        break;
    case GLFW_KEY_RIGHT:
        document_move_cursor(doc, 0, 1);
        break;
    case GLFW_KEY_PAGE_UP:
        document_cursor_page_up(doc);
        break;
    case GLFW_KEY_PAGE_DOWN:
        document_cursor_page_down(doc);
        break;
    default:
        break;
    }
}

static void handle_command_key(App *app, int key, int action, int mods) {
    (void)action; (void)mods;
    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);

    if (key == GLFW_KEY_BACKSPACE && cmd_len > 0) {
        cmd_len--;
        cmd_buf[cmd_len] = '\0';
        return;
    }

    if (key != GLFW_KEY_ENTER) return;

    const char *write_path = NULL;
    bool write_and_quit = false;
    if (strncmp(cmd_buf, "w ", 2) == 0) {
        write_path = cmd_buf + 2;
    } else if (strncmp(cmd_buf, "write ", 6) == 0) {
        write_path = cmd_buf + 6;
    } else if (strncmp(cmd_buf, "w! ", 3) == 0) {
        write_path = cmd_buf + 3;
    } else if (strncmp(cmd_buf, "write! ", 7) == 0) {
        write_path = cmd_buf + 7;
    } else if (strncmp(cmd_buf, "wq ", 3) == 0) {
        write_path = cmd_buf + 3;
        write_and_quit = true;
    } else if (strncmp(cmd_buf, "write-quit ", 11) == 0) {
        write_path = cmd_buf + 11;
        write_and_quit = true;
    } else if (strncmp(cmd_buf, "x ", 2) == 0) {
        write_path = cmd_buf + 2;
        write_and_quit = true;
    }

    /* Execute command */
    if (write_path && *write_path) {
        document_save_as(doc, write_path);
        if (write_and_quit)
            cmd_quit(app);
    } else if (strcmp(cmd_buf, "w") == 0 || strcmp(cmd_buf, "write") == 0) {
        document_save(doc);
    } else if (strcmp(cmd_buf, "q") == 0 || strcmp(cmd_buf, "quit") == 0) {
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "wq") == 0 || strcmp(cmd_buf, "x") == 0) {
        document_save(doc);
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "q!") == 0) {
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "w!") == 0) {
        document_save(doc);
    } else if (strcmp(cmd_buf, "wq!") == 0 || strcmp(cmd_buf, "x!") == 0) {
        document_save(doc);
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "qa") == 0 || strcmp(cmd_buf, "quit-all") == 0 ||
               strcmp(cmd_buf, "qa!") == 0 || strcmp(cmd_buf, "quit-all!") == 0) {
        cmd_quit(app);
    } else if (strcmp(cmd_buf, "bn") == 0 || strcmp(cmd_buf, "bnext") == 0 ||
               strcmp(cmd_buf, "buffer-next") == 0) {
        app_next_buffer(app);
    } else if (strcmp(cmd_buf, "bp") == 0 || strcmp(cmd_buf, "bprev") == 0 ||
               strcmp(cmd_buf, "buffer-previous") == 0) {
        app_prev_buffer(app);
    } else if (strcmp(cmd_buf, "bc") == 0 || strcmp(cmd_buf, "bclose") == 0 ||
               strcmp(cmd_buf, "buffer-close") == 0 ||
               strcmp(cmd_buf, "bc!") == 0 || strcmp(cmd_buf, "bclose!") == 0 ||
               strcmp(cmd_buf, "buffer-close!") == 0) {
        app_close_buffer(app, app_get_current_buffer_index(app));
    } else if (strcmp(cmd_buf, "new") == 0 || strcmp(cmd_buf, "n") == 0) {
        document_new(doc);
    } else if (strncmp(cmd_buf, "e ", 2) == 0 ||
               strncmp(cmd_buf, "o ", 2) == 0 ||
               strncmp(cmd_buf, "open ", 5) == 0 ||
               strncmp(cmd_buf, "edit ", 5) == 0) {
        const char *path = (cmd_buf[1] == ' ') ? cmd_buf + 2 : cmd_buf + 5;
        app_open_file(app, path);
    } else if (strncmp(cmd_buf, "r ", 2) == 0 || strncmp(cmd_buf, "read ", 5) == 0) {
        const char *path = cmd_buf[1] == ' ' ? cmd_buf + 2 : cmd_buf + 5;
        document_insert_file(doc, path);
    } else if (strncmp(cmd_buf, "mv ", 3) == 0 || strncmp(cmd_buf, "move ", 5) == 0) {
        const char *path = cmd_buf[2] == ' ' ? cmd_buf + 3 : cmd_buf + 5;
        document_move_file(doc, path);
    } else if (strcmp(cmd_buf, "reload") == 0 || strcmp(cmd_buf, "rl") == 0 ||
               strcmp(cmd_buf, "reload-all") == 0 || strcmp(cmd_buf, "rla") == 0) {
        if (doc->filepath)
            document_open(doc, doc->filepath);
    } else if (strcmp(cmd_buf, "sort") == 0) {
        document_sort_selection(doc);
    } else if (strcmp(cmd_buf, "fmt") == 0 || strcmp(cmd_buf, "format") == 0) {
        document_format_selection(doc);
    } else if (strncmp(cmd_buf, "reflow ", 7) == 0) {
        int width = atoi(cmd_buf + 7);
        if (width > 0) document_reflow(doc, width);
    } else if (strcmp(cmd_buf, "retab") == 0 || strcmp(cmd_buf, "retab!") == 0) {
        /* Convert all indentation to tabs using configured tab width */
        const LanguageSettings *ls = language_settings_get(doc->language_id);
        int tw = ls ? ls->tab_width : 4;
        document_indent_style_to_tabs(doc, tw);
    } else if (strcmp(cmd_buf, "expandtab") == 0 || strcmp(cmd_buf, "expandtab!") == 0) {
        const LanguageSettings *ls = language_settings_get(doc->language_id);
        int sw = ls ? ls->tab_width : 4;
        document_indent_style_to_spaces(doc, sw);
    } else if (strcmp(cmd_buf, "lsp-stop") == 0) {
        lsp_manager_stop_all((LSPManager *)app_get_lsp_manager(app));
    } else if (strcmp(cmd_buf, "lsp-restart") == 0) {
        lsp_manager_restart_all((LSPManager *)app_get_lsp_manager(app));
    } else if (strcmp(cmd_buf, "workspace-symbols") == 0) {
        panel_workspace_symbols_open(app);
    } else if (strcmp(cmd_buf, "workspace-diagnostics") == 0) {
        panel_workspace_diagnostics_open(app);
    } else if (strcmp(cmd_buf, "tree-sitter-subtree") == 0 ||
               strcmp(cmd_buf, "ts-subtree") == 0 ||
               strcmp(cmd_buf, "tree-sitter-highlight-name") == 0 ||
               strcmp(cmd_buf, "tree-sitter-scopes") == 0) {
        panel_treesitter_inspector_open(app);
    } else {
        Command *cmd = command_find(cmd_buf);
        if (cmd && cmd->fn) {
            cmd->fn(app);
        } else if (cmd_buf[0] >= '1' && cmd_buf[0] <= '9') {
            int line = atoi(cmd_buf);
            if (line > 0)
                document_cursor_to(doc, line - 1, 0);
        }
    }

    input_cmd_reset();
    mode_set(mode, MODE_NORMAL);
}

static void handle_select_key(App *app, int key, int action, int mods) {
    (void)action;
    Document *doc = (Document *)app_get_doc(app);
    ModeState *mode = (ModeState *)app_get_mode(app);

    if (mods & GLFW_MOD_SHIFT) {
        Cursor *cur = &doc->cursors[0];
        if (!cur->has_selection) cursor_select_start(cur);
    }

    /* Initialize selection only once when entering select mode */
    if (!mode->select_mode_initialized) {
        Cursor *cur = &doc->cursors[0];
        if (!cur->has_selection) cursor_select_start(cur);
        mode->select_mode_initialized = true;
    }

    /* g pending in select mode - goto motions extend the selection */
    if (mode->g_pending) {
        mode->g_pending = false;
        if (key == GLFW_KEY_G)
            document_cursor_doc_start(doc);
        else if (key == GLFW_KEY_E)
            document_cursor_doc_end(doc);
        else if (key == GLFW_KEY_S)
            document_cursor_first_non_blank(doc);
        else if (key == GLFW_KEY_H)
            document_goto_line_start(doc);
        else if (key == GLFW_KEY_L)
            document_goto_line_end(doc);
        else if (key == GLFW_KEY_T)
            document_goto_view_top(doc);
        else if (key == GLFW_KEY_C)
            document_goto_view_center(doc);
        else if (key == GLFW_KEY_B)
            document_goto_view_bottom(doc);
        else if (key == GLFW_KEY_J)
            document_move_cursor(doc, 1, 0);
        else if (key == GLFW_KEY_K)
            document_move_cursor(doc, -1, 0);
        else if (key == GLFW_KEY_PERIOD)
            document_goto_last_modification(doc);
        return;
    }

    /* Ctrl combinations in select mode */
    if (mods & GLFW_MOD_CONTROL) {
        switch (key) {
        case GLFW_KEY_F:
            document_page_down_extend(doc);
            return;
        case GLFW_KEY_B:
            document_page_up_extend(doc);
            return;
        case GLFW_KEY_D:
            document_half_page_down_extend(doc, doc->viewport_lines);
            return;
        case GLFW_KEY_U:
            document_half_page_up_extend(doc, doc->viewport_lines);
            return;
        default: break;
        }
    }

    /* Alt combinations in select mode */
    if (mods & GLFW_MOD_ALT) {
        if (key == GLFW_KEY_SEMICOLON) {
            document_flip_cursor_anchor(doc);
            return;
        }
        if (key == GLFW_KEY_COMMA) {
            document_remove_primary_selection(doc);
            return;
        }
        if (key == GLFW_KEY_D) {
            document_delete_selection(doc);
            mode_set(mode, MODE_NORMAL);
            return;
        }
        if (key == GLFW_KEY_C) {
            document_delete_selection(doc);
            mode_set(mode, MODE_INSERT);
            return;
        }
        if (key == GLFW_KEY_PERIOD) {
            /* Alt-. - Repeat last motion in select mode */
            if (mode->last_motion_type) {
                char c = mode->last_motion_char;
                switch (mode->last_motion_type) {
                case 'f': document_find_char_forward(doc, c); break;
                case 'F': document_find_char_backward(doc, c); break;
                case 't': document_till_char_forward(doc, c); break;
                case 'T': document_till_char_backward(doc, c); break;
                }
            }
            return;
        }
    }

    /* > (Shift+.) - Indent selection */
    if (key == GLFW_KEY_PERIOD && (mods & GLFW_MOD_SHIFT)) {
        document_indent_selection(doc);
        return;
    }

    /* < (Shift+,) - Dedent selection */
    if (key == GLFW_KEY_COMMA && (mods & GLFW_MOD_SHIFT)) {
        document_dedent_selection(doc);
        return;
    }

    /* Pending keys in select mode */
    if (mode->pending_key) {
        char pk = mode->pending_key;
        mode->pending_key = 0;

        if (pk == 'f') {
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_SPACE) c = ' ';
            if (c) {
                document_find_char_forward(doc, c);
                mode->last_motion_type = 'f';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'F') {
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_SPACE) c = ' ';
            if (c) {
                document_find_char_backward(doc, c);
                mode->last_motion_type = 'F';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 't') {
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_SPACE) c = ' ';
            if (c) {
                document_till_char_forward(doc, c);
                mode->last_motion_type = 't';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'T') {
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) c = '0' + (key - GLFW_KEY_0);
            else if (key == GLFW_KEY_SPACE) c = ' ';
            if (c) {
                document_till_char_backward(doc, c);
                mode->last_motion_type = 'T';
                mode->last_motion_char = c;
            }
            return;
        }
        if (pk == 'i') {
            /* Text object in select mode: extends selection */
            char obj = mode->pending_text_obj;
            bool inner = (obj == 'i');
            char c = 0;
            if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) c = 'a' + (key - GLFW_KEY_A);
            else if (key == GLFW_KEY_9) c = '(';
            else if (key == GLFW_KEY_0) c = ')';
            else if (key == GLFW_KEY_LEFT_BRACKET) c = (mods & GLFW_MOD_SHIFT) ? '{' : '[';
            else if (key == GLFW_KEY_RIGHT_BRACKET) c = (mods & GLFW_MOD_SHIFT) ? '}' : ']';
            else if (key == GLFW_KEY_COMMA) c = '<';
            else if (key == GLFW_KEY_PERIOD) c = '>';
            else if (key == GLFW_KEY_MINUS) c = '_';
            else if (key == GLFW_KEY_P) c = 'p';

            if (c) {
                Cursor *cur = &doc->cursors[0];
                if (!cur->has_selection) cursor_select_start(cur);
                switch (c) {
                case 'w': inner ? document_select_inside_word(doc) : document_select_around_word(doc); break;
                case '(': case ')': inner ? document_select_inside_paren(doc) : document_select_around_paren(doc); break;
                case '[': case ']': inner ? document_select_inside_bracket(doc) : document_select_around_bracket(doc); break;
                case '{': case '}': inner ? document_select_inside_curly(doc) : document_select_around_curly(doc); break;
                case '<': inner ? document_select_inside_angle(doc) : document_select_around_angle(doc); break;
                case '"': inner ? document_select_inside_quote(doc) : document_select_around_quote(doc); break;
                case '`': inner ? document_select_inside_backtick(doc) : document_select_around_backtick(doc); break;
                case 'p': inner ? document_select_inside_paragraph(doc) : document_select_around_paragraph(doc); break;
                default: break;
                }
            }
            return;
        }
        return;
    }

    /* i/a - text objects in select mode */
    if (key == GLFW_KEY_I && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode->pending_text_obj = 'i';
        mode->pending_key = 'i';
        return;
    }
    if (key == GLFW_KEY_A && !(mods & GLFW_MOD_SHIFT) && !(mods & GLFW_MOD_CONTROL) && !(mods & GLFW_MOD_ALT)) {
        mode->pending_text_obj = 'a';
        mode->pending_key = 'i';
        return;
    }

    switch (key) {
    case GLFW_KEY_ESCAPE:
        cursor_clear_selection(&doc->cursors[0]);
        mode_set(mode, MODE_NORMAL);
        break;
    /* Navigation - extends selection */
    case GLFW_KEY_H: case GLFW_KEY_LEFT:  document_move_cursor(doc, 0, -1); break;
    case GLFW_KEY_DOWN:  document_move_cursor(doc, 1, 0); break;
    case GLFW_KEY_K: case GLFW_KEY_UP:    document_move_cursor(doc, -1, 0); break;
    case GLFW_KEY_L: case GLFW_KEY_RIGHT: document_move_cursor(doc, 0, 1); break;
    case GLFW_KEY_HOME:  document_cursor_home(doc); break;
    case GLFW_KEY_END:   document_cursor_end(doc); break;
    case GLFW_KEY_PAGE_UP:   document_cursor_page_up(doc); break;
    case GLFW_KEY_PAGE_DOWN: document_cursor_page_down(doc); break;

    /* Word motions */
    case GLFW_KEY_W: document_cursor_word_forward(doc); break;
    case GLFW_KEY_B: document_cursor_word_backward(doc); break;
    case GLFW_KEY_E: document_cursor_word_end(doc); break;

    /* g - enter goto mode (extends selection) */
    case GLFW_KEY_G:
        if (mods & GLFW_MOD_SHIFT)
            document_cursor_doc_end(doc);
        else
            mode->g_pending = true;
        break;

    /* x - Extend selection to next line */
    case GLFW_KEY_X: {
        Cursor *cur = &doc->cursors[0];
        if (!cur->has_selection) {
            document_select_line(doc);
        } else {
            int sr, sc, er, ec;
            cursor_normalize(cur, &sr, &sc, &er, &ec);
            int next_line = er + 1;
            size_t total_lines = buffer_line_count(&doc->buffer);
            if (next_line < (int)total_lines) {
                cur->anchor_col = 0;
                cur->has_selection = true;
                cursor_move_to(cur, next_line, 0);
                int next_len = (int)buffer_line_len(&doc->buffer, next_line);
                cur->col = next_len;
            }
        }
        break;
    }

    /* d - Delete selection */
    case GLFW_KEY_D:
        document_delete_selection(doc);
        mode_set(mode, MODE_NORMAL);
        break;

    /* c - Change selection */
    case GLFW_KEY_C:
        document_delete_selection(doc);
        mode_set(mode, MODE_INSERT);
        break;

    /* y - Yank selection */
    case GLFW_KEY_Y: {
        Cursor *cur = &doc->cursors[0];
        document_yank(doc);
        cursor_clear_selection(cur);
        mode_set(mode, MODE_NORMAL);
        break;
    }

    /* J - Join lines in selection */
    case GLFW_KEY_J:
        document_join_lines_selection(doc);
        break;

    /* p / P - Paste after/before */
    case GLFW_KEY_P:
        if (mods & GLFW_MOD_SHIFT)
            document_paste_before(doc);
        else
            document_paste(doc);
        break;

    /* n / N - Search next/prev (extend selection) */
    case GLFW_KEY_N:
        if (mods & GLFW_MOD_SHIFT)
            document_search_prev(doc);
        else
            document_search_next(doc);
        break;

    /* f/F/t/T - find/till extend selection */
    case GLFW_KEY_F:
        mode->pending_key = (mods & GLFW_MOD_SHIFT) ? 'F' : 'f';
        break;
    case GLFW_KEY_T:
        mode->pending_key = (mods & GLFW_MOD_SHIFT) ? 'T' : 't';
        break;

    /* ~ - Toggle case */
    case GLFW_KEY_GRAVE_ACCENT:
        if (mods & GLFW_MOD_SHIFT)
            document_toggle_case(doc);
        else
            document_lowercase(doc);
        break;

    /* ; - Collapse selection */
    case GLFW_KEY_SEMICOLON:
        document_collapse_selection(doc);
        break;

    /* , - Keep primary selection */
    case GLFW_KEY_COMMA:
        document_keep_primary_selection(doc);
        break;

    /* % - Select entire file */
    case GLFW_KEY_5:
        document_select_all(doc);
        break;

    default: break;
    }
}
