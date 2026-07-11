// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

typedef struct SolidCircles SolidCircles;

#ifdef __cplusplus
extern "C" {
#endif

SolidCircles *CreateSolidCircles(const DrawCreateInfo *createInfo);
void          DestroySolidCircles(SolidCircles *render);
void          AddSolidCircle(SolidCircles *render, b2Transform transform, float radius, b2HexColor color);
void          FlushSolidCircles(SolidCircles *render, Camera *camera);

#ifdef __cplusplus
}
#endif
