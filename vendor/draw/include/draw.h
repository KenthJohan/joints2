// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "box2d/types.h"

typedef struct Camera {
	// World point the view is centered on. Double precision in large world mode.
	b2Pos center;
	float zoom;
	float width;
	float height;
} Camera;

typedef struct Draw Draw;

typedef enum DrawShaderType {
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
} DrawShaderType;

typedef struct DrawCreateInfo {
	const char *shaders[DRAW_SHADER_COUNT];
} DrawCreateInfo;

#ifdef __cplusplus
extern "C" {
#endif

Camera GetDefaultCamera(void);
void   ResetView(Camera *camera);
b2Pos  ConvertScreenToWorld(Camera *camera, b2Vec2 screenPoint);
b2Vec2 ConvertWorldToScreen(Camera *camera, b2Pos worldPoint);
b2Vec2 ConvertViewToScreen(Camera *camera, b2Vec2 viewPoint);
b2AABB GetViewBounds(Camera *camera);
void   FocusOnBounds(Camera *camera, b2AABB bounds);

Draw *CreateDraw(const DrawCreateInfo *createInfo);
void  DestroyDraw(Draw *draw);

void DrawScreenString(Draw *draw, float x, float y, b2HexColor color, const char *string, ...);

// Set the drawing origin to the camera center each frame so far from the origin the
// shift happens in double before the coordinates demote to float for the GPU.
void SetDrawOrigin(Draw *draw, b2Pos origin);

// World space draws.
void DrawPoint(Draw *draw, b2Pos p, float size, b2HexColor color);
void DrawLine(Draw *draw, b2Pos p1, b2Pos p2, b2HexColor color);
void DrawCircle(Draw *draw, b2Pos center, float radius, b2HexColor color);
void DrawCapsule(Draw *draw, b2Pos p1, b2Pos p2, float radius, b2HexColor color);
void DrawPolygon(Draw *draw, b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color);
void DrawSolidCircle(Draw *draw, b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color);
void DrawSolidPolygon(Draw *draw, b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius, b2HexColor color);
void DrawTransform(Draw *draw, b2WorldTransform transform, float scale);
void DrawBounds(Draw *draw, b2AABB aabb, b2HexColor color);
void DrawString(Draw *draw, Camera *camera, b2Pos p, b2HexColor color, const char *string, ...);

void FlushDraw(Draw *draw, Camera *camera);

// Some noise to reduce video compression.
void DrawBackground(Draw *draw, Camera *camera);

#ifdef __cplusplus
}
#endif
