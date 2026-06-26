#ifndef DE_APP_H
#define DE_APP_H

#include <stdbool.h>
#include "renderer.h"

typedef struct App App;

App  *app_create(int width, int height, const char *title);
void  app_destroy(App *app);
void  app_run(App *app);
void  app_quit(App *app);
void  app_open_file(App *app, const char *path);

int        app_get_width(App *app);
int        app_get_height(App *app);
double     app_get_dt(App *app);
void      *app_get_doc(App *app);
void      *app_get_mode(App *app);
Renderer  *app_get_renderer(App *app);

void app_set_clipboard(App *app, const char *text);
const char *app_get_clipboard(App *app);

/* Buffer management */
int  app_get_buffer_count(App *app);
int  app_get_current_buffer_index(App *app);
void app_switch_to_buffer(App *app, int index);
void app_next_buffer(App *app);
void app_prev_buffer(App *app);
bool app_close_buffer(App *app, int index);

#endif
