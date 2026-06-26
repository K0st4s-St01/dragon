#ifndef DE_INPUT_H
#define DE_INPUT_H

#include "app.h"
#include "mode.h"

/* Input handling - processes GLFW key events into editor actions */
void input_handle_key(App *app, int key, int scancode, int action, int mods);
void input_handle_char(App *app, unsigned int c);

#endif
