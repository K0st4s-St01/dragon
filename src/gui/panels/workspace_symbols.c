#include "panel_workspace_symbols.h"
#include "app.h"
#include "document.h"
#include "treesitter.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <GLFW/glfw3.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#define WS_MAX_SYMBOLS 512
#define WS_MAX_FILES 256
#define WS_PATH_MAX 1024

typedef struct {
    char name[160];
    char kind[24];
    char path[WS_PATH_MAX];
    int line;
} WorkspaceSymbol;

static bool ws_open = false;
static int ws_selected = 0;
static int ws_scroll = 0;
static WorkspaceSymbol ws_symbols[WS_MAX_SYMBOLS];
static int ws_count = 0;
static int ws_files_seen = 0;

static bool skip_dir(const char *name) {
    return strcmp(name, ".git") == 0 || strcmp(name, "build") == 0 ||
           strcmp(name, "vendor") == 0 || strcmp(name, ".cache") == 0 ||
           strcmp(name, "node_modules") == 0;
}

static const char *path_ext(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(path, '.');
    return (dot && (!slash || dot > slash)) ? dot + 1 : NULL;
}

static char *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    char *data = malloc((size_t)len + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(data, 1, (size_t)len, f);
    fclose(f);
    data[got] = '\0';
    if (len_out) *len_out = got;
    return data;
}

static bool ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static void add_symbol(const char *name, const char *kind, const char *path, int line) {
    if (!name || !name[0] || ws_count >= WS_MAX_SYMBOLS) return;
    snprintf(ws_symbols[ws_count].name, sizeof(ws_symbols[ws_count].name), "%s", name);
    snprintf(ws_symbols[ws_count].kind, sizeof(ws_symbols[ws_count].kind), "%s", kind ? kind : "symbol");
    snprintf(ws_symbols[ws_count].path, sizeof(ws_symbols[ws_count].path), "%s", path);
    ws_symbols[ws_count].line = line;
    ws_count++;
}

static void fallback_scan_symbols(const char *path, const char *ext, const char *text) {
    int line = 0;
    const char *p = text;
    while (*p && ws_count < WS_MAX_SYMBOLS) {
        const char *line_start = p;
        while (*p && *p != '\n') p++;
        const char *line_end = p;
        while (line_start < line_end && isspace((unsigned char)*line_start)) line_start++;

        char name[160] = {0};
        const char *kind = NULL;
        if (strcmp(ext, "py") == 0) {
            if (strncmp(line_start, "def ", 4) == 0 || strncmp(line_start, "class ", 6) == 0) {
                kind = line_start[0] == 'd' ? "function" : "class";
                const char *s = line_start + (line_start[0] == 'd' ? 4 : 6);
                int n = 0;
                while (s < line_end && ident_char(*s) && n < (int)sizeof(name) - 1)
                    name[n++] = *s++;
                name[n] = '\0';
            }
        } else if (strcmp(ext, "go") == 0 && strncmp(line_start, "func ", 5) == 0) {
            kind = "function";
            const char *s = line_start + 5;
            if (*s == '(') {
                while (s < line_end && *s != ')') s++;
                if (s < line_end) s++;
                while (s < line_end && isspace((unsigned char)*s)) s++;
            }
            int n = 0;
            while (s < line_end && ident_char(*s) && n < (int)sizeof(name) - 1)
                name[n++] = *s++;
            name[n] = '\0';
        } else {
            const char *open = strchr(line_start, '(');
            const char *close = open ? strchr(open, ')') : NULL;
            const char *semi = line_start;
            while (semi < line_end && *semi != ';') semi++;
            if (open && close && (!semi || semi >= line_end || semi > close)) {
                const char *s = open;
                while (s > line_start && isspace((unsigned char)s[-1])) s--;
                const char *end = s;
                while (s > line_start && ident_char(s[-1])) s--;
                if (s < end && ident_start(*s)) {
                    int n = (int)(end - s);
                    if (n >= (int)sizeof(name)) n = (int)sizeof(name) - 1;
                    memcpy(name, s, (size_t)n);
                    name[n] = '\0';
                    kind = "function";
                }
            }
        }
        if (kind && name[0])
            add_symbol(name, kind, path, line);

        if (*p == '\n') p++;
        line++;
    }
}

static void scan_file(TreeSitterManager *mgr, const char *path) {
    if (ws_count >= WS_MAX_SYMBOLS || ws_files_seen >= WS_MAX_FILES) return;
    const char *ext = path_ext(path);
    const char *lang_name = treesitter_language_name_for_extension(ext);
    if (!lang_name) return;
    ws_files_seen++;

    size_t len = 0;
    char *text = read_file(path, &len);
    if (!text) return;

    if (!treesitter_load_language(mgr, ext)) {
        fallback_scan_symbols(path, ext, text);
        free(text);
        return;
    }
    TreeSitterLanguage *lang = treesitter_get_language(mgr, lang_name);
    if (!lang) {
        fallback_scan_symbols(path, ext, text);
        free(text);
        return;
    }

    treesitter_parse(lang, text, (uint32_t)len);
    TreeSitterSymbols syms = treesitter_extract_symbols(lang);
    for (uint32_t i = 0; i < syms.count && ws_count < WS_MAX_SYMBOLS; i++) {
        add_symbol(syms.symbols[i].name, syms.symbols[i].kind, path, (int)syms.symbols[i].start_row);
    }
    if (syms.count == 0)
        fallback_scan_symbols(path, ext, text);
    treesitter_symbols_free(&syms);
    free(text);
}

