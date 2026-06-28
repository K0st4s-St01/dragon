#include <stdio.h>
#include "panel_file_browser.h"
#include "app.h"
#include "document.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <GLFW/glfw3.h>

#define FB_PATH_MAX     1024
#define FB_MAX_ENTRIES  2048
#define FB_MAX_EXPANDED 256

typedef struct {
    char path[FB_PATH_MAX];   /* full path */
    char name[256];           /* display name */
    int  depth;
    bool is_dir;
} FbEntry;

typedef enum {
    FB_MODE_OPEN_FILE,
    FB_MODE_SAVE_AS,
    FB_MODE_CHANGE_DIR,
    FB_MODE_WORKSPACE,
    FB_MODE_NEW_FILE,
    FB_MODE_NEW_FOLDER,
    FB_MODE_RENAME,
    FB_MODE_DELETE,
} FbMode;

static bool    fb_open = false;
static int     fb_selected = 0;
static int     fb_scroll = 0;
static FbMode  fb_mode = FB_MODE_OPEN_FILE;
static char    fb_root[FB_PATH_MAX];
static char    fb_path_input[FB_PATH_MAX];
static int     fb_path_len = 0;
static FbEntry fb_entries[FB_MAX_ENTRIES];
static int     fb_count = 0;
static int     fb_action_index = -1;

/* Set of expanded directory paths */
static char fb_expanded[FB_MAX_EXPANDED][FB_PATH_MAX];
static int  fb_expanded_count = 0;

static bool fb_is_expanded(const char *path) {
    for (int i = 0; i < fb_expanded_count; i++)
        if (strcmp(fb_expanded[i], path) == 0) return true;
    return false;
}

static void fb_set_expanded(const char *path, bool expanded) {
    int idx = -1;
    for (int i = 0; i < fb_expanded_count; i++)
        if (strcmp(fb_expanded[i], path) == 0) { idx = i; break; }

    if (expanded && idx < 0 && fb_expanded_count < FB_MAX_EXPANDED) {
        snprintf(fb_expanded[fb_expanded_count], FB_PATH_MAX, "%s", path);
        fb_expanded_count++;
    } else if (!expanded && idx >= 0) {
        for (int i = idx; i < fb_expanded_count - 1; i++)
            memcpy(fb_expanded[i], fb_expanded[i + 1], FB_PATH_MAX);
        fb_expanded_count--;
    }
}

static int fb_cmp(const void *a, const void *b) {
    const FbEntry *ea = (const FbEntry *)a;
    const FbEntry *eb = (const FbEntry *)b;
    if (ea->is_dir != eb->is_dir) return ea->is_dir ? -1 : 1;
    return strcmp(ea->name, eb->name);
}

static void fb_scan_dir(const char *dir, int depth) {
    DIR *d = opendir(dir);
    if (!d) return;

    /* Collect this directory's children, then sort. */
    FbEntry tmp[512];
    int n = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL && n < 512) {
        if (de->d_name[0] == '.') continue; /* skip hidden, . and .. */

        FbEntry e;
        snprintf(e.path, sizeof(e.path), "%s/%s", dir, de->d_name);
        snprintf(e.name, sizeof(e.name), "%s", de->d_name);
        e.depth = depth;

        struct stat st;
        e.is_dir = false;
        if (stat(e.path, &st) == 0)
            e.is_dir = S_ISDIR(st.st_mode);

        tmp[n++] = e;
    }
    closedir(d);

    qsort(tmp, n, sizeof(FbEntry), fb_cmp);

    for (int i = 0; i < n; i++) {
        if (fb_count >= FB_MAX_ENTRIES) return;
        fb_entries[fb_count++] = tmp[i];
        if (tmp[i].is_dir && fb_is_expanded(tmp[i].path))
            fb_scan_dir(tmp[i].path, depth + 1);
    }
}

static void fb_rebuild(void) {
    fb_count = 0;
    
    /* Add parent directory entry (..) at the top */
    if (strcmp(fb_root, "/") != 0) {
        FbEntry parent;
        /* Compute parent path more safely */
        char parent_path[FB_PATH_MAX];
        snprintf(parent_path, sizeof(parent_path), "%s", fb_root);
        char *last_slash = strrchr(parent_path, '/');
        if (last_slash && last_slash != parent_path) {
            *last_slash = '\0';
        } else if (last_slash == parent_path) {
            snprintf(parent_path, sizeof(parent_path), "/");
        }
        snprintf(parent.path, sizeof(parent.path), "%s", parent_path);
        snprintf(parent.name, sizeof(parent.name), "..");
        parent.depth = 0;
        parent.is_dir = true;
        fb_entries[fb_count++] = parent;
    }
    
    fb_scan_dir(fb_root, 0);
    if (fb_selected >= fb_count) fb_selected = fb_count > 0 ? fb_count - 1 : 0;
    if (fb_selected < 0) fb_selected = 0;
}

