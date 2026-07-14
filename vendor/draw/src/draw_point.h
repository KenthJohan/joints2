// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "draw_internal.h"
#include <flecs.h>

typedef struct
{
	b2Vec2 position;
	float  size;
	RGBA8  rgba;
} PointData;


typedef struct
{
	ecs_vec_t points;
	GLuint         vaoId;
	GLuint         vboId;
	GLuint         programId;
	GLint          projectionUniform;
} PointRender;

#ifdef __cplusplus
extern "C" {
#endif

PointRender *CreatePointDrawData(const DrawCreateInfo *createInfo);
void         DestroyPointDrawData(PointRender *render);
void         AddPoint(PointRender *render, b2Vec2 v, float size, b2HexColor c);
void         FlushPoints(PointRender *render, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
