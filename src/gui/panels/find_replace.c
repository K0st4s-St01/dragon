#include <stdio.h>
#include "panel_find_replace.h"
#include "app.h"
#include "document.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <string.h>
#include <GLFW/glfw3.h>

static bool fr_open = false;
static char fr_query[256] = {0};
static int  fr_query_len = 0;
static char fr_replace[256] = {0};
static int  fr_replace_len = 0;
static int  fr_active_field = 0; /* 0=query, 1=replace */
static double fr_last_blink = 0;
static bool fr_cursor_visible = true;
static FindAction fr_action = FR_ACTION_FIND;

void panel_find_open(App *app, Document *doc) {
    (void)doc; (void)app;
    fr_open = true;
    fr_query[0] = '\0';
    fr_query_len = 0;
    fr_replace[0] = '\0';
    fr_replace_len = 0;
    fr_active_field = 0;
    fr_action = FR_ACTION_FIND;
    fr_last_blink = glfwGetTime();
    fr_cursor_visible = true;
}

void panel_find_open_ex(App *app, Document *doc, FindAction action) {
    panel_find_open(app, doc);
    fr_action = action;
}

void panel_find_open_replace(App *app, Document *doc) {
    panel_find_open(app, doc);
    fr_action = FR_ACTION_REPLACE;
}

void panel_find_close(App *app) {
    (void)app;
    fr_open = false;
}

bool panel_find_is_open(void) {
    return fr_open;
}

static bool find_next_range(Document *doc, size_t *start_out, size_t *end_out) {
    if (fr_query_len == 0 || !doc) return false;
    Buffer *buf = &doc->buffer;
    Cursor *cur = &doc->cursors[0];
    size_t start = buffer_pos_from_row_col(buf, cur->row, cur->col) + 1;
    if (start >= buf->len) start = 0;

    const char *text = buf->text + start;
    size_t remaining = buf->len - start;
    const char *found = memmem(text, remaining, fr_query, fr_query_len);
    if (!found) {
        found = memmem(buf->text, buf->len, fr_query, fr_query_len);
        if (!found) return false;
        start = (size_t)(found - buf->text);
    } else {
        start += (size_t)(found - text);
    }

    *start_out = start;
    *end_out = start + (size_t)fr_query_len;
    return true;
}

static bool find_next(Document *doc) {
    size_t start = 0, end = 0;
    if (!find_next_range(doc, &start, &end)) return false;
    int row, col;
    buffer_row_col_from_pos(&doc->buffer, start, &row, &col);
    cursor_move_to(&doc->cursors[0], row, col);
    return true;
}

static bool replace_next(Document *doc) {
    if (fr_query_len == 0 || !doc) return false;
    Cursor *cur = &doc->cursors[0];
    bool replace_selection = false;

    if (cur->has_selection) {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        size_t start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
        size_t end = buffer_pos_from_row_col(&doc->buffer, er, ec);
        if (end >= start && end - start == (size_t)fr_query_len &&
            memcmp(doc->buffer.text + start, fr_query, (size_t)fr_query_len) == 0) {
            replace_selection = true;
        }
    }

    size_t start = 0, end = 0;
    if (replace_selection) {
        int sr, sc, er, ec;
        cursor_normalize(cur, &sr, &sc, &er, &ec);
        start = buffer_pos_from_row_col(&doc->buffer, sr, sc);
        end = buffer_pos_from_row_col(&doc->buffer, er, ec);
    } else if (!find_next_range(doc, &start, &end)) {
        return false;
    }

    buffer_delete(&doc->buffer, start, end - start);
    buffer_insert(&doc->buffer, start, fr_replace, (size_t)fr_replace_len);
    buffer_row_col_from_pos(&doc->buffer, start + (size_t)fr_replace_len, &cur->row, &cur->col);
    cursor_clear_selection(cur);
    document_mark_dirty(doc);
    return true;
}

void panel_find_input(App *app, Document *doc, unsigned int c) {
    (void)doc;
    if (!fr_open) return;
    if (c == '\t') {
        fr_active_field = 1 - fr_active_field;
        return;
    }
    if (fr_active_field == 0) {
        if (c == 8 && fr_query_len > 0) {
            fr_query[--fr_query_len] = '\0';
        } else if (c >= 32 && c < 127 && fr_query_len < 255) {
            fr_query[fr_query_len++] = (char)c;
            fr_query[fr_query_len] = '\0';
        }
    } else {
        if (c == 8 && fr_replace_len > 0) {
            fr_replace[--fr_replace_len] = '\0';
        } else if (c >= 32 && c < 127 && fr_replace_len < 255) {
            fr_replace[fr_replace_len++] = (char)c;
            fr_replace[fr_replace_len] = '\0';
        }
    }
    (void)app;
}