static void fb_set_input_join(const char *dir, const char *name) {
    const char *base = (dir && *dir) ? dir : ".";
    size_t base_len = strlen(base);
    size_t pos = base_len < FB_PATH_MAX - 1 ? base_len : FB_PATH_MAX - 1;
    memcpy(fb_path_input, base, pos);
    if (pos > 0 && fb_path_input[pos - 1] != '/' && pos < FB_PATH_MAX - 1)
        fb_path_input[pos++] = '/';
    for (size_t i = 0; name[i] && pos < FB_PATH_MAX - 1; i++)
        fb_path_input[pos++] = name[i];
    fb_path_input[pos] = '\0';
}

static void fb_open_at_root(App *app, const char *root, FbMode mode) {
    (void)app;
    fb_open = true;
    fb_selected = 0;
    fb_scroll = 0;
    fb_mode = mode;
    fb_path_input[0] = '\0';
    fb_path_len = 0;
    if (root && *root) {
        char resolved[FB_PATH_MAX];
        if (realpath(root, resolved))
            snprintf(fb_root, sizeof(fb_root), "%s", resolved);
        else
            snprintf(fb_root, sizeof(fb_root), "%s", root);
    } else if (!getcwd(fb_root, sizeof(fb_root))) {
        snprintf(fb_root, sizeof(fb_root), ".");
    }
    fb_rebuild();
}

void panel_file_browser_open(App *app) {
    fb_open_at_root(app, NULL, FB_MODE_OPEN_FILE);
}

void panel_file_browser_open_at(App *app, const char *root) {
    fb_open_at_root(app, root ? root : ".", FB_MODE_OPEN_FILE);
}

void panel_file_browser_open_at_home(App *app) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    fb_open_at_root(app, home, FB_MODE_OPEN_FILE);
}

void panel_file_browser_open_save_as(App *app) {
    Document *doc = (Document *)app_get_doc(app);
    const char *root = app_get_workspace_root(app);
    if (doc && doc->filepath) {
        snprintf(fb_path_input, sizeof(fb_path_input), "%s", doc->filepath);
        char dir[FB_PATH_MAX];
        snprintf(dir, sizeof(dir), "%s", doc->filepath);
        char *slash = strrchr(dir, '/');
        if (slash && slash != dir) {
            *slash = '\0';
            root = dir;
        } else if (slash == dir) {
            root = "/";
        }
    } else {
        fb_set_input_join(root, "untitled.txt");
    }
    fb_open_at_root(app, root, FB_MODE_SAVE_AS);
    if (doc && doc->filepath) {
        snprintf(fb_path_input, sizeof(fb_path_input), "%s", doc->filepath);
    } else {
        fb_set_input_join(fb_root, "untitled.txt");
    }
    fb_path_len = (int)strlen(fb_path_input);
}

void panel_file_browser_open_change_dir(App *app) {
    fb_open_at_root(app, app_get_workspace_root(app), FB_MODE_CHANGE_DIR);
}

void panel_file_browser_open_workspace(App *app) {
    fb_open_at_root(app, app_get_workspace_root(app), FB_MODE_WORKSPACE);
}

void panel_file_browser_close(App *app) {
    (void)app;
    fb_open = false;
}

bool panel_file_browser_is_open(void) {
    return fb_open;
}

