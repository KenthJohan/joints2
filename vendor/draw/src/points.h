// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "internal.h"
#include <flecs.h>

typedef struct
{
	b2Vec2 position;
	float  size;
	RGBA8  rgba;
} points_data_t;


typedef struct
{
	ecs_vec_t points;
	GLuint         vaoId;
	GLuint         vboId;
	GLuint         programId;
	GLint          projectionUniform;
} points_t;

#ifdef __cplusplus
extern "C" {
#endif

points_t *points_init(const draw_create_info_t *createInfo);
void         points_destroy(points_t *render);
void         points_add(points_t *render, b2Vec2 v, float size, b2HexColor c);
void         points_flush(points_t *render, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
