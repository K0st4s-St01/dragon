#ifndef DE_TEXT_H
#define DE_TEXT_H

#include "renderer.h"

typedef struct {
    GLuint texture;
    int    w, h;
    float  glyph_w;
    float  glyph_h;
    float  ascent;
    float  advance;
} Font;

void font_init(Font *f, const char *path, float size);
void font_free(Font *f);
void font_draw(Font *f, Renderer *r, const char *text, float x, float y,
               float cr, float cg, float cb, float ca);
float font_text_width(Font *f, const char *text);
int   font_atlas_init(Font *f, const char *path, float size);
int   font_atlas_init_default(Font *f, float size);

#endif
