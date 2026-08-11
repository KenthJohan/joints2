#pragma once
#include <stdint.h>

typedef uint32_t     egg_color_t;
typedef struct egg_t egg_t;

typedef struct {
	float x;
	float y;
} egg_vec2_t;

egg_t *egg_init(void);

void egg_destroy(egg_t *egg);

void egg_set_pixel_scale(egg_t *egg, float pixelScale);

void egg_draw_text(egg_t *egg, float x, float y, float rotationCos, float rotationSin, float fontSize, egg_color_t color, const char *string);

void egg_draw_line(egg_t *egg, float x1, float y1, float x2, float y2, float thickness, egg_color_t color);

void egg_draw_point(egg_t *egg, float x, float y, float size, egg_color_t color);

void egg_draw_circle(egg_t *egg, float x, float y, float radius, egg_color_t color);

void egg_draw_circle_outline(egg_t *egg, float x, float y, float radius, float thickness, egg_color_t color);

void egg_draw_capsule_outline(egg_t *egg, float x1, float y1, float x2, float y2, float radius, float thickness, egg_color_t color);

void egg_draw_transform(egg_t *egg, float x, float y, float rotationCos, float rotationSin, float scale, egg_color_t color);

void egg_draw_rectangle(egg_t *egg, float x, float y, float rotationCos, float rotationSin, float width, float height, egg_color_t color);

void egg_draw_bounds(egg_t *egg, float minX, float minY, float maxX, float maxY, egg_color_t color);

void egg_draw_polygon(egg_t *egg, const egg_vec2_t *vertices, int vertex_count, float tx, float ty, float rot_c, float rot_s, egg_color_t color);

void egg_flush(egg_t *egg, const float *projectionMatrix);
