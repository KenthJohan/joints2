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
	background_t  *background;
	points_t *points;
	lines_t  *lines;
	circles_t *hollowCircles;
	solid_circles_t *circles;
	solid_capsules_t    *capsules;
	solid_polygons_t    *polygons;
	text_t  *text;
} draw_t;

draw_t *draw_init(const draw_create_info_t *createInfo)
{
	draw_t *draw          = malloc(sizeof(draw_t));
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
void draw_point(draw_t *draw, b2Pos p, float size, b2HexColor color)
{
	points_add(draw->points, b2ToVec2(p), size, color);
}

void draw_line(draw_t *draw, b2Pos p1, b2Pos p2, b2HexColor color)
{
	lines_add(draw->lines, b2ToVec2(p1), b2ToVec2(p2), color);
}

void draw_circle(draw_t *draw, b2Pos center, float radius, b2HexColor color)
{
	circles_add(draw->hollowCircles, b2ToVec2(center), radius, color);
}

void draw_capsule(draw_t *draw, b2Pos p1, b2Pos p2, float radius, b2HexColor color)
{
	solid_capsules_add(draw->capsules, b2ToVec2(p1), b2ToVec2(p2), radius, color);
}

void draw_polygon(draw_t *draw, b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color)
{
	b2Vec2 p1 = b2TransformWorldPoint(transform, vertices[vertexCount - 1]);
	for (int i = 0; i < vertexCount; ++i) {
		b2Vec2 p2 = b2TransformWorldPoint(transform, vertices[i]);
		lines_add(draw->lines, p1, p2, color);
		p1 = p2;
	}
}

void draw_solid_circle(draw_t *draw, b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color)
{
	b2WorldTransform xf = {b2TransformWorldPoint(transform, center), transform.q};
	b2Transform localTransform = {b2ToVec2(xf.p), xf.q};
	solid_circles_add(draw->circles, localTransform, radius, color);
}

void draw_solid_polygon(draw_t *draw, b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius,
b2HexColor color)
{
	b2Transform localTransform = {b2ToVec2(transform.p), transform.q};
	solid_polygons_add(draw->polygons, localTransform, vertices, vertexCount, radius, color);
}

void draw_transform(draw_t *draw, b2WorldTransform transform, float scale)
{
	b2Transform xf = {b2ToVec2(transform.p), transform.q};

	b2Vec2 p1 = xf.p;

	b2Vec2 p2 = b2MulAdd(p1, scale, b2Rot_GetXAxis(xf.q));
	lines_add(draw->lines, p1, p2, b2_colorRed);

	p2 = b2MulAdd(p1, scale, b2Rot_GetYAxis(xf.q));
	lines_add(draw->lines, p1, p2, b2_colorGreen);
}

void draw_bounds(draw_t *draw, b2AABB aabb, b2HexColor color)
{
	b2Vec2 lower = aabb.lowerBound;
	b2Vec2 upper = aabb.upperBound;

	b2Vec2 p1 = lower;
	b2Vec2 p2 = {upper.x, lower.y};
	b2Vec2 p3 = upper;
	b2Vec2 p4 = {lower.x, upper.y};

	lines_add(draw->lines, p1, p2, color);
	lines_add(draw->lines, p2, p3, color);
	lines_add(draw->lines, p3, p4, color);
	lines_add(draw->lines, p4, p1, color);
}

void draw_screen_string(draw_t *draw, float x, float y, b2HexColor color, const char *string, ...)
{
	char    buffer[2048] = {0};
	va_list args;
	va_start(args, string);
	vsnprintf(buffer, sizeof(buffer), string, args);
	va_end(args);

	text_add(draw->text, x, y, 0.5f, color, buffer);
}

void draw_string(draw_t *draw, b2Pos p, b2HexColor color, const char *string, ...)
{
	char    buffer[2048] = {0};
	va_list args;
	va_start(args, string);
	vsnprintf(buffer, sizeof(buffer), string, args);
	va_end(args);
	b2Vec2 textPos = b2ToVec2(p);
	text_add(draw->text, textPos.x, textPos.y, 0.5f, color, buffer);
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
