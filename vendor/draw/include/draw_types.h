// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "box2d/types.h"

typedef struct Camera {
	// World point the view is centered on. Double precision in large world mode.
	b2Pos center;
	float zoom;
	float width;
	float height;
} Camera;

typedef struct Draw Draw;

typedef enum DrawShaderType {
	DRAW_SHADER_BACKGROUND_VERTEX,
	DRAW_SHADER_BACKGROUND_FRAGMENT,
	DRAW_SHADER_POINT_VERTEX,
	DRAW_SHADER_POINT_FRAGMENT,
	DRAW_SHADER_LINE_VERTEX,
	DRAW_SHADER_LINE_FRAGMENT,
	DRAW_SHADER_CIRCLE_VERTEX,
	DRAW_SHADER_CIRCLE_FRAGMENT,
	DRAW_SHADER_SOLID_CIRCLE_VERTEX,
	DRAW_SHADER_SOLID_CIRCLE_FRAGMENT,
	DRAW_SHADER_SOLID_CAPSULE_VERTEX,
	DRAW_SHADER_SOLID_CAPSULE_FRAGMENT,
	DRAW_SHADER_SOLID_POLYGON_VERTEX,
	DRAW_SHADER_SOLID_POLYGON_FRAGMENT,
	DRAW_SHADER_TEXT_VERTEX,
	DRAW_SHADER_TEXT_FRAGMENT,
	DRAW_SHADER_COUNT,
} DrawShaderType;

typedef struct DrawCreateInfo {
	const char *shaders[DRAW_SHADER_COUNT];
} DrawCreateInfo;
