#include "dragon_editor/app.h"
#include "dragon_editor/renderer.h"
#include "dragon_editor/mode.h"
#include "dragon_editor/document.h"
#include "dragon_editor/command.h"
#include "dragon_editor/input.h"
#include "dragon_editor/gui.h"
#include "dragon_editor/lsp.h"
#include "dragon_editor/lsp_config.h"
#include "dragon_editor/treesitter.h"
#include "dragon_editor/config.h"
#include "dragon_editor/theme.h"
#include "dragon_editor/panel_notification.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_BUFFERS 64

struct App {
    GLFWwindow *window;
    Renderer    renderer;
    Gui         gui;
    ModeState   mode;
    Document    documents[MAX_BUFFERS];
    int         doc_count;
    int         current_doc;
    double      dt;
    double      last_time;
    int         win_w, win_h;
    char       *clipboard;
    char       *workspace_root;
    LSPManager  lsp_manager;
    TreeSitterManager *ts_manager;
    int         syntax_update_timer;  /* Frame counter for throttled updates */
    Config     *config;
    WindowManager window_mgr;
};

static void framebuffer_cb(GLFWwindow *win, int w, int h) {
    App *app = (App *)glfwGetWindowUserPointer(win);
    app->win_w = w;
    app->win_h = h;
    renderer_resize(&app->renderer, w, h);
}

static void key_cb(GLFWwindow *win, int key, int scancode, int action, int mods) {
    App *app = (App *)glfwGetWindowUserPointer(win);
    input_handle_key(app, key, scancode, action, mods);
}

static void char_cb(GLFWwindow *win, unsigned int c) {
    App *app = (App *)glfwGetWindowUserPointer(win);
    input_handle_char(app, c);
}

static bool app_doc_is_empty_scratch(Document *doc) {
    return doc && !doc->filepath && !doc->dirty && doc->buffer.len == 0;
}

static void app_set_active_buffer(App *app, int index) {
    if (!app || index < 0 || index >= app->doc_count) return;
    Document *cur = &app->documents[app->current_doc];
    if (cur->filepath)
        document_set_alternate(cur, cur->filepath);
    app->current_doc = index;
    if (app->window_mgr.active >= 0 && app->window_mgr.active < app->window_mgr.count)
        app->window_mgr.windows[app->window_mgr.active].doc_index = index;
}

static void app_sync_buffer_from_active_window(App *app) {
    if (!app || app->window_mgr.active < 0 || app->window_mgr.active >= app->window_mgr.count)
        return;
    int doc_index = app->window_mgr.windows[app->window_mgr.active].doc_index;
    if (doc_index >= 0 && doc_index < app->doc_count)
        app->current_doc = doc_index;
}

