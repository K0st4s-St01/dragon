#ifndef DE_INPUT_H
#define DE_INPUT_H

#include "app.h"
#include "mode.h"

/* Input handling - processes GLFW key events into editor actions */
void input_handle_key(App *app, int key, int scancode, int action, int mods);
void input_handle_char(App *app, unsigned int c);
const char *input_cmd_get(void);
int input_cmd_completion_count(void);
int input_cmd_completion_selected(void);
const char *input_cmd_completion_name(int index);
const char *input_cmd_completion_detail(int index);

#endif
