// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "internal.h"

typedef struct
{
	GLuint vaoId;
	GLuint vboId;
	GLuint programId;
	GLint  timeUniform;
	GLint  resolutionUniform;
	GLint  baseColorUniform;
} background_t;

#ifdef __cplusplus
extern "C" {
#endif

background_t *background_init(const draw_create_info_t *createInfo);
void          background_destroy(background_t *background);
void          background_render(background_t *background, float width, float height);

#ifdef __cplusplus
}
#endif