/* Normalize a URI path for comparison: strip file:// prefix, remove . and .. */
static void normalize_uri_path(const char *uri, char *out, size_t out_size) {
    if (!uri || !out || out_size == 0) { if (out && out_size > 0) out[0] = '\0'; return; }
    
    /* Strip file:// prefix */
    const char *path = uri;
    if (strncmp(path, "file://", 7) == 0) path += 7;

    char decoded[2048];
    size_t decoded_len = 0;
    for (const char *p = path; *p && decoded_len < sizeof(decoded) - 1; p++) {
        if (*p == '%' && p[1] && p[2]) {
            int hi = (p[1] >= '0' && p[1] <= '9') ? p[1] - '0' :
                     (p[1] >= 'a' && p[1] <= 'f') ? p[1] - 'a' + 10 :
                     (p[1] >= 'A' && p[1] <= 'F') ? p[1] - 'A' + 10 : -1;
            int lo = (p[2] >= '0' && p[2] <= '9') ? p[2] - '0' :
                     (p[2] >= 'a' && p[2] <= 'f') ? p[2] - 'a' + 10 :
                     (p[2] >= 'A' && p[2] <= 'F') ? p[2] - 'A' + 10 : -1;
            if (hi >= 0 && lo >= 0) {
                decoded[decoded_len++] = (char)((hi << 4) | lo);
                p += 2;
                continue;
            }
        }
        decoded[decoded_len++] = *p;
    }
    decoded[decoded_len] = '\0';
    path = decoded;
    
    /* Simple normalization: copy char by char, handle . and .. */
    const char *seg_start = path;
    
    /* Skip leading slash, we'll add it back */
    if (*path == '/') { path++; seg_start = path; }
    
    /* Copy path and normalize segments */
    char buf[2048];
    size_t buf_len = 0;
    buf[0] = '/';
    buf_len = 1;
    
    const char *p = path;
    while (*p && buf_len < sizeof(buf) - 1) {
        if (*p == '/') {
            /* End of segment */
            size_t seg_len = (size_t)(p - seg_start);
            if (seg_len == 1 && seg_start[0] == '.') {
                /* Skip single dot */
            } else if (seg_len == 2 && seg_start[0] == '.' && seg_start[1] == '.') {
                /* Go up one level */
                if (buf_len > 1) {
                    buf_len--;  /* remove trailing slash */
                    while (buf_len > 1 && buf[buf_len - 1] != '/')
                        buf_len--;
                }
            } else {
                memcpy(buf + buf_len, seg_start, seg_len);
                buf_len += seg_len;
                buf[buf_len++] = '/';
            }
            seg_start = p + 1;
        }
        p++;
    }
    
    /* Handle last segment */
    size_t seg_len = (size_t)(p - seg_start);
    if (seg_len == 1 && seg_start[0] == '.') {
        /* Skip */
    } else if (seg_len == 2 && seg_start[0] == '.' && seg_start[1] == '.') {
        if (buf_len > 1) {
            buf_len--;
            while (buf_len > 1 && buf[buf_len - 1] != '/')
                buf_len--;
        }
    } else {
        memcpy(buf + buf_len, seg_start, seg_len);
        buf_len += seg_len;
    }
    
    /* Remove trailing slash if not root */
    if (buf_len > 1 && buf[buf_len - 1] == '/')
        buf_len--;
    
    buf[buf_len] = '\0';
    
    /* Copy to output, truncating if needed */
    size_t copy_len = buf_len < out_size - 1 ? buf_len : out_size - 1;
    memcpy(out, buf, copy_len);
    out[copy_len] = '\0';
}

static Document *app_find_doc_by_uri(App *app, const char *uri) {
    if (!app || !uri || !*uri) return NULL;
    
    char norm_uri[2048];
    normalize_uri_path(uri, norm_uri, sizeof(norm_uri));
    
    for (int i = 0; i < app->doc_count; i++) {
        Document *doc = &app->documents[i];
        if (!doc->filepath) continue;
        
        char norm_doc_path[2048];
        /* Normalize the doc filepath as if it were a URI path */
        char doc_uri_buf[2048];
        snprintf(doc_uri_buf, sizeof(doc_uri_buf), "file://%s", doc->filepath);
        normalize_uri_path(doc_uri_buf, norm_doc_path, sizeof(norm_doc_path));
        
        if (strcmp(norm_uri, norm_doc_path) == 0)
            return doc;
    }
    return NULL;
}

static void diagnostic_counts(LSPDiagnostics *diagnostics, int *errors, int *warnings, int *info) {
    if (errors) *errors = 0;
    if (warnings) *warnings = 0;
    if (info) *info = 0;
    if (!diagnostics) return;
    for (int i = 0; i < diagnostics->count; i++) {
        if (diagnostics->items[i].severity == LSP_DIAG_ERROR) {
            if (errors) (*errors)++;
        } else if (diagnostics->items[i].severity == LSP_DIAG_WARNING) {
            if (warnings) (*warnings)++;
        } else if (info) {
            (*info)++;
        }
    }
}