void panel_file_browser_input(App *app, unsigned int c) {
    (void)app;
    if (!fb_open) return;

    /* Handle delete confirmation */
    if (fb_mode == FB_MODE_DELETE) {
        if (c == 'y' || c == 'Y') {
            if (fb_action_index >= 0 && fb_action_index < fb_count) {
                FbEntry *e = &fb_entries[fb_action_index];
                if (e->is_dir)
                    rmdir(e->path);
                else
                    remove(e->path);
                fb_rebuild();
            }
        }
        fb_mode = FB_MODE_OPEN_FILE;
        fb_action_index = -1;
        return;
    }

    /* Handle action triggers in OPEN_FILE mode */
    if (fb_mode == FB_MODE_OPEN_FILE) {
        if (c == 'a') {
            fb_mode = FB_MODE_NEW_FILE;
            fb_path_input[0] = '\0';
            fb_path_len = 0;
            return;
        }
        if (c == 'A') {
            fb_mode = FB_MODE_NEW_FOLDER;
            fb_path_input[0] = '\0';
            fb_path_len = 0;
            return;
        }
        if (c == 'r' && fb_count > 0) {
            fb_action_index = fb_selected;
            fb_mode = FB_MODE_RENAME;
            snprintf(fb_path_input, sizeof(fb_path_input), "%s", fb_entries[fb_selected].name);
            fb_path_len = (int)strlen(fb_path_input);
            return;
        }
        if (c == 'd' && fb_count > 0) {
            fb_action_index = fb_selected;
            fb_mode = FB_MODE_DELETE;
            return;
        }
        return;
    }

    /* Text input for SAVE_AS, NEW_FILE, NEW_FOLDER, RENAME */
    if (fb_mode == FB_MODE_SAVE_AS || fb_mode == FB_MODE_NEW_FILE ||
        fb_mode == FB_MODE_NEW_FOLDER || fb_mode == FB_MODE_RENAME) {
        if (c >= 32 && c < 127 && fb_path_len < FB_PATH_MAX - 1) {
            fb_path_input[fb_path_len++] = (char)c;
            fb_path_input[fb_path_len] = '\0';
        }
    }
}

