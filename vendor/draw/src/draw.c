// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw.h"

#include "draw_internal.h"

Camera GetDefaultCamera(void)
{
	return (Camera){
	.center = {0.0f, 20.0f},
	.zoom   = 1.0f,
	.width  = 1920.0f,
	.height = 1080.0f,
	};
}

void ResetView(Camera *camera)
{
	camera->center = (b2Pos){0.0f, 20.0f};
	camera->zoom   = 1.0f;
}

b2Pos ConvertScreenToWorld(Camera *camera, b2Vec2 screenPoint)
{
	float w = camera->width;
	float h = camera->height;
	float u = screenPoint.x / w;
	float v = (h - screenPoint.y) / h;

	float  ratio   = w / h;
	b2Vec2 extents = {camera->zoom * ratio, camera->zoom};

	// Form the offset from the view center in float, then add to the center. Building
	// center +/- extents in float would lose the view-sized extents far from the origin.
	b2Vec2 offset = {extents.x * (2.0f * u - 1.0f), extents.y * (2.0f * v - 1.0f)};
	return b2OffsetPos(camera->center, offset);
}

b2Vec2 ConvertViewToScreen(Camera *camera, b2Vec2 viewPoint)
{
	float w     = camera->width;
	float h     = camera->height;
	float ratio = w / h;

	b2Vec2 extents = {camera->zoom * ratio, camera->zoom};

	float u = (viewPoint.x + extents.x) / (2.0f * extents.x);
	float v = (viewPoint.y + extents.y) / (2.0f * extents.y);

	b2Vec2 ps = {u * w, (1.0f - v) * h};
	return ps;
}

b2Vec2 ConvertWorldToScreen(Camera *camera, b2Pos worldPoint)
{
	// Distance from the view center, demoted to float, then the float mapping
	return ConvertViewToScreen(camera, b2SubPos(worldPoint, camera->center));
}

b2AABB GetViewBounds(Camera *camera)
{
	if (camera->height == 0.0f || camera->width == 0.0f) {
		b2AABB bounds = {
		.lowerBound = b2Vec2_zero,
		.upperBound = b2Vec2_zero,
		};
		return bounds;
	}

	b2Pos lower = ConvertScreenToWorld(camera, (b2Vec2){0.0f, camera->height});
	b2Pos upper = ConvertScreenToWorld(camera, (b2Vec2){camera->width, 0.0f});

	// Engine cull box stays float. Round outward so nothing visible is clipped far from the origin.
	b2AABB bounds;
	bounds.lowerBound = (b2Vec2){b2RoundDownFloat(lower.x), b2RoundDownFloat(lower.y)};
	bounds.upperBound = (b2Vec2){b2RoundUpFloat(upper.x), b2RoundUpFloat(upper.y)};
	return bounds;
}

void FocusOnBounds(Camera *camera, b2AABB bounds)
{
	if (camera->width == 0) {
		return;
	}

	b2Vec2 extents = b2AABB_Extents(bounds);

	if (extents.x < B2_LINEAR_SLOP || extents.y < B2_LINEAR_SLOP) {
		return;
	}

	float invRatio = camera->height / camera->width;
	camera->zoom   = b2MaxFloat(extents.x * invRatio, extents.y);

	// Need to guard against zero because zoom can get stuck there
	camera->zoom = b2MaxFloat(camera->zoom, 0.01f);

	camera->center = b2ToPos(b2AABB_Center(bounds));
}

typedef struct Draw {
	Background  *background;
	PointRender *points;
	LineRender  *lines;
	CircleRender *hollowCircles;
	SolidCircles *circles;
	Capsules    *capsules;
	Polygons    *polygons;
	TextRender  *text;

	// Camera center in large world mode, subtracted by the DrawWorld helpers. Zero in float mode.
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
	// Fold the local center offset into the world transform, then shift into the camera frame
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

void FlushDraw(Draw *draw, Camera *camera)
{
	// order matters
	FlushSolidCircles(draw->circles, camera);
	FlushCapsules(draw->capsules, camera);
	FlushPolygons(draw->polygons, camera);
	FlushCircles(draw->hollowCircles, camera);
	FlushLines(draw->lines, camera);
	FlushPoints(draw->points, camera);
	FlushText(draw->text, camera);
	CheckOpenGL();
}

void DrawBackground(Draw *draw, Camera *camera)
{
	RenderBackground(draw->background, camera);
}