static void app_store_diagnostics(App *app, LSPDiagnostics *diagnostics) {
    if (!diagnostics) return;

    Document *target = app_find_doc_by_uri(app, diagnostics->uri);
    if (!target && (!diagnostics->uri || diagnostics->uri[0] == '\0'))
        target = &app->documents[app->current_doc];

    if (!target) {
        lsp_free_diagnostics(diagnostics);
        return;
    }

    int old_errors = 0, old_warnings = 0, old_info = 0;
    int new_errors = 0, new_warnings = 0, new_info = 0;
    diagnostic_counts((LSPDiagnostics *)target->diagnostics, &old_errors, &old_warnings, &old_info);
    diagnostic_counts(diagnostics, &new_errors, &new_warnings, &new_info);

    if (target->diagnostics)
        lsp_free_diagnostics((LSPDiagnostics *)target->diagnostics);
    target->diagnostics = diagnostics;

    if (old_errors != new_errors || old_warnings != new_warnings || old_info != new_info) {
        const char *name = target->filepath ? strrchr(target->filepath, '/') : NULL;
        name = name ? name + 1 : (target->filepath ? target->filepath : "[No Name]");
        if (new_errors || new_warnings || new_info) {
            notification_push(new_errors ? NOTIF_ERROR : (new_warnings ? NOTIF_WARNING : NOTIF_INFO),
                              "%s: %d errors, %d warnings, %d info",
                              name, new_errors, new_warnings, new_info);
        } else if (old_errors || old_warnings || old_info) {
            notification_push(NOTIF_SUCCESS, "%s: diagnostics cleared", name);
        }
    }
}

static void app_update_diagnostics_from_lsp(App *app) {
    if (!app) return;

    for (int i = 0; i < app->lsp_manager.client_count; i++) {
        LSPClient *client = &app->lsp_manager.clients[i];
        if (client->status != LSP_STATUS_INITIALIZED)
            continue;

        char *deferred[16] = {0};
        int deferred_count = 0;
        for (int frame = 0; frame < 16; frame++) {
            char *response = lsp_client_read_response(client);
            if (!response)
                break;

            LSPDiagnostics *diagnostics = lsp_parse_publish_diagnostics_notification(response);
            if (diagnostics) {
                app_store_diagnostics(app, diagnostics);
                free(response);
                continue;
            }
            if (deferred_count < (int)(sizeof(deferred) / sizeof(deferred[0]))) {
                deferred[deferred_count++] = response;
            } else {
                lsp_client_unread_response(client, response);
                free(response);
                break;
            }
        }

        for (int d = deferred_count - 1; d >= 0; d--) {
            lsp_client_unread_response(client, deferred[d]);
            free(deferred[d]);
        }
    }
}

int app_get_width(App *app)   { return app->win_w; }
int app_get_height(App *app)  { return app->win_h; }
double app_get_dt(App *app)   { return app->dt; }
void *app_get_doc(App *app)    { return &app->documents[app->current_doc]; }
void *app_get_doc_at(App *app, int index) {
    if (!app || index < 0 || index >= app->doc_count) return NULL;
    return &app->documents[index];
}
void *app_get_mode(App *app)  { return &app->mode; }
Renderer *app_get_renderer(App *app) { return &app->renderer; }
void *app_get_lsp_manager(App *app) { return &app->lsp_manager; }
void *app_get_treesitter_manager(App *app) { return app->ts_manager; }
Config *app_get_config(App *app) { return app->config; }

void app_set_clipboard(App *app, const char *text) {
    free(app->clipboard);
    app->clipboard = text ? strdup(text) : NULL;
}

const char *app_get_clipboard(App *app) {
    return app->clipboard;
}

App *app_create(int width, int height, const char *title) {
    App *app = calloc(1, sizeof(App));
    app->win_w = width;
    app->win_h = height;

    if (!glfwInit()) return NULL;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    app->window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!app->window) { glfwTerminate(); free(app); return NULL; }

    glfwMakeContextCurrent(app->window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(app->window); glfwTerminate(); free(app);
        return NULL;
    }
    printf("OpenGL %s\n", glGetString(GL_VERSION));

    glfwSetWindowUserPointer(app->window, app);
    glfwSetFramebufferSizeCallback(app->window, framebuffer_cb);
    glfwSetKeyCallback(app->window, key_cb);
    glfwSetCharCallback(app->window, char_cb);

    renderer_init(&app->renderer, width, height);
    gui_init(&app->gui);
    mode_init(&app->mode);
    
    /* Load configuration and apply theme */
    app->config = config_load();
    theme_apply_config(app->config);
    
    lsp_manager_init(&app->lsp_manager);
    lsp_config_load_defaults(&app->lsp_manager);
    app->ts_manager = treesitter_manager_new();
    window_manager_init(&app->window_mgr);
    
    /* Initialize workspace to current directory */
    app->workspace_root = getcwd(NULL, 0);
    app->lsp_manager.workspace_root = strdup(app->workspace_root);
    
    /* Initialize first buffer */
    app->doc_count = 1;
    app->current_doc = 0;
    document_init(&app->documents[0]);
    
    command_registry_init();

    app->last_time = glfwGetTime();
    app->syntax_update_timer = 0;
    return app;
}

