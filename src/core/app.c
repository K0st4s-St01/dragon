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

static char *app_filepath_to_uri(const char *filepath) {
    if (!filepath) return NULL;
    char *uri = malloc(strlen(filepath) + 8);
    if (!uri) return NULL;
    snprintf(uri, strlen(filepath) + 8, "file://%s", filepath);
    return uri;
}

static Document *app_find_doc_by_uri(App *app, const char *uri) {
    if (!app || !uri || !*uri) return NULL;
    for (int i = 0; i < app->doc_count; i++) {
        Document *doc = &app->documents[i];
        char *doc_uri = app_filepath_to_uri(doc->filepath);
        bool matches = doc_uri && strcmp(doc_uri, uri) == 0;
        free(doc_uri);
        if (matches)
            return doc;
    }
    return NULL;
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

    if (target->diagnostics)
        lsp_free_diagnostics((LSPDiagnostics *)target->diagnostics);
    target->diagnostics = diagnostics;
}

static void app_update_diagnostics_from_lsp(App *app) {
    if (!app) return;

    for (int i = 0; i < app->lsp_manager.client_count; i++) {
        LSPClient *client = &app->lsp_manager.clients[i];
        if (client->status != LSP_STATUS_INITIALIZED)
            continue;

        for (int frame = 0; frame < 16; frame++) {
            char *response = lsp_client_read_response(client);
            if (!response)
                break;

            LSPDiagnostics *diagnostics = lsp_parse_publish_diagnostics_notification(response);
            if (diagnostics)
                app_store_diagnostics(app, diagnostics);
            free(response);
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
    document_open(&app->documents[app->current_doc], path);
    document_notify_lsp_open(&app->documents[app->current_doc], &app->lsp_manager);
}

/* Buffer management */
int app_get_buffer_count(App *app) {
    return app->doc_count;
}

int app_get_current_buffer_index(App *app) {
    return app->current_doc;
}

void app_switch_to_buffer(App *app, int index) {
    if (index >= 0 && index < app->doc_count) {
        app->current_doc = index;
    }
}

void app_next_buffer(App *app) {
    if (app->doc_count > 1) {
        app->current_doc = (app->current_doc + 1) % app->doc_count;
    }
}

void app_prev_buffer(App *app) {
    if (app->doc_count > 1) {
        app->current_doc = (app->current_doc - 1 + app->doc_count) % app->doc_count;
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
