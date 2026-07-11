// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

typedef struct CircleRender CircleRender;

#ifdef __cplusplus
extern "C" {
#endif

CircleRender *CreateCircles(const DrawCreateInfo *createInfo);
void          DestroyCircles(CircleRender *render);
void          AddCircle(CircleRender *render, b2Vec2 center, float radius, b2HexColor color);
void          FlushCircles(CircleRender *render, Camera *camera, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