void app_destroy(App *app) {
    if (!app) return;
    lsp_manager_free(&app->lsp_manager);
    if (app->ts_manager) treesitter_manager_free(app->ts_manager);
    gui_free(&app->gui);
    renderer_free(&app->renderer);
    for (int i = 0; i < app->doc_count; i++) {
        document_free(&app->documents[i]);
    }
    glfwDestroyWindow(app->window);
    glfwTerminate();
    free(app->clipboard);
    free(app->workspace_root);
    config_free(app->config);
    free(app);
}

void app_run(App *app) {
    const double target_frame_time = 1.0 / 60.0;  /* 60 FPS */
    
    while (!glfwWindowShouldClose(app->window)) {
        double frame_start = glfwGetTime();
        double now = frame_start;
        app->dt = now - app->last_time;
        app->last_time = now;
        notification_update(app->dt);

        glfwPollEvents();
        
        Document *doc = &app->documents[app->current_doc];

        /* Produce visible syntax highlighting locally first; LSP semantic tokens are a refinement. */
        if (doc && doc->syntax_dirty && !doc->ts_attempted) {
            bool parsed = app->ts_manager ? document_parse_treesitter(doc, app->ts_manager) : false;
            doc->ts_attempted = true;
            if (!parsed)
                parsed = document_update_syntax_fallback(doc);
            doc->ts_parsed = parsed;
            if (parsed)
                doc->syntax_dirty = false;

        }

        if (doc && doc->lsp_dirty && doc->filepath && doc->language_id) {
            document_notify_lsp_change(doc, &app->lsp_manager);
            doc->lsp_dirty = false;
        }

        /* Throttled LSP semantic-token update for languages without local highlighting. */
        app->syntax_update_timer++;
        if (app->syntax_update_timer >= 30 && doc && doc->language_id && doc->syntax_dirty && !doc->ts_parsed) {
            app->syntax_update_timer = 0;
            document_update_syntax_from_lsp(doc, &app->lsp_manager);
            if (doc->syntax.token_count > 0)
                doc->syntax_dirty = false;
        }
        
        /* Check for LSP diagnostics notifications (non-blocking) */
        app_update_diagnostics_from_lsp(app);

        renderer_clear(&app->renderer);
        gui_begin(&app->gui);
        gui_render(&app->gui, app, &app->documents[app->current_doc], &app->mode);
        gui_end(&app->gui);

        glfwSwapBuffers(app->window);
        
        /* Frame rate limiter - maintain 60 FPS */
        double frame_end = glfwGetTime();
        double frame_elapsed = frame_end - frame_start;
        double sleep_time = target_frame_time - frame_elapsed;
        
        if (sleep_time > 0.0) {
            /* Sleep for remaining frame time in microseconds */
            usleep((unsigned int)(sleep_time * 1000000.0));
        }
    }
}

void app_quit(App *app) {
    glfwSetWindowShouldClose(app->window, GLFW_TRUE);
}

void app_open_file(App *app, const char *path) {
    if (!app || !path) return;

    for (int i = 0; i < app->doc_count; i++) {
        if (app->documents[i].filepath && strcmp(app->documents[i].filepath, path) == 0) {
            app_set_active_buffer(app, i);
            return;
        }
    }

    int target = app->current_doc;
    if (!app_doc_is_empty_scratch(&app->documents[target])) {
        if (app->doc_count >= MAX_BUFFERS)
            return;
        target = app->doc_count++;
        document_init(&app->documents[target]);
    }

    document_open(&app->documents[target], path);
    if (app->documents[target].filepath) {
        app_set_active_buffer(app, target);
        document_notify_lsp_open(&app->documents[target], &app->lsp_manager);
    } else if (target == app->doc_count - 1 && target != app->current_doc) {
        document_free(&app->documents[target]);
        app->doc_count--;
    }
}

