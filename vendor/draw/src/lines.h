// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "internal.h"
#include <flecs.h>

typedef struct
{
	b2Vec2 position;
	RGBA8  rgba;
} lines_data_t;

typedef struct
{
	ecs_vec_t points;
	GLuint    vaoId;
	GLuint    vboId;
	GLuint    programId;
	GLint     projectionUniform;
} lines_t;

#ifdef __cplusplus
extern "C" {
#endif

lines_t *lines_init(const draw_create_info_t *createInfo);
void     lines_destroy(lines_t *render);
void     lines_add(lines_t *render, b2Vec2 p1, b2Vec2 p2, b2HexColor c);
void     lines_flush(lines_t *render, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
