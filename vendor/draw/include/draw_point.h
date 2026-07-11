// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

typedef struct PointRender PointRender;

#ifdef __cplusplus
extern "C" {
#endif

PointRender *CreatePointDrawData(const DrawCreateInfo *createInfo);
void         DestroyPointDrawData(PointRender *render);
void         AddPoint(PointRender *render, b2Vec2 v, float size, b2HexColor c);
void         FlushPoints(PointRender *render, Camera *camera);

#ifdef __cplusplus
}
#endif
