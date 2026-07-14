// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "draw_internal.h"
#include <flecs.h>

typedef struct
{
	b2Transform transform;
	float       radius;
	float       length;
	RGBA8       rgba;
} Capsule;


typedef struct
{
	ecs_vec_t    capsules;
	GLuint       vaoId;
	GLuint       vboIds[2];
	GLuint       programId;
	GLint        projectionUniform;
	GLint        pixelScaleUniform;
} Capsules;

#ifdef __cplusplus
extern "C" {
#endif

Capsules *CreateCapsules(const DrawCreateInfo *createInfo);
void      DestroyCapsules(Capsules *render);
void      AddCapsule(Capsules *render, b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor c);
void      FlushCapsules(Capsules *render, float pixelScale, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
