// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

typedef struct draw_t draw_t;
typedef uint32_t      draw_color_t;

typedef enum draw_shader_type_t {
	DRAW_SHADER_BACKGROUND_VERTEX,
	DRAW_SHADER_BACKGROUND_FRAGMENT,
	DRAW_SHADER_POINT_VERTEX,
	DRAW_SHADER_POINT_FRAGMENT,
	DRAW_SHADER_LINE_VERTEX,
	DRAW_SHADER_LINE_FRAGMENT,
	DRAW_SHADER_CIRCLE_VERTEX,
	DRAW_SHADER_CIRCLE_FRAGMENT,
	DRAW_SHADER_SOLID_CIRCLE_VERTEX,
	DRAW_SHADER_SOLID_CIRCLE_FRAGMENT,
	DRAW_SHADER_SOLID_CAPSULE_VERTEX,
	DRAW_SHADER_SOLID_CAPSULE_FRAGMENT,
	DRAW_SHADER_SOLID_POLYGON_VERTEX,
	DRAW_SHADER_SOLID_POLYGON_FRAGMENT,
	DRAW_SHADER_TEXT_VERTEX,
	DRAW_SHADER_TEXT_FRAGMENT,
	DRAW_SHADER_COUNT,
} draw_shader_type_t;

typedef struct draw_create_info_t {
	const char *shaders[DRAW_SHADER_COUNT];
} draw_create_info_t;

draw_t *draw_init(const draw_create_info_t *createInfo);
void    draw_destroy(draw_t *draw);

void draw_screen_string(draw_t *draw, float x, float y, draw_color_t color, const char *string, ...);

void draw_point(draw_t *draw, float x, float y, float size, draw_color_t color);
void draw_line(draw_t *draw, float x1, float y1, float x2, float y2, draw_color_t color);
void draw_circle(draw_t *draw, float centerX, float centerY, float radius, draw_color_t color);
void draw_capsule(draw_t *draw, float x1, float y1, float x2, float y2, float radius, draw_color_t color);
void draw_polygon(draw_t *draw, float tx, float ty, float rotCos, float rotSin, const float *verticesXY, int vertexCount, draw_color_t color);
void draw_solid_circle(draw_t *draw, float tx, float ty, float rotCos, float rotSin, float centerX, float centerY, float radius, draw_color_t color);
void draw_solid_polygon(draw_t *draw, float tx, float ty, float rotCos, float rotSin, const float *verticesXY, int vertexCount, float radius, draw_color_t color);
void draw_transform(draw_t *draw, float tx, float ty, float rotCos, float rotSin, float scale);
void draw_bounds(draw_t *draw, float minX, float minY, float maxX, float maxY, draw_color_t color);
void draw_string(draw_t *draw, float x, float y, draw_color_t color, const char *string, ...);

void draw_flush(draw_t *draw, float pixelScale, const float *projectionMatrix);
void draw_background(draw_t *draw, float width, float height);
