// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "container.h"
#include "shader.h"

#include "draw_types.h"

#include "box2d/constants.h"
#include "box2d/math_functions.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(_MSC_VER)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#else
#include <stdlib.h>
#endif

// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
// clang-format on

#define BUFFER_OFFSET(x) ((const void *)(x))

typedef struct RGBA8 {
	uint8_t r, g, b, a;
} RGBA8;

static inline RGBA8 MakeRGBA8(b2HexColor c, float alpha)
{
	return (RGBA8){
	(uint8_t)((c >> 16) & 0xFF),
	(uint8_t)((c >> 8) & 0xFF),
	(uint8_t)(c & 0xFF),
	(uint8_t)(0xFF * alpha),
	};
}

static inline void BuildProjectionMatrix(Camera *camera, float *m, float zBias)
{
	float  ratio   = camera->width / camera->height;
	b2Vec2 extents = {camera->zoom * ratio, camera->zoom};

	float w = 2.0f * extents.x;
	float h = 2.0f * extents.y;

	m[0] = 2.0f / w;
	m[1] = 0.0f;
	m[2] = 0.0f;
	m[3] = 0.0f;

	m[4] = 0.0f;
	m[5] = 2.0f / h;
	m[6] = 0.0f;
	m[7] = 0.0f;

	m[8]  = 0.0f;
	m[9]  = 0.0f;
	m[10] = -1.0f;
	m[11] = 0.0f;

	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = zBias;
	m[15] = 1.0f;
}
