// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "draw_internal.h"

typedef struct
{
	GLuint vaoId;
	GLuint vboId;
	GLuint programId;
	GLint  timeUniform;
	GLint  resolutionUniform;
	GLint  baseColorUniform;
} Background;

#ifdef __cplusplus
extern "C" {
#endif

Background *CreateBackground(const DrawCreateInfo *createInfo);
void        DestroyBackground(Background *background);
void        RenderBackground(Background *background, float width, float height);

#ifdef __cplusplus
}
#endif
