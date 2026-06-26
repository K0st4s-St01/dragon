#include "dragon_editor/app.h"
#include "dragon_editor/renderer.h"
#include "dragon_editor/mode.h"
#include "dragon_editor/document.h"
#include "dragon_editor/command.h"
#include "dragon_editor/input.h"
#include "dragon_editor/gui.h"

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

int app_get_width(App *app)   { return app->win_w; }
int app_get_height(App *app)  { return app->win_h; }
double app_get_dt(App *app)   { return app->dt; }
void *app_get_doc(App *app)    { return &app->documents[app->current_doc]; }
void *app_get_mode(App *app)  { return &app->mode; }
Renderer *app_get_renderer(App *app) { return &app->renderer; }

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
    
    /* Initialize first buffer */
    app->doc_count = 1;
    app->current_doc = 0;
    document_init(&app->documents[0]);
    
    command_registry_init();

    /* Initialize workspace to current directory */
    app->workspace_root = getcwd(NULL, 0);

    app->last_time = glfwGetTime();
    return app;
}

void app_destroy(App *app) {
    if (!app) return;
    gui_free(&app->gui);
    renderer_free(&app->renderer);
    for (int i = 0; i < app->doc_count; i++) {
        document_free(&app->documents[i]);
    }
    glfwDestroyWindow(app->window);
    glfwTerminate();
    free(app->clipboard);
    free(app->workspace_root);
    free(app);
}

void app_run(App *app) {
    while (!glfwWindowShouldClose(app->window)) {
        double now = glfwGetTime();
        app->dt = now - app->last_time;
        app->last_time = now;

        glfwPollEvents();

        renderer_clear(&app->renderer);
        gui_begin(&app->gui);
        gui_render(&app->gui, app, &app->documents[app->current_doc], &app->mode);
        gui_end(&app->gui);

        glfwSwapBuffers(app->window);
    }
}

void app_quit(App *app) {
    glfwSetWindowShouldClose(app->window, GLFW_TRUE);
}

void app_open_file(App *app, const char *path) {
    document_open(&app->documents[app->current_doc], path);
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
}
