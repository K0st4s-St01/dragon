#include "panel_completion.h"
#include "app.h"
#include "document.h"
#include "buffer.h"
#include "lsp.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <GLFW/glfw3.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define COMPLETION_MAX 128
#define COMPLETION_LABEL_MAX 128

typedef struct {
    char label[COMPLETION_LABEL_MAX];
    char detail[192];
} CompletionEntry;

static bool completion_open = false;
static int completion_selected = 0;
static int completion_scroll = 0;
static int completion_count = 0;
static int completion_prefix_len = 0;
static CompletionEntry completion_entries[COMPLETION_MAX];

static bool word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static int current_prefix(Document *doc, char *buf, int size) {
    Cursor *cur = &doc->cursors[0];
    const char *line = buffer_line_ptr(&doc->buffer, cur->row);
    int col = cur->col;
    int start = col;
    while (start > 0 && word_char(line[start - 1])) start--;
    int len = col - start;
    if (len >= size) len = size - 1;
    memcpy(buf, line + start, len);
    buf[len] = '\0';
    return len;
}

static bool has_label(const char *label) {
    for (int i = 0; i < completion_count; i++)
        if (strcmp(completion_entries[i].label, label) == 0) return true;
    return false;
}

static void add_completion(const char *label, const char *detail, const char *prefix) {
    if (!label || !label[0] || completion_count >= COMPLETION_MAX) return;
    if (prefix && prefix[0] && strncmp(label, prefix, strlen(prefix)) != 0) return;
    if (has_label(label)) return;
    snprintf(completion_entries[completion_count].label, COMPLETION_LABEL_MAX, "%s", label);
    snprintf(completion_entries[completion_count].detail, sizeof(completion_entries[completion_count].detail),
             "%s", detail ? detail : "");
    completion_count++;
}

static char *file_uri(const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path) + 8;
    char *uri = malloc(len);
    if (!uri) return NULL;
    snprintf(uri, len, "file://%s", path);
    return uri;
}

static void add_lsp_completions(App *app, Document *doc, const char *prefix) {
    if (!doc || !doc->language_id || !doc->filepath) return;
    LSPManager *manager = (LSPManager *)app_get_lsp_manager(app);
    LSPClient *client = lsp_manager_get_client(manager, doc->language_id);
    if (!client) return;

    char *uri = file_uri(doc->filepath);
    if (!uri) return;
    Cursor *cur = &doc->cursors[0];
    lsp_client_send_completion_request(client, uri, cur->row, cur->col);
    usleep(60000);
    char *response = lsp_client_read_response(client);
    if (response) {
        LSPCompletionItems *items = lsp_parse_completion_response(response);
        for (int i = 0; items && i < items->count; i++)
            add_completion(items->items[i].label, items->items[i].detail, prefix);
        lsp_free_completion_items(items);
        free(response);
    }
    free(uri);
}

static void add_buffer_words(Document *doc, const char *prefix) {
    int prefix_len = (int)strlen(prefix);
    for (size_t row = 0; row < buffer_line_count(&doc->buffer); row++) {
        const char *line = buffer_line_ptr(&doc->buffer, row);
        int len = (int)buffer_line_len(&doc->buffer, row);
        int i = 0;
        while (i < len) {
            while (i < len && !word_char(line[i])) i++;
            int start = i;
            while (i < len && word_char(line[i])) i++;
            int word_len = i - start;
            if (word_len > prefix_len && word_len < COMPLETION_LABEL_MAX) {
                char word[COMPLETION_LABEL_MAX];
                memcpy(word, line + start, word_len);
                word[word_len] = '\0';
                add_completion(word, "buffer", prefix);
            }
        }
    }
}

void panel_completion_open(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    char prefix[COMPLETION_LABEL_MAX];
    completion_prefix_len = current_prefix(doc, prefix, sizeof(prefix));
    completion_selected = 0;
    completion_scroll = 0;
    completion_count = 0;
    add_lsp_completions(app, doc, prefix);
    add_buffer_words(doc, prefix);
    completion_open = completion_count > 0;
}

void panel_completion_close(App *app) {
    (void)app;
    completion_open = false;
}

bool panel_completion_is_open(void) {
    return completion_open;
}

static void accept_completion(App *app) {
    if (completion_selected < 0 || completion_selected >= completion_count) return;
    Document *doc = (Document *)app_get_doc(app);
    const char *label = completion_entries[completion_selected].label;
    int label_len = (int)strlen(label);
    if (label_len > completion_prefix_len)
        document_insert_text(doc, label + completion_prefix_len);
    panel_completion_close(app);
}

void panel_completion_key(App *app, int key, int mods) {
    if (!completion_open) return;
    if (key == GLFW_KEY_UP ||
               (key == GLFW_KEY_TAB && (mods & GLFW_MOD_SHIFT)) ||
               (key == GLFW_KEY_P && (mods & GLFW_MOD_CONTROL))) {
        if (completion_selected > 0) completion_selected--;
    } else if (key == GLFW_KEY_DOWN || key == GLFW_KEY_TAB ||
        (key == GLFW_KEY_N && (mods & GLFW_MOD_CONTROL))) {
        if (completion_selected < completion_count - 1) completion_selected++;
    } else if (key == GLFW_KEY_ENTER) {
        accept_completion(app);
    } else if (key == GLFW_KEY_ESCAPE || (key == GLFW_KEY_C && (mods & GLFW_MOD_CONTROL))) {
        panel_completion_close(app);
    } else {
        panel_completion_close(app);
    }
}

void panel_completion_render(Gui *g, App *app) {
    if (!completion_open) return;
    Document *doc = (Document *)app_get_doc(app);
    Cursor *cur = &doc->cursors[0];
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);

    float row_h = g->font.glyph_h + 7;
    int visible = completion_count < 8 ? completion_count : 8;
    float pw = 360;
    float ph = visible * row_h + 56;
    float px = 80 + cur->col * g->font.glyph_w;
    float py = 48 + cur->row * row_h;
    if (px + pw > w - 12) px = (float)w - pw - 12;
    if (py + ph > h - 34) py = (float)h - ph - 34;

    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], 0.98f);
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py + ph - 1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px + pw - 1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);

    if (completion_selected < completion_scroll) completion_scroll = completion_selected;
    if (completion_selected >= completion_scroll + visible)
        completion_scroll = completion_selected - visible + 1;

    for (int i = completion_scroll; i < completion_count && (i - completion_scroll) < visible; i++) {
        float y = py + 8 + (i - completion_scroll) * row_h;
        if (i == completion_selected)
            renderer_draw_rect(r, px + 4, y - 2, pw - 8, row_h,
                               t->menu_selected[0], t->menu_selected[1], t->menu_selected[2], t->menu_selected[3]);
        font_draw(&g->font, r, completion_entries[i].label, px + 12, y,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], 1);
        font_draw(&g->font, r, completion_entries[i].detail, px + 180, y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }

    if (completion_selected >= 0 && completion_selected < completion_count &&
        completion_entries[completion_selected].detail[0]) {
        font_draw(&g->font, r, completion_entries[completion_selected].detail,
                  px + 12, py + ph - 24,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }
}
