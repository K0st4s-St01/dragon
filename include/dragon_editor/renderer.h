#ifndef DE_RENDERER_H
#define DE_RENDERER_H

#include <glad/glad.h>

typedef struct {
    GLuint vao, vbo, shader;
    int    w, h;
} Renderer;

void renderer_init(Renderer *r, int w, int h);
void renderer_free(Renderer *r);
void renderer_clear(Renderer *r);
void renderer_resize(Renderer *r, int w, int h);
void renderer_draw_rect(Renderer *r, float x, float y, float w, float h,
                        float cr, float cg, float cb, float ca);
void renderer_flush(Renderer *r);

#endif
