#pragma once
#include <stddef.h>
#include <stdint.h>

#include "egg.h"
#include "stb_truetype.h"

#define EGG_FIRST_CHAR     32
#define EGG_CHAR_COUNT     96
#define EGG_ATLAS_WIDTH    512
#define EGG_ATLAS_HEIGHT   512
#define EGG_BAKE_FONT_SIZE 32.0f

typedef struct {
        float x;
        float y;
        float c;
        float s;
} egg_instance_transform_t;

typedef struct {
        float   position[2];
        float   instanceIndex;
        float   uv[2];
        float   useTexture;
        uint8_t rgba[4];
} egg_vertex_t;

typedef struct {
        egg_vertex_t *data;
        int32_t       count;
        int32_t       capacity;
} egg_vertex_buffer_t;

typedef struct {
        egg_instance_transform_t *data;
        int32_t                   count;
        int32_t                   capacity;
} egg_transform_buffer_t;

typedef struct {
        egg_vertex_buffer_t    vertices;
        egg_transform_buffer_t transforms;
} egg_drawlist_t;

// CPU-side draw state: z-ordered lists of vertices/transforms.
typedef struct {
        egg_drawlist_t *lists;
        int32_t         listCount;
        int32_t         listCapacity;
        float           pixelScale;
} egg_draw_t;

typedef struct {
        stbtt_bakedchar glyphs[EGG_CHAR_COUNT];
        float           lineHeight;
} egg_font_t;

// Bakes the default system font into an EGG_ATLAS_WIDTH x EGG_ATLAS_HEIGHT single-channel bitmap.
int egg_font_bake(egg_font_t *font, unsigned char *bitmap);

void egg_dl_init(egg_draw_t *d);
void egg_dl_destroy(egg_draw_t *d);

void egg_dl_text(egg_draw_t *d, const egg_font_t *font, int32_t z, float x, float y, float rotationCos, float rotationSin, float fontSize, egg_color_t color, const char *string);
void egg_dl_line(egg_draw_t *d, int32_t z, float x1, float y1, float x2, float y2, float thickness, egg_color_t color);
void egg_dl_point(egg_draw_t *d, int32_t z, float x, float y, float size, egg_color_t color);
void egg_dl_circle(egg_draw_t *d, int32_t z, float x, float y, float radius, egg_color_t color);
void egg_dl_circle_outline(egg_draw_t *d, int32_t z, float x, float y, float radius, float thickness, egg_color_t color);
void egg_dl_capsule_outline(egg_draw_t *d, int32_t z, float x1, float y1, float x2, float y2, float radius, float thickness, egg_color_t color);
void egg_dl_transform(egg_draw_t *d, int32_t z, float x, float y, float rotationCos, float rotationSin, float scale, egg_color_t color);
void egg_dl_rectangle(egg_draw_t *d, int32_t z, float x, float y, float rotationCos, float rotationSin, float width, float height, egg_color_t color);
void egg_dl_rectangle_outline(egg_draw_t *d, int32_t z, float x, float y, float rotationCos, float rotationSin, float width, float height, float thickness, egg_color_t color);
void egg_dl_bounds(egg_draw_t *d, int32_t z, float minX, float minY, float maxX, float maxY, egg_color_t color);
void egg_dl_polygon(egg_draw_t *d, int32_t z, const egg_vec2_t *vertices, int vertex_count, float tx, float ty, float rot_c, float rot_s, egg_color_t color);
