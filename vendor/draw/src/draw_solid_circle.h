// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "container.h"
#include "draw_internal.h"

typedef struct
{
	b2Transform transform;
	float       radius;
	RGBA8       rgba;
} SolidCircle;

ARRAY_DECLARE(SolidCircle);

typedef struct
{
	SolidCircleArray circles;
	GLuint           vaoId;
	GLuint           vboIds[2];
	GLuint           programId;
	GLint            projectionUniform;
	GLint            pixelScaleUniform;
} SolidCircles;

#ifdef __cplusplus
extern "C" {
#endif

SolidCircles *CreateSolidCircles(const DrawCreateInfo *createInfo);
void          DestroySolidCircles(SolidCircles *render);
void          AddSolidCircle(SolidCircles *render, b2Transform transform, float radius, b2HexColor color);
void          FlushSolidCircles(SolidCircles *render, float pixelScale, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
