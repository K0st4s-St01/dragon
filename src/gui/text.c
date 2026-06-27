#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "dragon_editor/text.h"
#include "dragon_editor/renderer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define ATLAS_W 1024
#define ATLAS_H 1024
#define MAX_GLYPHS 2048

static unsigned char atlas_bitmap[ATLAS_W * ATLAS_H];
static GLuint atlas_texture = 0;

typedef struct {
    int codepoint;
    float u0, v0, u1, v1;  /* UV in atlas */
    float pix_w, pix_h;    /* glyph bitmap size in pixels */
    float xoff, yoff;      /* offset from baseline to top-left */
    float xadvance;
    bool used;
} GlyphInfo;

static GlyphInfo glyphs[MAX_GLYPHS];
static int glyph_count = 0;
static stbtt_fontinfo font_info;
static float font_scale;
static bool font_loaded = false;

static int atlas_x = 0, atlas_y = 0, row_h = 0;

static GlyphInfo *find_glyph(int codepoint) {
    for (int i = 0; i < glyph_count; i++)
        if (glyphs[i].codepoint == codepoint && glyphs[i].used)
            return &glyphs[i];
    return NULL;
}

static GlyphInfo *rasterize_glyph(int codepoint) {
    if (!font_loaded) return NULL;
    if (glyph_count >= MAX_GLYPHS) return NULL;

    GlyphInfo *g = &glyphs[glyph_count];

    int advance_width, left_side_bearing;
    stbtt_GetCodepointHMetrics(&font_info, codepoint, &advance_width, &left_side_bearing);

    int w, h, xoff, yoff;
    unsigned char *bitmap = stbtt_GetCodepointBitmap(&font_info, 0, font_scale,
                                                      codepoint, &w, &h, &xoff, &yoff);

    /* Pack into atlas */
    if (w > 0 && h > 0) {
        if (atlas_x + w + 1 >= ATLAS_W) {
            atlas_x = 0;
            atlas_y += row_h + 1;
            row_h = 0;
        }
        if (atlas_y + h + 1 >= ATLAS_H) {
            stbtt_FreeBitmap(bitmap, NULL);
            return NULL;
        }

        for (int row = 0; row < h; row++)
            memcpy(&atlas_bitmap[(atlas_y + row) * ATLAS_W + atlas_x],
                   &bitmap[row * w], (size_t)w);

        g->u0 = (float)atlas_x / ATLAS_W;
        g->v0 = (float)atlas_y / ATLAS_H;
        g->u1 = (float)(atlas_x + w) / ATLAS_W;
        g->v1 = (float)(atlas_y + h) / ATLAS_H;

        if (h > row_h) row_h = h;
        atlas_x += w + 1;
    } else {
        g->u0 = g->v0 = g->u1 = g->v1 = 0;
    }

    stbtt_FreeBitmap(bitmap, NULL);

    g->codepoint = codepoint;
    g->pix_w = (float)w;
    g->pix_h = (float)h;
    g->xoff = (float)xoff;
    g->yoff = (float)yoff;
    g->xadvance = advance_width * font_scale;
    g->used = true;
    glyph_count++;

    /* Re-upload atlas */
    if (atlas_texture && w > 0 && h > 0) {
        glBindTexture(GL_TEXTURE_2D, atlas_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ATLAS_W, ATLAS_H,
                        GL_RED, GL_UNSIGNED_BYTE, atlas_bitmap);
    }

    return g;
}

static int utf8_decode(const char **p) {
    const unsigned char *s = (const unsigned char *)*p;
    int c = *s++;
    if (c < 0x80) {
        *p = (const char *)s;
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        if (!s[0] || (s[0] & 0xC0) != 0x80) {
            *p = (const char *)s;
            return 0xFFFD;
        }
        c = (c & 0x1F) << 6;
        c |= (*s++ & 0x3F);
        *p = (const char *)s;
        return c;
    } else if ((c & 0xF0) == 0xE0) {
        if (!s[0] || !s[1] || (s[0] & 0xC0) != 0x80 || (s[1] & 0xC0) != 0x80) {
            *p = (const char *)s;
            return 0xFFFD;
        }
        c = (c & 0x0F) << 12;
        c |= (*s++ & 0x3F) << 6;
        c |= (*s++ & 0x3F);
        *p = (const char *)s;
        return c;
    } else if ((c & 0xF8) == 0xF0) {
        if (!s[0] || !s[1] || !s[2] ||
            (s[0] & 0xC0) != 0x80 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) {
            *p = (const char *)s;
            return 0xFFFD;
        }
        c = (c & 0x07) << 18;
        c |= (*s++ & 0x3F) << 12;
        c |= (*s++ & 0x3F) << 6;
        c |= (*s++ & 0x3F);
        *p = (const char *)s;
        return c;
    }
    *p = (const char *)s;
    return 0xFFFD;
}

