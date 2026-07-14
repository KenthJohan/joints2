// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw.h"
#include "stb_truetype.h"
#include "draw_internal.h"

#define TEXT_CHAR_COUNT 96

typedef struct
{
	b2Vec2 position;
	b2Vec2 uv;
	RGBA8  rgba;
} TextVertex;

ARRAY_DECLARE(TextVertex);



typedef struct {
	TextVertexArray vertices;
	stbtt_bakedchar glyphs[TEXT_CHAR_COUNT];
	GLuint          vaoId;
	GLuint          vboId;
	GLuint          textureId;
	GLuint          programId;
	GLint           projectionUniform;
	GLint           textureUniform;
	float           lineHeight;
	int             initialized;
} TextRender;

#ifdef __cplusplus
extern "C" {
#endif

TextRender *CreateTextRender(const DrawCreateInfo *createInfo);
void        DestroyTextRender(TextRender *render);
void        AddText(TextRender *render, float x, float y, float fontSize, b2HexColor color, const char *string);
void        FlushText(TextRender *render, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
