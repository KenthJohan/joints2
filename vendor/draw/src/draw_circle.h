// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"

#include "draw_internal.h"
#include <flecs.h>

typedef struct
{
	b2Vec2 position;
	float  radius;
	RGBA8  rgba;
} CircleData;


typedef struct
{
	ecs_vec_t       circles;
	GLuint          vaoId;
	GLuint          vboIds[2];
	GLuint          programId;
	GLint           projectionUniform;
	GLint           pixelScaleUniform;
} CircleRender;

#ifdef __cplusplus
extern "C" {
#endif

CircleRender *CreateCircles(const DrawCreateInfo *createInfo);
void          DestroyCircles(CircleRender *render);
void          AddCircle(CircleRender *render, b2Vec2 center, float radius, b2HexColor color);
void          FlushCircles(CircleRender *render, float pixelScale, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
