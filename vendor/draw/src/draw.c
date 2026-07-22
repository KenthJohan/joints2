// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw.h"

#include "internal.h"
#include "background.h"
#include "points.h"
#include "lines.h"
#include "circles.h"
#include "solid_circles.h"
#include "solid_capsules.h"
#include "solid_polygons.h"
#include "text.h"

#include <string.h>

typedef struct draw_t {
	background_t     *background;
	points_t         *points;
	lines_t          *lines;
	circles_t        *hollowCircles;
	solid_circles_t  *circles;
	solid_capsules_t *capsules;
	solid_polygons_t *polygons;
	text_t           *text;
} draw_t;

static draw_transform_t make_transform(float tx, float ty, float rotCos, float rotSin)
{
	return (draw_transform_t){{tx, ty}, {rotCos, rotSin}};
}

draw_t *draw_init(const draw_create_info_t *createInfo)
{
	draw_t *draw        = malloc(sizeof(draw_t));
	*draw               = (draw_t){0};
	draw->background    = background_init(createInfo);
	draw->points        = points_init(createInfo);
	draw->lines         = lines_init(createInfo);
	draw->hollowCircles = circles_init(createInfo);
	draw->circles       = solid_circles_init(createInfo);
	draw->capsules      = solid_capsules_init(createInfo);
	draw->polygons      = solid_polygons_init(createInfo);
	draw->text          = text_init(createInfo);
	return draw;
}

void draw_destroy(draw_t *draw)
{
	if (draw == NULL) {
		return;
	}

	background_destroy(draw->background);
	points_destroy(draw->points);
	lines_destroy(draw->lines);
	circles_destroy(draw->hollowCircles);
	solid_circles_destroy(draw->circles);
	solid_capsules_destroy(draw->capsules);
	solid_polygons_destroy(draw->polygons);
	text_destroy(draw->text);
	free(draw);
}

void draw_point(draw_t *draw, float x, float y, float size, draw_color_t color)
{
	points_add(draw->points, (draw_vec2_t){x, y}, size, color);
}

void draw_line(draw_t *draw, float x1, float y1, float x2, float y2, draw_color_t color)
{
	lines_add(draw->lines, (draw_vec2_t){x1, y1}, (draw_vec2_t){x2, y2}, color);
}

void draw_circle(draw_t *draw, float centerX, float centerY, float radius, draw_color_t color)
{
	circles_add(draw->hollowCircles, (draw_vec2_t){centerX, centerY}, radius, color);
}

void draw_capsule(draw_t *draw, float x1, float y1, float x2, float y2, float radius, draw_color_t color)
{
	solid_capsules_add(draw->capsules, (draw_vec2_t){x1, y1}, (draw_vec2_t){x2, y2}, radius, color);
}

void draw_polygon(draw_t *draw, float tx, float ty, float rotCos, float rotSin, const float *verticesXY, int vertexCount, draw_color_t color)
{
	draw_transform_t transform = make_transform(tx, ty, rotCos, rotSin);
	draw_vec2_t      p1        = draw_transform_point(transform, (draw_vec2_t){verticesXY[2 * (vertexCount - 1)], verticesXY[2 * (vertexCount - 1) + 1]});
	for (int i = 0; i < vertexCount; ++i) {
		draw_vec2_t local = {verticesXY[2 * i], verticesXY[2 * i + 1]};
		draw_vec2_t p2    = draw_transform_point(transform, local);
		lines_add(draw->lines, p1, p2, color);
		p1 = p2;
	}
}

void draw_solid_circle(draw_t *draw, float tx, float ty, float rotCos, float rotSin, float centerX, float centerY, float radius,
draw_color_t color)
{
	draw_transform_t transform      = make_transform(tx, ty, rotCos, rotSin);
	draw_vec2_t      center         = {centerX, centerY};
	draw_transform_t localTransform = {draw_transform_point(transform, center), transform.q};
	solid_circles_add(draw->circles, localTransform, radius, color);
}

void draw_solid_polygon(draw_t *draw, float tx, float ty, float rotCos, float rotSin, const float *verticesXY, int vertexCount, float radius,
draw_color_t color)
{
	assert(vertexCount <= DRAW_MAX_POLYGON_VERTICES);

	draw_transform_t transform = make_transform(tx, ty, rotCos, rotSin);
	draw_vec2_t      vertices[DRAW_MAX_POLYGON_VERTICES];
	for (int i = 0; i < vertexCount; ++i) {
		vertices[i] = (draw_vec2_t){verticesXY[2 * i], verticesXY[2 * i + 1]};
	}
	solid_polygons_add(draw->polygons, transform, vertices, vertexCount, radius, color);
}

void draw_transform(draw_t *draw, float tx, float ty, float rotCos, float rotSin, float scale)
{
	draw_transform_t xf = make_transform(tx, ty, rotCos, rotSin);

	draw_vec2_t p1 = xf.p;

	draw_vec2_t p2 = draw_vec2_mul_add(p1, scale, draw_rot_get_x_axis(xf.q));
	lines_add(draw->lines, p1, p2, DRAW_COLOR_RED);

	p2 = draw_vec2_mul_add(p1, scale, draw_rot_get_y_axis(xf.q));
	lines_add(draw->lines, p1, p2, DRAW_COLOR_GREEN);
}

void draw_bounds(draw_t *draw, float minX, float minY, float maxX, float maxY, draw_color_t color)
{
	draw_vec2_t p1 = {minX, minY};
	draw_vec2_t p2 = {maxX, minY};
	draw_vec2_t p3 = {maxX, maxY};
	draw_vec2_t p4 = {minX, maxY};

	lines_add(draw->lines, p1, p2, color);
	lines_add(draw->lines, p2, p3, color);
	lines_add(draw->lines, p3, p4, color);
	lines_add(draw->lines, p4, p1, color);
}

void draw_screen_string(draw_t *draw, float x, float y, draw_color_t color, const char *string, ...)
{
	char    buffer[2048] = {0};
	va_list args;
	va_start(args, string);
	vsnprintf(buffer, sizeof(buffer), string, args);
	va_end(args);

	text_add(draw->text, x, y, 0.5f, color, buffer);
}

void draw_string(draw_t *draw, float x, float y, draw_color_t color, const char *string, ...)
{
	char    buffer[2048] = {0};
	va_list args;
	va_start(args, string);
	vsnprintf(buffer, sizeof(buffer), string, args);
	va_end(args);
	text_add(draw->text, x, y, 0.5f, color, buffer);
}

static void SetProjectionZBias(float *dst, const float *src, float zBias)
{
	memcpy(dst, src, 16 * sizeof(float));
	dst[14] = zBias;
}

void draw_flush(draw_t *draw, float pixelScale, const float *projectionMatrix)
{
	float projDefault[16];
	float projLine[16];
	float projShape[16];

	SetProjectionZBias(projDefault, projectionMatrix, 0.0f);
	SetProjectionZBias(projLine, projectionMatrix, 0.1f);
	SetProjectionZBias(projShape, projectionMatrix, 0.2f);

	// order matters
	solid_circles_flush(draw->circles, pixelScale, projShape);
	solid_capsules_flush(draw->capsules, pixelScale, projShape);
	solid_polygons_flush(draw->polygons, pixelScale, projShape);
	circles_flush(draw->hollowCircles, pixelScale, projShape);
	lines_flush(draw->lines, projLine);
	points_flush(draw->points, projDefault);
	text_flush(draw->text, projDefault);
	CheckOpenGL();
}

void draw_background(draw_t *draw, float width, float height)
{
	background_render(draw->background, width, height);
}
