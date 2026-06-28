#ifndef DE_THEME_H
#define DE_THEME_H

#include <stdbool.h>
#include "config.h"

typedef struct {
    float bg[4];
    float fg[4];
    float gutter_bg[4];
    float gutter_fg[4];
    float status_bg[4];
    float status_fg[4];
    float selection_bg[4];
    float cursor_color[4];
    float line_highlight[4];
    float menu_bg[4];
    float menu_fg[4];
    float menu_selected[4];
    float accent[4];
    float error[4];
    float warning[4];
    float keyword[4];
    float string[4];
    float number[4];
    float comment[4];
    float function_color[4];
    float type_color[4];       /* class, struct, enum, typedef */
    float variable_color[4];   /* variable, parameter */
    float macro_color[4];      /* macro, define */
    float operator_color[4];   /* operators */
    float namespace_color[4];  /* namespace, module */
} Theme;

Theme *theme_default(void);
Theme *theme_get(void);
const char *theme_current_name(void);
bool theme_get_named(const char *name, Theme *out);
bool theme_apply_named(const char *name);
int theme_list_names(const char **names, int max_names);

/* Apply theme colors from config */
void theme_apply_config(const Config *config);

#endif
