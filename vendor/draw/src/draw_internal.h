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
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
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
