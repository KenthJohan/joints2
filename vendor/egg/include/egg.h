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

void egg_draw_text(egg_t *egg, float x, float y, float rotationCos, float rotationSin, float fontSize, egg_color_t color, const char *string);

void egg_draw_rectangle(egg_t *egg, float x, float y, float rotationCos, float rotationSin, float width, float height, egg_color_t color);

void egg_draw_polygon(egg_t *egg, const egg_vec2_t *vertices, int vertex_count, float tx, float ty,
					 float rot_c, float rot_s, egg_color_t color);

void egg_flush(egg_t *egg, const float *projectionMatrix);
