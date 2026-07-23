// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "stb_truetype.h"
#include "internal.h"
#include <flecs.h>

#define TEXT_CHAR_COUNT 96

typedef struct
{
	draw_vec2_t position;
	draw_vec2_t uv;
	RGBA8       rgba;
} text_vertex_t;

typedef struct {
	ecs_vec_t       vertices;
	stbtt_bakedchar glyphs[TEXT_CHAR_COUNT];
	GLuint          vaoId;
	GLuint          vboId;
	GLuint          textureId;
	GLuint          programId;
	GLint           projectionUniform;
	GLint           textureUniform;
	float           lineHeight;
	int             initialized;
} text_t;

#ifdef __cplusplus
extern "C" {
#endif

text_t *text_init(const draw_create_info_t *createInfo);
void    text_destroy(text_t *render);
void    text_add(text_t *render, float x, float y, float fontSize, draw_color_t color, const char *string);
void    text_flush(text_t *render, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
