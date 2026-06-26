#include "dragon_editor/renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>

static const char *vert_src =
    "#version 330 core\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aTexCoord;\n"
    "uniform mat4 proj;\n"
    "out vec2 TexCoord;\n"
    "void main(){\n"
    "  gl_Position = proj * vec4(aPos, 0.0, 1.0);\n"
    "  TexCoord = aTexCoord;\n"
    "}\n";

static const char *frag_src =
    "#version 330 core\n"
    "in vec2 TexCoord;\n"
    "uniform vec4 color;\n"
    "uniform sampler2D tex;\n"
    "uniform int useTex;\n"
    "out vec4 FragColor;\n"
    "void main(){\n"
    "  if (useTex == 1)\n"
    "    FragColor = vec4(color.rgb, color.a * texture(tex, TexCoord).r);\n"
    "  else\n"
    "    FragColor = color;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader: %s\n", log);
    }
    return s;
}

static GLuint create_program(const char *vs, const char *fs) {
    GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

void renderer_init(Renderer *r, int w, int h) {
    r->w = w;
    r->h = h;
    r->shader = create_program(vert_src, frag_src);

    glGenVertexArrays(1, &r->vao);
    glGenBuffers(1, &r->vbo);
    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    /* pos (2) + texcoord (2) = 4 floats per vertex */
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void renderer_free(Renderer *r) {
    glDeleteVertexArrays(1, &r->vao);
    glDeleteBuffers(1, &r->vbo);
    glDeleteProgram(r->shader);
}

void renderer_clear(Renderer *r) {
    (void)r;
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_resize(Renderer *r, int w, int h) {
    r->w = w;
    r->h = h;
    glViewport(0, 0, w, h);
}

void renderer_draw_rect(Renderer *r, float x, float y, float w, float h,
                        float cr, float cg, float cb, float ca) {
    glUseProgram(r->shader);
    float l = 2.0f / r->w;
    float b = 2.0f / r->h;
    float proj[16] = {l,0,0,0, 0,-b,0,0, 0,0,-1,0, -1,1,0,1};
    glUniformMatrix4fv(glGetUniformLocation(r->shader, "proj"), 1, GL_FALSE, proj);
    glUniform4f(glGetUniformLocation(r->shader, "color"), cr, cg, cb, ca);
    glUniform1i(glGetUniformLocation(r->shader, "useTex"), 0);

    float quad[] = {
        x,   y,   0, 0,
        x+w, y,   1, 0,
        x+w, y+h, 1, 1,
        x,   y,   0, 0,
        x+w, y+h, 1, 1,
        x,   y+h, 0, 1,
    };

    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void renderer_flush(Renderer *r) {
    (void)r;
    glFlush();
}