void panel_file_browser_key(App *app, int key) {
    if (!fb_open) return;

    switch (key) {
    case GLFW_KEY_BACKSPACE:
        if ((fb_mode == FB_MODE_SAVE_AS || fb_mode == FB_MODE_NEW_FILE ||
             fb_mode == FB_MODE_NEW_FOLDER || fb_mode == FB_MODE_RENAME) && fb_path_len > 0) {
            fb_path_input[--fb_path_len] = '\0';
        }
        break;
    case GLFW_KEY_DOWN:
    case GLFW_KEY_J:
    case GLFW_KEY_N: /* Ctrl-n routed as plain n here */
        if (fb_selected < fb_count - 1) fb_selected++;
        break;
    case GLFW_KEY_UP:
    case GLFW_KEY_K:
    case GLFW_KEY_P:
        if (fb_selected > 0) fb_selected--;
        break;
    case GLFW_KEY_PAGE_DOWN:
        fb_selected += 10;
        if (fb_selected >= fb_count) fb_selected = fb_count > 0 ? fb_count - 1 : 0;
        break;
    case GLFW_KEY_PAGE_UP:
        fb_selected -= 10;
        if (fb_selected < 0) fb_selected = 0;
        break;
    case GLFW_KEY_HOME:
        fb_selected = 0;
        break;
    case GLFW_KEY_END:
        fb_selected = fb_count > 0 ? fb_count - 1 : 0;
        break;
    case GLFW_KEY_LEFT:
    case GLFW_KEY_H: {
        /* Collapse current dir, or jump to parent entry */
        if (fb_count == 0) break;
        FbEntry *e = &fb_entries[fb_selected];
        if (e->is_dir && fb_is_expanded(e->path)) {
            fb_set_expanded(e->path, false);
            fb_rebuild();
        } else {
            int depth = e->depth;
            for (int i = fb_selected - 1; i >= 0; i--) {
                if (fb_entries[i].depth < depth) { fb_selected = i; break; }
            }
        }
        break;
    }
    case GLFW_KEY_RIGHT:
    case GLFW_KEY_L: {
        if (fb_count == 0) break;
        FbEntry *e = &fb_entries[fb_selected];
        if (e->is_dir && !fb_is_expanded(e->path)) {
            fb_set_expanded(e->path, true);
            fb_rebuild();
        }
        break;
    }
    case GLFW_KEY_ENTER: {
        if (fb_mode == FB_MODE_NEW_FILE) {
            if (fb_path_len > 0) {
                const char *target = fb_root;
                if (fb_count > 0) {
                    FbEntry *sel = &fb_entries[fb_selected];
                    if (sel->is_dir && strcmp(sel->name, "..") != 0)
                        target = sel->path;
                }
                char fullpath[FB_PATH_MAX];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", target, fb_path_input);
                int fd = open(fullpath, O_CREAT | O_EXCL | O_WRONLY, 0644);
                if (fd >= 0) close(fd);
                if (fb_count > 0 && fb_entries[fb_selected].is_dir &&
                    strcmp(fb_entries[fb_selected].name, "..") != 0)
                    fb_set_expanded(fb_entries[fb_selected].path, true);
            }
            fb_mode = FB_MODE_OPEN_FILE;
            fb_rebuild();
            break;
        }
        if (fb_mode == FB_MODE_NEW_FOLDER) {
            if (fb_path_len > 0) {
                const char *target = fb_root;
                if (fb_count > 0) {
                    FbEntry *sel = &fb_entries[fb_selected];
                    if (sel->is_dir && strcmp(sel->name, "..") != 0)
                        target = sel->path;
                }
                char fullpath[FB_PATH_MAX];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", target, fb_path_input);
                mkdir(fullpath, 0755);
                if (fb_count > 0 && fb_entries[fb_selected].is_dir &&
                    strcmp(fb_entries[fb_selected].name, "..") != 0)
                    fb_set_expanded(fb_entries[fb_selected].path, true);
            }
            fb_mode = FB_MODE_OPEN_FILE;
            fb_rebuild();
            break;
        }
        if (fb_mode == FB_MODE_RENAME) {
            if (fb_path_len > 0 && fb_action_index >= 0 && fb_action_index < fb_count) {
                FbEntry *e = &fb_entries[fb_action_index];
                char newpath[FB_PATH_MAX];
                char dir[FB_PATH_MAX];
                snprintf(dir, sizeof(dir), "%s", e->path);
                char *last_slash = strrchr(dir, '/');
                if (last_slash) {
                    *last_slash = '\0';
                    snprintf(newpath, sizeof(newpath), "%s/%s", dir, fb_path_input);
                } else {
                    snprintf(newpath, sizeof(newpath), "%s/%s", fb_root, fb_path_input);
                }
                rename(e->path, newpath);
            }
            fb_mode = FB_MODE_OPEN_FILE;
            fb_action_index = -1;
            fb_rebuild();
            break;
        }
        if (fb_mode == FB_MODE_SAVE_AS) {
            Document *doc = (Document *)app_get_doc(app);
            if (doc && fb_path_len > 0) {
                document_save_as(doc, fb_path_input);
                panel_file_browser_close(app);
            }
            break;
        }
        if (fb_count == 0) {
            if (fb_mode == FB_MODE_CHANGE_DIR && chdir(fb_root) == 0) {
                app_set_workspace_root(app, fb_root);
                panel_file_browser_close(app);
            } else if (fb_mode == FB_MODE_WORKSPACE) {
                app_set_workspace_root(app, fb_root);
                panel_file_browser_close(app);
            }
            break;
        }
        FbEntry *e = &fb_entries[fb_selected];
        if (fb_mode == FB_MODE_CHANGE_DIR) {
            const char *path = e->is_dir ? e->path : fb_root;
            if (chdir(path) == 0) {
                app_set_workspace_root(app, path);
                panel_file_browser_close(app);
            }
        } else if (fb_mode == FB_MODE_WORKSPACE) {
            const char *path = e->is_dir ? e->path : fb_root;
            app_set_workspace_root(app, path);
            panel_file_browser_close(app);
        } else if (e->is_dir) {
            /* Handle parent directory (..) specially */
            if (strcmp(e->name, "..") == 0) {
                /* Navigate to parent directory */
                char parent[FB_PATH_MAX];
                snprintf(parent, sizeof(parent), "%s", fb_root);
                char *last_slash = strrchr(parent, '/');
                if (last_slash && last_slash != parent) {
                    *last_slash = '\0';
                    snprintf(fb_root, sizeof(fb_root), "%s", parent);
                } else if (last_slash == parent) {
                    snprintf(fb_root, sizeof(fb_root), "/");
                }
                fb_selected = 0;
                fb_scroll = 0;
                fb_rebuild();
            } else {
                /* Toggle expand/collapse for regular directories */
                fb_set_expanded(e->path, !fb_is_expanded(e->path));
                fb_rebuild();
            }
        } else {
            app_open_file(app, e->path);
            panel_file_browser_close(app);
        }
        break;
    }
    case GLFW_KEY_W: {
        /* Set workspace root to selected directory */
        if (fb_count == 0) break;
        FbEntry *e = &fb_entries[fb_selected];
        if (e->is_dir) {
            app_set_workspace_root(app, e->path);
            panel_file_browser_close(app);
        }
        break;
    }
    default:
        break;
    }
}

