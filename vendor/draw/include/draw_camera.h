// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

Camera GetDefaultCamera(void);
void   ResetView(Camera *camera);
b2Pos  ConvertScreenToWorld(Camera *camera, b2Vec2 screenPoint);
b2Vec2 ConvertWorldToScreen(Camera *camera, b2Pos worldPoint);
b2Vec2 ConvertViewToScreen(Camera *camera, b2Vec2 viewPoint);
b2AABB GetViewBounds(Camera *camera);
void   FocusOnBounds(Camera *camera, b2AABB bounds);

#ifdef __cplusplus
}
#endif
