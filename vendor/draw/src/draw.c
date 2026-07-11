// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw.h"

#include "draw_internal.h"

#include <string.h>

typedef struct Draw {
	Background  *background;
	PointRender *points;
	LineRender  *lines;
	CircleRender *hollowCircles;
	SolidCircles *circles;
	Capsules    *capsules;
	Polygons    *polygons;
	TextRender  *text;

	// View origin in large world mode, subtracted by the DrawWorld helpers. Zero in float mode.
	b2Pos origin;
} Draw;

Draw *CreateDraw(const DrawCreateInfo *createInfo)
{
	Draw *draw          = malloc(sizeof(Draw));
	*draw               = (Draw){0};
	draw->background    = CreateBackground(createInfo);
	draw->points        = CreatePointDrawData(createInfo);
	draw->lines         = CreateLineRender(createInfo);
	draw->hollowCircles = CreateCircles(createInfo);
	draw->circles       = CreateSolidCircles(createInfo);
	draw->capsules      = CreateCapsules(createInfo);
	draw->polygons      = CreatePolygons(createInfo);
	draw->text          = CreateTextRender(createInfo);
	return draw;
}

void DestroyDraw(Draw *draw)
{
	if (draw == NULL) {
		return;
	}

	DestroyBackground(&draw->background);
	DestroyPointDrawData(draw->points);
	DestroyLineRender(draw->lines);
	DestroyCircles(draw->hollowCircles);
	DestroySolidCircles(draw->circles);
	DestroyCapsules(draw->capsules);
	DestroyPolygons(draw->polygons);
	DestroyTextRender(draw->text);
	free(draw);
}

void SetDrawOrigin(Draw *draw, b2Pos origin)
{
	draw->origin = origin;
}

void DrawPoint(Draw *draw, b2Pos p, float size, b2HexColor color)
{
	AddPoint(draw->points, b2SubPos(p, draw->origin), size, color);
}

void DrawLine(Draw *draw, b2Pos p1, b2Pos p2, b2HexColor color)
{
	AddLine(draw->lines, b2SubPos(p1, draw->origin), b2SubPos(p2, draw->origin), color);
}

void DrawCircle(Draw *draw, b2Pos center, float radius, b2HexColor color)
{
	AddCircle(draw->hollowCircles, b2SubPos(center, draw->origin), radius, color);
}

void DrawCapsule(Draw *draw, b2Pos p1, b2Pos p2, float radius, b2HexColor color)
{
	AddCapsule(draw->capsules, b2SubPos(p1, draw->origin), b2SubPos(p2, draw->origin), radius, color);
}

void DrawPolygon(Draw *draw, b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color)
{
	b2Transform xf = b2ToRelativeTransform(transform, draw->origin);
	b2Vec2      p1 = b2TransformPoint(xf, vertices[vertexCount - 1]);
	for (int i = 0; i < vertexCount; ++i) {
		b2Vec2 p2 = b2TransformPoint(xf, vertices[i]);
		AddLine(draw->lines, p1, p2, color);
		p1 = p2;
	}
}

void DrawSolidCircle(Draw *draw, b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color)
{
	// Fold the local center offset into the world transform, then shift into the local view frame
	b2WorldTransform xf             = {b2TransformWorldPoint(transform, center), transform.q};
	b2Transform      localTransform = b2ToRelativeTransform(xf, draw->origin);
	AddSolidCircle(draw->circles, localTransform, radius, color);
}

void DrawSolidPolygon(Draw *draw, b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius,
b2HexColor color)
{
	AddPolygon(draw->polygons, b2ToRelativeTransform(transform, draw->origin), vertices, vertexCount, radius, color);
}

void DrawTransform(Draw *draw, b2WorldTransform transform, float scale)
{
	b2Transform xf = b2ToRelativeTransform(transform, draw->origin);

	b2Vec2 p1 = xf.p;

	b2Vec2 p2 = b2MulAdd(p1, scale, b2Rot_GetXAxis(xf.q));
	AddLine(draw->lines, p1, p2, b2_colorRed);

	p2 = b2MulAdd(p1, scale, b2Rot_GetYAxis(xf.q));
	AddLine(draw->lines, p1, p2, b2_colorGreen);
}

void DrawBounds(Draw *draw, b2AABB aabb, b2HexColor color)
{
	b2Vec2 lower = b2SubPos(b2ToPos(aabb.lowerBound), draw->origin);
	b2Vec2 upper = b2SubPos(b2ToPos(aabb.upperBound), draw->origin);

	b2Vec2 p1 = lower;
	b2Vec2 p2 = {upper.x, lower.y};
	b2Vec2 p3 = upper;
	b2Vec2 p4 = {lower.x, upper.y};

	AddLine(draw->lines, p1, p2, color);
	AddLine(draw->lines, p2, p3, color);
	AddLine(draw->lines, p3, p4, color);
	AddLine(draw->lines, p4, p1, color);
}

void DrawScreenString(Draw *draw, float x, float y, b2HexColor color, const char *string, ...)
{
	char    buffer[2048] = {0};
	va_list args;
	va_start(args, string);
	vsnprintf(buffer, sizeof(buffer), string, args);
	va_end(args);

	AddText(draw->text, x, y, 0.5f, color, buffer);
}

void DrawString(Draw *draw, b2Pos p, b2HexColor color, const char *string, ...)
{
	char    buffer[2048] = {0};
	va_list args;
	va_start(args, string);
	vsnprintf(buffer, sizeof(buffer), string, args);
	va_end(args);
	b2Vec2 textPos = b2SubPos(p, draw->origin);
	AddText(draw->text, textPos.x, textPos.y, 0.5f, color, buffer);
}

static void SetProjectionZBias(float *dst, const float *src, float zBias)
{
	memcpy(dst, src, 16 * sizeof(float));
	dst[14] = zBias;
}

void FlushDraw(Draw *draw, float pixelScale, const float *projectionMatrix)
{
	float projDefault[16];
	float projLine[16];
	float projShape[16];

	SetProjectionZBias(projDefault, projectionMatrix, 0.0f);
	SetProjectionZBias(projLine, projectionMatrix, 0.1f);
	SetProjectionZBias(projShape, projectionMatrix, 0.2f);

	// order matters
	FlushSolidCircles(draw->circles, pixelScale, projShape);
	FlushCapsules(draw->capsules, pixelScale, projShape);
	FlushPolygons(draw->polygons, pixelScale, projShape);
	FlushCircles(draw->hollowCircles, pixelScale, projShape);
	FlushLines(draw->lines, projLine);
	FlushPoints(draw->points, projDefault);
	FlushText(draw->text, projDefault);
	CheckOpenGL();
}

void DrawBackground(Draw *draw, float width, float height)
{
	RenderBackground(draw->background, width, height);
}
