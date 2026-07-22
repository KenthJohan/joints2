// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "internal.h"
#include <flecs.h>

typedef struct
{
	draw_transform_t transform;
	float            radius;
	float            length;
	RGBA8            rgba;
} solid_capsules_data_t;

typedef struct
{
	ecs_vec_t capsules;
	GLuint    vaoId;
	GLuint    vboIds[2];
	GLuint    programId;
	GLint     projectionUniform;
	GLint     pixelScaleUniform;
} solid_capsules_t;

#ifdef __cplusplus
extern "C" {
#endif

solid_capsules_t *solid_capsules_init(const draw_create_info_t *createInfo);
void              solid_capsules_destroy(solid_capsules_t *render);
void              solid_capsules_add(solid_capsules_t *render, draw_vec2_t p1, draw_vec2_t p2, float radius, draw_color_t c);
void              solid_capsules_flush(solid_capsules_t *render, float pixelScale, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
