// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "internal.h"
#include <flecs.h>

typedef struct
{
	b2Transform transform;
	b2Vec2      p1, p2, p3, p4, p5, p6, p7, p8;
	int         count;
	float       radius;
	RGBA8       color;
} solid_polygons_data_t;


typedef struct
{
	ecs_vec_t    polygons;
	GLuint       vaoId;
	GLuint       vboIds[2];
	GLuint       programId;
	GLint        projectionUniform;
	GLint        pixelScaleUniform;
} solid_polygons_t;

#ifdef __cplusplus
extern "C" {
#endif

solid_polygons_t *solid_polygons_init(const draw_create_info_t *createInfo);
void      solid_polygons_destroy(solid_polygons_t *render);
void      solid_polygons_add(solid_polygons_t *render, b2Transform transform, const b2Vec2 *points, int count, float radius, b2HexColor color);
void      solid_polygons_flush(solid_polygons_t *render, float pixelScale, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