int font_atlas_init(Font *f, const char *path, float size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *ttf = malloc((size_t)sz);
    fread(ttf, 1, (size_t)sz, fp);
    fclose(fp);

    if (!stbtt_InitFont(&font_info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        free(ttf);
        return -1;
    }

    font_scale = stbtt_ScaleForPixelHeight(&font_info, size);
    font_loaded = true;

    memset(atlas_bitmap, 0, sizeof(atlas_bitmap));
    memset(glyphs, 0, sizeof(glyphs));
    glyph_count = 0;
    atlas_x = 0;
    atlas_y = 0;
    row_h = 0;

    /* Pre-rasterize ASCII */
    for (int c = 32; c < 128; c++)
        rasterize_glyph(c);

    /* Upload atlas */
    glGenTextures(1, &atlas_texture);
    glBindTexture(GL_TEXTURE_2D, atlas_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_W, ATLAS_H, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Get font metrics */
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
    f->glyph_h = (ascent - descent) * font_scale;
    f->ascent = ascent * font_scale;

    int advance_width, left_side_bearing;
    stbtt_GetCodepointHMetrics(&font_info, 'M', &advance_width, &left_side_bearing);
    f->glyph_w = advance_width * font_scale;
    f->advance = f->glyph_w;
    f->texture = atlas_texture;
    f->w = ATLAS_W;
    f->h = ATLAS_H;

    return 0;
}

int font_atlas_init_default(Font *f, float size) {
    const char *fonts[] = {
        "/usr/share/fonts/TTF/JetBrainsMonoNerdFont-Regular.ttf",
        "/usr/share/fonts/TTF/HackNerdFont-Regular.ttf",
        "/usr/share/fonts/TTF/FiraCodeNerdFont-Regular.ttf",
        "/usr/share/fonts/TTF/AgaveNerdFontMono-Regular.ttf",
        "/usr/share/fonts/TTF/0xProtoNerdFontMono-Regular.ttf",
        "/usr/share/fonts/noto/NotoSansMono-Regular.ttf",
        "/usr/share/fonts/TTF/Hack-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/truetype/jetbrainsmono/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/TTF/FiraCode-Regular.ttf",
        "/usr/share/fonts/truetype/firacode/FiraCode-Regular.ttf",
        "/usr/share/fonts/truetype/hack/Hack-Regular.ttf",
        NULL,
    };
    for (int i = 0; fonts[i]; i++)
        if (font_atlas_init(f, fonts[i], size) == 0) {
            fprintf(stderr, "Loaded font: %s\n", fonts[i]);
            return 0;
        }
    fprintf(stderr, "No TTF font found.\n");
    return -1;
}

void font_init(Font *f, const char *path, float size) {
    memset(f, 0, sizeof(Font));
    if (path)
        font_atlas_init(f, path, size);
    else
        font_atlas_init_default(f, size);
}

void font_free(Font *f) {
    if (atlas_texture) glDeleteTextures(1, &atlas_texture);
    atlas_texture = 0;
    f->texture = 0;
}

void font_draw(Font *f, Renderer *r, const char *text, float x, float y,
               float cr, float cg, float cb, float ca) {
    if (!text || !f->texture) return;

    int max_quads = 0;
    for (const char *p = text; *p; ) { utf8_decode(&p); max_quads++; }

    float *verts = malloc(sizeof(float) * max_quads * 24);
    int vcount = 0;
    float cx = x;
    float baseline = y + f->ascent;

    const char *p = text;
    while (*p) {
        int cp = utf8_decode(&p);

        GlyphInfo *g = find_glyph(cp);
        if (!g) g = rasterize_glyph(cp);
        if (!g) continue;

        if (g->pix_w > 0 && g->pix_h > 0) {
            float x0 = cx + g->xoff;
            float y0 = baseline + g->yoff;
            float x1 = x0 + g->pix_w;
            float y1 = y0 + g->pix_h;

            int i = vcount;
            verts[i+0]=x0;  verts[i+1]=y0;  verts[i+2]=g->u0; verts[i+3]=g->v0;
            verts[i+4]=x1;  verts[i+5]=y0;  verts[i+6]=g->u1; verts[i+7]=g->v0;
            verts[i+8]=x1;  verts[i+9]=y1;  verts[i+10]=g->u1;verts[i+11]=g->v1;
            verts[i+12]=x0; verts[i+13]=y0; verts[i+14]=g->u0;verts[i+15]=g->v0;
            verts[i+16]=x1; verts[i+17]=y1; verts[i+18]=g->u1;verts[i+19]=g->v1;
            verts[i+20]=x0; verts[i+21]=y1; verts[i+22]=g->u0;verts[i+23]=g->v1;
            vcount += 24;
        }

        cx += g->xadvance;
    }

    if (vcount > 0) {
        glUseProgram(r->shader);
        float l = 2.0f / r->w;
        float b = 2.0f / r->h;
        float proj[16] = {l,0,0,0, 0,-b,0,0, 0,0,-1,0, -1,1,0,1};
        glUniformMatrix4fv(glGetUniformLocation(r->shader, "proj"), 1, GL_FALSE, proj);
        glUniform4f(glGetUniformLocation(r->shader, "color"), cr, cg, cb, ca);
        glUniform1i(glGetUniformLocation(r->shader, "useTex"), 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, f->texture);
        glUniform1i(glGetUniformLocation(r->shader, "tex"), 0);

        glBindVertexArray(r->vao);
        glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
        glBufferData(GL_ARRAY_BUFFER, vcount * sizeof(float), verts, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
        glEnableVertexAttribArray(1);
        glDrawArrays(GL_TRIANGLES, 0, vcount / 4);
        glBindVertexArray(0);

        glUniform1i(glGetUniformLocation(r->shader, "useTex"), 0);
    }

    free(verts);
}

float font_text_width(Font *f, const char *text) {
    (void)f;
    if (!text || !font_loaded) return 0;
    float w = 0;
    const char *p = text;
    while (*p) {
        int cp = utf8_decode(&p);
        GlyphInfo *g = find_glyph(cp);
        if (!g) g = rasterize_glyph(cp);
        if (g) w += g->xadvance;
    }
    return w;
}
