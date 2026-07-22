// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "shader.h"

#include <assert.h>
#include <math.h>
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

enum {
	DRAW_MAX_POLYGON_VERTICES = 8,
};

typedef struct draw_vec2_t {
	float x;
	float y;
} draw_vec2_t;

typedef struct draw_rot_t {
	float c;
	float s;
} draw_rot_t;

typedef struct draw_transform_t {
	draw_vec2_t p;
	draw_rot_t  q;
} draw_transform_t;

static inline int draw_min_int(int a, int b)
{
	return a < b ? a : b;
}

static inline draw_vec2_t draw_vec2_sub(draw_vec2_t a, draw_vec2_t b)
{
	return (draw_vec2_t){a.x - b.x, a.y - b.y};
}

static inline float draw_vec2_length(draw_vec2_t v)
{
	return sqrtf(v.x * v.x + v.y * v.y);
}

static inline draw_vec2_t draw_vec2_lerp(draw_vec2_t a, draw_vec2_t b, float t)
{
	return (draw_vec2_t){a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
}

static inline draw_vec2_t draw_rot_get_x_axis(draw_rot_t q)
{
	return (draw_vec2_t){q.c, q.s};
}

static inline draw_vec2_t draw_rot_get_y_axis(draw_rot_t q)
{
	return (draw_vec2_t){-q.s, q.c};
}

static inline draw_vec2_t draw_vec2_mul_add(draw_vec2_t a, float s, draw_vec2_t b)
{
	return (draw_vec2_t){a.x + s * b.x, a.y + s * b.y};
}

static inline draw_vec2_t draw_transform_point(draw_transform_t t, draw_vec2_t p)
{
	return (draw_vec2_t){t.q.c * p.x - t.q.s * p.y + t.p.x, t.q.s * p.x + t.q.c * p.y + t.p.y};
}

#define DRAW_COLOR_RED   0xFF0000u
#define DRAW_COLOR_GREEN 0x00FF00u

typedef struct RGBA8 {
	uint8_t r, g, b, a;
} RGBA8;

static inline RGBA8 MakeRGBA8(draw_color_t c, float alpha)
{
	return (RGBA8){
	(uint8_t)((c >> 16) & 0xFF),
	(uint8_t)((c >> 8) & 0xFF),
	(uint8_t)(c & 0xFF),
	(uint8_t)(0xFF * alpha),
	};
}
