// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "internal.h"
#include <flecs.h>

typedef struct
{
	b2Transform transform;
	float       radius;
	RGBA8       rgba;
} solid_circles_data_t;

typedef struct
{
	ecs_vec_t circles;
	GLuint    vaoId;
	GLuint    vboIds[2];
	GLuint    programId;
	GLint     projectionUniform;
	GLint     pixelScaleUniform;
} solid_circles_t;

#ifdef __cplusplus
extern "C" {
#endif

solid_circles_t *solid_circles_init(const draw_create_info_t *createInfo);
void             solid_circles_destroy(solid_circles_t *render);
void             solid_circles_add(solid_circles_t *render, b2Transform transform, float radius, b2HexColor color);
void             solid_circles_flush(solid_circles_t *render, float pixelScale, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