void panel_file_browser_render(Gui *g, App *app) {
    if (!fb_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);

    float pw = 460.0f;
    float ph = (float)h - 80.0f;
    float px = 20.0f;
    float py = 40.0f;

    /* Dim background */
    renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0.0f, 0.0f, 0.0f, 0.4f);

    /* Panel background */
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);

    /* Border */
    renderer_draw_rect(r, px, py, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py+ph-1, pw, 1, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px+pw-1, py, 1, ph, t->accent[0], t->accent[1], t->accent[2], 1.0f);

    /* Title - show root basename */
    const char *root_name = strrchr(fb_root, '/');
    root_name = root_name ? root_name + 1 : fb_root;
    char title[FB_PATH_MAX + 16];
    const char *prefix = "Files";
    if (fb_mode == FB_MODE_SAVE_AS) prefix = "Save as";
    else if (fb_mode == FB_MODE_CHANGE_DIR) prefix = "Change dir";
    else if (fb_mode == FB_MODE_WORKSPACE) prefix = "Workspace";
    else if (fb_mode == FB_MODE_NEW_FILE) prefix = "New file";
    else if (fb_mode == FB_MODE_NEW_FOLDER) prefix = "New folder";
    else if (fb_mode == FB_MODE_RENAME) prefix = "Rename";
    else if (fb_mode == FB_MODE_DELETE) prefix = "Delete";
    snprintf(title, sizeof(title), "%s: %s", prefix, root_name);
    font_draw(&g->font, r, title, px + 14, py + 10,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);

    /* List */
    float list_y = py + 40;
    if (fb_mode == FB_MODE_SAVE_AS || fb_mode == FB_MODE_NEW_FILE ||
        fb_mode == FB_MODE_NEW_FOLDER || fb_mode == FB_MODE_RENAME) {
        renderer_draw_rect(r, px + 14, list_y - 2, pw - 28, g->font.glyph_h + 8,
                           t->gutter_bg[0], t->gutter_bg[1], t->gutter_bg[2], t->gutter_bg[3]);
        char input[FB_PATH_MAX + 2];
        snprintf(input, sizeof(input), "%s_", fb_path_input);
        font_draw(&g->font, r, input, px + 18, list_y + 2,
                  t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], t->menu_fg[3]);
        list_y += g->font.glyph_h + 18;
    }
    float line_h = g->font.glyph_h + 6;
    int max_visible = (int)((ph - 64) / line_h);
    if (max_visible < 1) max_visible = 1;

    /* Keep selection in view */
    if (fb_selected < fb_scroll) fb_scroll = fb_selected;
    if (fb_selected >= fb_scroll + max_visible) fb_scroll = fb_selected - max_visible + 1;
    if (fb_scroll < 0) fb_scroll = 0;

    if (fb_count == 0) {
        font_draw(&g->font, r, "(empty directory)", px + 14, list_y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }

    for (int i = fb_scroll; i < fb_count && (i - fb_scroll) < max_visible; i++) {
        FbEntry *e = &fb_entries[i];
        float ry = list_y + (i - fb_scroll) * line_h;
        bool sel = (i == fb_selected);

        if (sel) {
            renderer_draw_rect(r, px + 4, ry - 2, pw - 8, line_h,
                               t->menu_selected[0], t->menu_selected[1],
                               t->menu_selected[2], t->menu_selected[3]);
        }

        float indent = (float)e->depth * 16.0f;
        float tx = px + 14 + indent;

        /* Prefix marker */
        const char *marker;
        if (e->is_dir)
            marker = fb_is_expanded(e->path) ? "v" : ">";
        else
            marker = " ";
        font_draw(&g->font, r, marker, tx, ry,
                  t->accent[0], t->accent[1], t->accent[2], 1.0f);

        /* Name (directories get a trailing slash) */
        char label[300];
        if (e->is_dir)
            snprintf(label, sizeof(label), "%s/", e->name);
        else
            snprintf(label, sizeof(label), "%s", e->name);

        float nr = e->is_dir ? t->accent[0] : t->menu_fg[0];
        float ng = e->is_dir ? t->accent[1] : t->menu_fg[1];
        float nb = e->is_dir ? t->accent[2] : t->menu_fg[2];
        font_draw(&g->font, r, label, tx + 16, ry, nr, ng, nb, 1.0f);
    }

    /* Footer help */
    float footer_y = py + ph - 24;
    renderer_draw_rect(r, px, footer_y - 4, pw, 1,
                       t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], 0.3f);
    font_draw(&g->font, r,
              fb_mode == FB_MODE_SAVE_AS
                  ? "type path  Enter save  j/k move  l expand  Esc close"
                  : fb_mode == FB_MODE_DELETE
                  ? "y confirm delete  other cancel"
                  : (fb_mode == FB_MODE_NEW_FILE || fb_mode == FB_MODE_NEW_FOLDER || fb_mode == FB_MODE_RENAME)
                  ? "type name  Enter confirm  Backspace del  Esc close"
                  : "j/k move  Enter choose  l open  h collapse  a new file  A new folder  r rename  d delete  Esc close",
              px + 14, footer_y,
              t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
