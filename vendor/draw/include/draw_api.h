// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

Draw *CreateDraw(const DrawCreateInfo *createInfo);
void  DestroyDraw(Draw *draw);

void DrawScreenString(Draw *draw, float x, float y, b2HexColor color, const char *string, ...);

void DrawPoint(Draw *draw, b2Pos p, float size, b2HexColor color);
void DrawLine(Draw *draw, b2Pos p1, b2Pos p2, b2HexColor color);
void DrawCircle(Draw *draw, b2Pos center, float radius, b2HexColor color);
void DrawCapsule(Draw *draw, b2Pos p1, b2Pos p2, float radius, b2HexColor color);
void DrawPolygon(Draw *draw, b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, b2HexColor color);
void DrawSolidCircle(Draw *draw, b2WorldTransform transform, b2Vec2 center, float radius, b2HexColor color);
void DrawSolidPolygon(Draw *draw, b2WorldTransform transform, const b2Vec2 *vertices, int vertexCount, float radius, b2HexColor color);
void DrawTransform(Draw *draw, b2WorldTransform transform, float scale);
void DrawBounds(Draw *draw, b2AABB aabb, b2HexColor color);
void DrawString(Draw *draw, b2Pos p, b2HexColor color, const char *string, ...);

void FlushDraw(Draw *draw, float pixelScale, const float *projectionMatrix);
void DrawBackground(Draw *draw, float width, float height);

#ifdef __cplusplus
}
#endif
