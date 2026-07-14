// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "draw_internal.h"
#include <flecs.h>

typedef struct
{
	b2Vec2 position;
	RGBA8  rgba;
} LineData;


typedef struct 
{
	ecs_vec_t       points;
	GLuint          vaoId;
	GLuint          vboId;
	GLuint          programId;
	GLint           projectionUniform;
} LineRender;


#ifdef __cplusplus
extern "C" {
#endif

LineRender *CreateLineRender(const DrawCreateInfo *createInfo);
void        DestroyLineRender(LineRender *render);
void        AddLine(LineRender *render, b2Vec2 p1, b2Vec2 p2, b2HexColor c);
void        FlushLines(LineRender *render, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