/* Buffer management */
int app_get_buffer_count(App *app) {
    return app->doc_count;
}

int app_get_current_buffer_index(App *app) {
    return app->current_doc;
}

void app_switch_to_buffer(App *app, int index) {
    app_set_active_buffer(app, index);
}

void app_next_buffer(App *app) {
    if (app->doc_count > 1) {
        app_set_active_buffer(app, (app->current_doc + 1) % app->doc_count);
    }
}

void app_prev_buffer(App *app) {
    if (app->doc_count > 1) {
        app_set_active_buffer(app, (app->current_doc - 1 + app->doc_count) % app->doc_count);
    }
}

bool app_close_buffer(App *app, int index) {
    if (index < 0 || index >= app->doc_count) return false;
    if (app->doc_count == 1) return false; /* Can't close last buffer */
    
    /* Free the document */
    document_free(&app->documents[index]);
    
    /* Shift remaining documents */
    for (int i = index; i < app->doc_count - 1; i++) {
        app->documents[i] = app->documents[i + 1];
    }
    app->doc_count--;
    
    /* Adjust current_doc if needed */
    if (app->current_doc >= app->doc_count) {
        app->current_doc = app->doc_count - 1;
    } else if (app->current_doc > index) {
        app->current_doc--;
    }

    for (int i = 0; i < app->window_mgr.count; i++) {
        if (app->window_mgr.windows[i].doc_index == index)
            app->window_mgr.windows[i].doc_index = app->current_doc;
        else if (app->window_mgr.windows[i].doc_index > index)
            app->window_mgr.windows[i].doc_index--;
        if (app->window_mgr.windows[i].doc_index >= app->doc_count)
            app->window_mgr.windows[i].doc_index = app->current_doc;
    }
    
    return true;
}

/* Workspace management */
const char *app_get_workspace_root(App *app) {
    return app->workspace_root ? app->workspace_root : ".";
}

void app_set_workspace_root(App *app, const char *path) {
    free(app->workspace_root);
    app->workspace_root = strdup(path);
    free(app->lsp_manager.workspace_root);
    app->lsp_manager.workspace_root = strdup(path);
}

/* Window/split management */
WindowManager *app_get_window_manager(App *app) {
    return &app->window_mgr;
}

void app_split_vertical(App *app) {
    if (window_split_vertical(&app->window_mgr, app->current_doc) >= 0)
        app_sync_buffer_from_active_window(app);
}

void app_split_horizontal(App *app) {
    if (window_split_horizontal(&app->window_mgr, app->current_doc) >= 0)
        app_sync_buffer_from_active_window(app);
}

void app_close_split(App *app) {
    window_close(&app->window_mgr);
    app_sync_buffer_from_active_window(app);
}

void app_next_window(App *app) {
    window_next(&app->window_mgr);
    app_sync_buffer_from_active_window(app);
}

void app_prev_window(App *app) {
    window_prev(&app->window_mgr);
    app_sync_buffer_from_active_window(app);
}

void app_goto_window_left(App *app) {
    window_goto_left(&app->window_mgr);
    app_sync_buffer_from_active_window(app);
}

void app_goto_window_right(App *app) {
    window_goto_right(&app->window_mgr);
    app_sync_buffer_from_active_window(app);
}

void app_goto_window_up(App *app) {
    window_goto_up(&app->window_mgr);
    app_sync_buffer_from_active_window(app);
}

void app_goto_window_down(App *app) {
    window_goto_down(&app->window_mgr);
    app_sync_buffer_from_active_window(app);
}

void app_maximize_window(App *app) {
    window_maximize(&app->window_mgr);
}

void app_equalize_windows(App *app) {
    window_equalize(&app->window_mgr);
}
