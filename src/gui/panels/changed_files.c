#include "panel_changed_files.h"
#include "app.h"
#include "theme.h"
#include "gui.h"
#include "renderer.h"

#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CF_MAX 256

typedef struct {
    char status[4];
    char path[512];
    char full_path[1024];
} ChangedFile;

static bool cf_open = false;
static int cf_selected = 0;
static int cf_scroll = 0;
static ChangedFile cf_items[CF_MAX];
static int cf_count = 0;
static bool cf_git_found = true;

static const char *basename_label(const char *path) {
    const char *slash = path ? strrchr(path, '/') : NULL;
    return slash && slash[1] ? slash + 1 : (path && *path ? path : "[No Name]");
}

static bool is_deleted_status(const char *status) {
    return status && (status[0] == 'D' || status[1] == 'D');
}

static void cf_draw_fit(Gui *g, Renderer *r, const char *text,
                        float x, float right, float y,
                        float cr, float cg, float cb, float ca) {
    if (!text || !*text || right <= x) return;
    char clipped[512];
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

static void add_changed_file(const char *root, const char *status, const char *path) {
    if (cf_count >= CF_MAX || !root || !path || !*path) return;
    ChangedFile *item = &cf_items[cf_count++];
    snprintf(item->status, sizeof(item->status), "%s", status && *status ? status : "??");
    snprintf(item->path, sizeof(item->path), "%s", path);
    if (path[0] == '/')
        snprintf(item->full_path, sizeof(item->full_path), "%s", path);
    else
        snprintf(item->full_path, sizeof(item->full_path), "%s/%s", root, path);
}

static void parse_git_status_line(const char *root, char *line) {
    if (!line || strlen(line) < 4) return;
    char status[4] = {line[0], line[1], '\0', '\0'};
    char *path = line + 3;

    char *arrow = strstr(path, " -> ");
    if (arrow) path = arrow + 4;

    size_t len = strlen(path);
    if (len >= 2 && path[0] == '"' && path[len - 1] == '"') {
        path[len - 1] = '\0';
        path++;
    }
    add_changed_file(root, status, path);
}

static void collect_changed_files(App *app) {
    cf_count = 0;
    cf_selected = 0;
    cf_scroll = 0;
    cf_git_found = true;

    const char *root = app_get_workspace_root(app);
    if (!root || !*root) root = ".";

    int pipefd[2];
    if (pipe(pipefd) != 0) {
        cf_git_found = false;
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execlp("git", "git", "-C", root, "status", "--porcelain", "--untracked-files=all", (char *)NULL);
        _exit(127);
    }

    close(pipefd[1]);
    if (pid < 0) {
        close(pipefd[0]);
        cf_git_found = false;
        return;
    }

    FILE *stream = fdopen(pipefd[0], "r");
    if (!stream) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        cf_git_found = false;
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), stream) && cf_count < CF_MAX) {
        line[strcspn(line, "\r\n")] = '\0';
        parse_git_status_line(root, line);
    }
    fclose(stream);

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        cf_git_found = false;
}

void panel_changed_files_open(App *app) {
    cf_open = true;
    collect_changed_files(app);
}

void panel_changed_files_close(App *app) {
    (void)app;
    cf_open = false;
}

bool panel_changed_files_is_open(void) {
    return cf_open;
}

void panel_changed_files_key(App *app, int key) {
    if (!cf_open) return;
    switch (key) {
    case GLFW_KEY_DOWN:
    case GLFW_KEY_J:
        if (cf_selected < cf_count - 1) cf_selected++;
        break;
    case GLFW_KEY_UP:
    case GLFW_KEY_K:
        if (cf_selected > 0) cf_selected--;
        break;
    case GLFW_KEY_PAGE_DOWN:
        cf_selected += 10;
        if (cf_selected >= cf_count) cf_selected = cf_count > 0 ? cf_count - 1 : 0;
        break;
    case GLFW_KEY_PAGE_UP:
        cf_selected -= 10;
        if (cf_selected < 0) cf_selected = 0;
        break;
    case GLFW_KEY_HOME:
        cf_selected = 0;
        break;
    case GLFW_KEY_END:
        cf_selected = cf_count > 0 ? cf_count - 1 : 0;
        break;
    case GLFW_KEY_R:
        collect_changed_files(app);
        break;
    case GLFW_KEY_ENTER:
        if (cf_selected >= 0 && cf_selected < cf_count &&
            !is_deleted_status(cf_items[cf_selected].status)) {
            app_open_file(app, cf_items[cf_selected].full_path);
            panel_changed_files_close(app);
        }
        break;
    case GLFW_KEY_ESCAPE:
        panel_changed_files_close(app);
        break;
    default:
        break;
    }
}