static void scan_dir(TreeSitterManager *mgr, const char *dir, int depth) {
    if (depth > 6 || ws_count >= WS_MAX_SYMBOLS || ws_files_seen >= WS_MAX_FILES) return;
    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char path[WS_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!skip_dir(de->d_name)) scan_dir(mgr, path, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            scan_file(mgr, path);
        }
        if (ws_count >= WS_MAX_SYMBOLS || ws_files_seen >= WS_MAX_FILES) break;
    }
    closedir(d);
}

void panel_workspace_symbols_open(App *app) {
    ws_open = true;
    ws_selected = 0;
    ws_scroll = 0;
    ws_count = 0;
    ws_files_seen = 0;

    TreeSitterManager *mgr = treesitter_manager_new();
    if (!mgr) return;
    scan_dir(mgr, app_get_workspace_root(app), 0);
    treesitter_manager_free(mgr);
}

void panel_workspace_symbols_close(App *app) {
    (void)app;
    ws_open = false;
}

bool panel_workspace_symbols_is_open(void) {
    return ws_open;
}

void panel_workspace_symbols_key(App *app, int key) {
    if (!ws_open) return;
    switch (key) {
    case GLFW_KEY_DOWN:
    case GLFW_KEY_J:
        if (ws_selected < ws_count - 1) ws_selected++;
        break;
    case GLFW_KEY_UP:
    case GLFW_KEY_K:
        if (ws_selected > 0) ws_selected--;
        break;
    case GLFW_KEY_PAGE_DOWN:
        ws_selected += 10;
        if (ws_selected >= ws_count) ws_selected = ws_count > 0 ? ws_count - 1 : 0;
        break;
    case GLFW_KEY_PAGE_UP:
        ws_selected -= 10;
        if (ws_selected < 0) ws_selected = 0;
        break;
    case GLFW_KEY_ENTER:
        if (ws_selected >= 0 && ws_selected < ws_count) {
            app_open_file(app, ws_symbols[ws_selected].path);
            Document *doc = (Document *)app_get_doc(app);
            document_cursor_to(doc, ws_symbols[ws_selected].line, 0);
            document_sync_viewport_to_cursor(doc);
            panel_workspace_symbols_close(app);
        }
        break;
    case GLFW_KEY_ESCAPE:
        panel_workspace_symbols_close(app);
        break;
    default:
        break;
    }
}

void panel_workspace_symbols_render(Gui *g, App *app) {
    if (!ws_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);

    float pw = 780.0f, ph = 500.0f;
    float px = (float)w / 2.0f - pw / 2.0f;
    float py = (float)h / 2.0f - ph / 2.0f;
    renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0, 0, 0, 0.5f);
    renderer_draw_rect(r, px, py, pw, ph, t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py + ph - 1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);
    renderer_draw_rect(r, px + pw - 1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1);

    char title[96];
    snprintf(title, sizeof(title), "Workspace Symbols (%d, %d files)", ws_count, ws_files_seen);
    font_draw(&g->font, r, title, px + 14, py + 10, t->accent[0], t->accent[1], t->accent[2], 1);

    float row_h = g->font.glyph_h + 7;
    int visible = (int)((ph - 72) / row_h);
    if (ws_selected < ws_scroll) ws_scroll = ws_selected;
    if (ws_selected >= ws_scroll + visible) ws_scroll = ws_selected - visible + 1;
    if (ws_scroll < 0) ws_scroll = 0;

    if (ws_count == 0) {
        font_draw(&g->font, r, "No tree-sitter symbols found", px + 18, py + 42,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }
    for (int i = ws_scroll; i < ws_count && (i - ws_scroll) < visible; i++) {
        float y = py + 42 + (i - ws_scroll) * row_h;
        if (i == ws_selected)
            renderer_draw_rect(r, px + 5, y - 2, pw - 10, row_h,
                               t->menu_selected[0], t->menu_selected[1], t->menu_selected[2], t->menu_selected[3]);
        char left[220];
        snprintf(left, sizeof(left), "[%s] %s", ws_symbols[i].kind, ws_symbols[i].name);
        font_draw(&g->font, r, left, px + 18, y, t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], 1);
        char loc[WS_PATH_MAX + 32];
        snprintf(loc, sizeof(loc), "%s:%d", ws_symbols[i].path, ws_symbols[i].line + 1);
        font_draw(&g->font, r, loc, px + 300, y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }

    font_draw(&g->font, r, "Enter go  j/k move  Esc close", px + 14, py + ph - 24,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
