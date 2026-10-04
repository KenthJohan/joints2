#pragma once
#include <stdint.h>
#include <EgShapedraw.h>

typedef uint32_t            egg_color_t;
typedef struct egg_render_t egg_render_t;
typedef struct egg_draw_t   egg_draw_t;

typedef struct {
	float x;
	float y;
} egg_vec2_t;

// GL renderer: owns shaders, buffers and the glyph atlas. Needs a current GL context.
egg_render_t *egg_render_init(void);

void egg_render_destroy(egg_render_t *render);

// Draws and clears everything recorded in `draw`.
void egg_flush(egg_render_t *render, egg_draw_t *draw, const float *projectionMatrix);

// CPU-side draw recorder. Does not need a GL context.
egg_draw_t *egg_draw_create(void);

void egg_draw_destroy(egg_draw_t *draw);

void egg_draw_set_pixel_scale(egg_draw_t *draw, float pixelScale);

// Every draw call takes a z index: higher z is drawn later (on top); negative values clamp to 0.

void egg_draw_text(egg_draw_t *draw, int32_t z, float x, float y, float rotationCos, float rotationSin, float fontSize, egg_color_t color, const char *string);

void egg_draw_line(egg_draw_t *draw, int32_t z, float x1, float y1, float x2, float y2, float thickness, egg_color_t color);

void egg_draw_point(egg_draw_t *draw, int32_t z, float x, float y, float size, egg_color_t color);

void egg_draw_circle(egg_draw_t *draw, int32_t z, float x, float y, float radius, egg_color_t color);

void egg_draw_circle_outline(egg_draw_t *draw, int32_t z, float x, float y, float radius, float thickness, egg_color_t color);

void egg_draw_capsule_outline(egg_draw_t *draw, int32_t z, float x1, float y1, float x2, float y2, float radius, float thickness, egg_color_t color);

void egg_draw_transform(egg_draw_t *draw, int32_t z, float x, float y, float rotationCos, float rotationSin, float scale, egg_color_t color);

void egg_draw_rectangle(egg_draw_t *draw, int32_t z, float x, float y, float rotationCos, float rotationSin, float width, float height, egg_color_t color);

void egg_draw_rectangle_outline(egg_draw_t *draw, int32_t z, float x, float y, float rotationCos, float rotationSin, float width, float height, float thickness, egg_color_t color);

void egg_draw_bounds(egg_draw_t *draw, int32_t z, float minX, float minY, float maxX, float maxY, egg_color_t color);

void egg_draw_polygon(egg_draw_t *draw, int32_t z, const egg_vec2_t *vertices, int vertex_count, float tx, float ty, float rot_c, float rot_s, egg_color_t color);

// Appends pre-transformed triangles (world space) to the z list; `vertices` is an EgShapedrawVertex array.
void egg_draw_append_vertices(egg_draw_t *draw, int32_t z, const EgShapedrawVertex *vertices, int32_t count);
