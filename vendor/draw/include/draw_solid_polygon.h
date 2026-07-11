// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

typedef struct Polygons Polygons;

#ifdef __cplusplus
extern "C" {
#endif

Polygons *CreatePolygons(const DrawCreateInfo *createInfo);
void      DestroyPolygons(Polygons *render);
void      AddPolygon(Polygons *render, b2Transform transform, const b2Vec2 *points, int count, float radius, b2HexColor color);
void      FlushPolygons(Polygons *render, Camera *camera);

#ifdef __cplusplus
}
#endif
