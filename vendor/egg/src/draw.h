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

typedef struct {
        stbtt_bakedchar glyphs[EGG_CHAR_COUNT];
        float           lineHeight;
} egg_font_t;

// CPU-side draw state: z-ordered lists of vertices/transforms.
struct egg_draw_t {
        egg_drawlist_t *lists;
        int32_t         listCount;
        int32_t         listCapacity;
        float           pixelScale;
        egg_font_t      font;
};

// Bakes the default system font into an EGG_ATLAS_WIDTH x EGG_ATLAS_HEIGHT single-channel bitmap.
int egg_font_bake(egg_font_t *font, unsigned char *bitmap);