void panel_find_key(App *app, Document *doc, int key) {
    if (!fr_open) return;
    if (key == GLFW_KEY_ENTER) {
        if (fr_action == FR_ACTION_FIND) {
            find_next(doc);
        } else if (fr_action == FR_ACTION_REPLACE) {
            replace_next(doc);
        } else if (fr_action == FR_ACTION_SELECT) {
            document_select_all_matches(doc, fr_query, fr_query_len);
            panel_find_close(app);
            return;
        } else if (fr_action == FR_ACTION_SPLIT) {
            document_split_all_matches(doc, fr_query, fr_query_len);
            panel_find_close(app);
            return;
        } else if (fr_action == FR_ACTION_KEEP) {
            document_keep_matching(doc, fr_query, fr_query_len);
            panel_find_close(app);
            return;
        } else if (fr_action == FR_ACTION_REMOVE) {
            document_remove_matching(doc, fr_query, fr_query_len);
            panel_find_close(app);
            return;
        } else if (fr_action == FR_ACTION_PIPE) {
            document_pipe_selection(doc, fr_query);
            panel_find_close(app);
            return;
        } else if (fr_action == FR_ACTION_PIPE_TO) {
            document_pipe_to(doc, fr_query);
            panel_find_close(app);
            return;
        } else if (fr_action == FR_ACTION_INSERT_OUTPUT) {
            document_insert_output(doc, fr_query);
            panel_find_close(app);
            return;
        } else if (fr_action == FR_ACTION_APPEND_OUTPUT) {
            document_append_output(doc, fr_query);
            panel_find_close(app);
            return;
        } else if (fr_action == FR_ACTION_FILTER) {
            document_filter_selection(doc, fr_query);
            panel_find_close(app);
            return;
        }
    } else if (key == GLFW_KEY_TAB) {
        fr_active_field = 1 - fr_active_field;
    } else if (key == GLFW_KEY_ESCAPE) {
        panel_find_close(app);
        return;
    }
    (void)app;
}

void panel_find_render(Gui *g, App *app, Document *doc) {
    (void)doc;
    if (!fr_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);

    double now = glfwGetTime();
    if (now - fr_last_blink > 0.5) {
        fr_cursor_visible = !fr_cursor_visible;
        fr_last_blink = now;
    }

    float pw = 400, ph = 160;
    float px = (float)w/2 - pw/2;
    float py = 80;

    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py+ph-1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px+pw-1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);

    /* Determine label based on action */
    const char *label = "Find:";
    const char *btn_label = "Find Next";
    if (fr_action == FR_ACTION_REPLACE) {
        label = "Find:";
        btn_label = "Replace Next";
    } else if (fr_action == FR_ACTION_PIPE) {
        label = "Pipe |:";
        btn_label = "Execute";
    } else if (fr_action == FR_ACTION_PIPE_TO) {
        label = "Pipe to Alt-|:";
        btn_label = "Execute";
    } else if (fr_action == FR_ACTION_INSERT_OUTPUT) {
        label = "Insert !:";
        btn_label = "Execute";
    } else if (fr_action == FR_ACTION_APPEND_OUTPUT) {
        label = "Append Alt-!:";
        btn_label = "Execute";
    } else if (fr_action == FR_ACTION_FILTER) {
        label = "Filter $:";
        btn_label = "Execute";
    } else if (fr_action == FR_ACTION_SELECT) {
        btn_label = "Select All";
    } else if (fr_action == FR_ACTION_SPLIT) {
        btn_label = "Split";
    } else if (fr_action == FR_ACTION_KEEP) {
        btn_label = "Keep";
    } else if (fr_action == FR_ACTION_REMOVE) {
        btn_label = "Remove";
    }

    font_draw(&g->font, r, label, px+14, py+10, t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
    renderer_draw_rect(r, px+110, py+6, pw-124, g->font.glyph_h+8,
                       t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);
    char buf[260];
    snprintf(buf, sizeof(buf), "%s%s", fr_query, (fr_active_field==0 && fr_cursor_visible) ? "_" : " ");
    font_draw(&g->font, r, buf, px+114, py+10,
              t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);

    /* Hide replace field for shell commands */
    if (fr_action < FR_ACTION_PIPE) {
        font_draw(&g->font, r, "Replace:", px+14, py+36, t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
        renderer_draw_rect(r, px+70, py+32, pw-84, g->font.glyph_h+8,
                           t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);
        snprintf(buf, sizeof(buf), "%s%s", fr_replace, (fr_active_field==1 && fr_cursor_visible) ? "_" : " ");
        font_draw(&g->font, r, buf, px+74, py+36,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
    }

    /* Buttons */
    float btn_y = py + 60;
    renderer_draw_rect(r, px+14, btn_y, 120, g->font.glyph_h+8,
                       t->accent[0], t->accent[1], t->accent[2], 1);
    font_draw(&g->font, r, btn_label, px+20, btn_y+4, 1, 1, 1, 1);

    renderer_draw_rect(r, px+144, btn_y, 90, g->font.glyph_h+8,
                       t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);
    font_draw(&g->font, r, "Close", px+164, btn_y+4,
              t->fg[0], t->fg[1], t->fg[2], t->fg[3]);
}
