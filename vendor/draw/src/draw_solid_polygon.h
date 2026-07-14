// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "draw_internal.h"
#include <flecs.h>

typedef struct
{
	b2Transform transform;
	b2Vec2      p1, p2, p3, p4, p5, p6, p7, p8;
	int         count;
	float       radius;
	RGBA8       color;
} Polygon;


typedef struct
{
	ecs_vec_t    polygons;
	GLuint       vaoId;
	GLuint       vboIds[2];
	GLuint       programId;
	GLint        projectionUniform;
	GLint        pixelScaleUniform;
} Polygons;

#ifdef __cplusplus
extern "C" {
#endif

Polygons *CreatePolygons(const DrawCreateInfo *createInfo);
void      DestroyPolygons(Polygons *render);
void      AddPolygon(Polygons *render, b2Transform transform, const b2Vec2 *points, int count, float radius, b2HexColor color);
void      FlushPolygons(Polygons *render, float pixelScale, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
