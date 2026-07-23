// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"

#include "internal.h"
#include <flecs.h>

typedef struct
{
	draw_vec2_t position;
	float       radius;
	RGBA8       rgba;
} circles_data_t;

typedef struct
{
	ecs_vec_t circles;
	GLuint    vaoId;
	GLuint    vboIds[2];
	GLuint    programId;
	GLint     projectionUniform;
	GLint     pixelScaleUniform;
} circles_t;

#ifdef __cplusplus
extern "C" {
#endif

circles_t *circles_init(const draw_create_info_t *createInfo);
void       circles_destroy(circles_t *render);
void       circles_add(circles_t *render, draw_vec2_t center, float radius, draw_color_t color);
void       circles_flush(circles_t *render, float pixelScale, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