void panel_changed_files_render(Gui *g, App *app) {
    if (!cf_open) return;
    Renderer *r = app_get_renderer(app);
    Theme *t = theme_get();
    int w = app_get_width(app);
    int h = app_get_height(app);
    float pw = (float)w * 0.62f;
    if (pw < 560.0f) pw = 560.0f;
    if (pw > 860.0f) pw = 860.0f;
    if (pw > (float)w - 48.0f) pw = (float)w - 48.0f;
    float ph = (float)h * 0.62f;
    if (ph < 340.0f) ph = 340.0f;
    if (ph > (float)h - 80.0f) ph = (float)h - 80.0f;
    float px = (float)w / 2.0f - pw / 2.0f;
    float py = (float)h / 2.0f - ph / 2.0f;
    float row_h = g->font.glyph_h + 7.0f;
    int visible = (int)((ph - 86.0f) / row_h);
    if (visible < 1) visible = 1;

    if (cf_selected < cf_scroll) cf_scroll = cf_selected;
    if (cf_selected >= cf_scroll + visible) cf_scroll = cf_selected - visible + 1;
    if (cf_scroll < 0) cf_scroll = 0;

    renderer_draw_rect(r, 0, 0, (float)w, (float)h, 0, 0, 0, 0.50f);
    renderer_draw_rect(r, px, py, pw, ph,
                       t->menu_bg[0], t->menu_bg[1], t->menu_bg[2], t->menu_bg[3]);
    renderer_draw_rect(r, px, py, pw, 2.0f, t->accent[0], t->accent[1], t->accent[2], 1.0f);
    renderer_draw_rect(r, px, py + 34.0f, pw, 1.0f, t->accent[0], t->accent[1], t->accent[2], 0.25f);

    char title[128];
    snprintf(title, sizeof(title), "Changed Files (%d)", cf_count);
    font_draw(&g->font, r, title, px + 14.0f, py + 10.0f,
              t->accent[0], t->accent[1], t->accent[2], 1.0f);

    float list_y = py + 48.0f;
    if (!cf_git_found) {
        font_draw(&g->font, r, "git status failed for this workspace", px + 18.0f, list_y,
                  t->warning[0], t->warning[1], t->warning[2], 1.0f);
    } else if (cf_count == 0) {
        font_draw(&g->font, r, "No changed files", px + 18.0f, list_y,
                  t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
    }

    for (int i = cf_scroll; i < cf_count && (i - cf_scroll) < visible; i++) {
        float y = list_y + (float)(i - cf_scroll) * row_h;
        bool selected = i == cf_selected;
        if (selected) {
            renderer_draw_rect(r, px + 6.0f, y - 2.0f, pw - 12.0f, row_h,
                               t->menu_selected[0], t->menu_selected[1],
                               t->menu_selected[2], t->menu_selected[3]);
        }

        bool deleted = is_deleted_status(cf_items[i].status);
        float sr = deleted ? t->error[0] : (cf_items[i].status[0] == '?' ? t->warning[0] : t->accent[0]);
        float sg = deleted ? t->error[1] : (cf_items[i].status[0] == '?' ? t->warning[1] : t->accent[1]);
        float sb = deleted ? t->error[2] : (cf_items[i].status[0] == '?' ? t->warning[2] : t->accent[2]);
        font_draw(&g->font, r, cf_items[i].status, px + 18.0f, y, sr, sg, sb, 1.0f);
        float path_x = px + pw * 0.38f;
        cf_draw_fit(g, r, basename_label(cf_items[i].path), px + 58.0f, path_x - 16.0f, y,
                    t->menu_fg[0], t->menu_fg[1], t->menu_fg[2], 1.0f);
        cf_draw_fit(g, r, cf_items[i].path, path_x, px + pw - 18.0f, y,
                    t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], 1.0f);
    }

    const char *help = "Enter open  r refresh  j/k move  Esc close";
    renderer_draw_rect(r, px, py + ph - 29.0f, pw, 1.0f,
                       t->accent[0], t->accent[1], t->accent[2], 0.25f);
    cf_draw_fit(g, r, help, px + 14.0f, px + pw - 14.0f, py + ph - 24.0f,
                t->gutter_fg[0], t->gutter_fg[1], t->gutter_fg[2], t->gutter_fg[3]);
}
