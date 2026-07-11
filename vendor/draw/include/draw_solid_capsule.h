// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

typedef struct Capsules Capsules;

#ifdef __cplusplus
extern "C" {
#endif

Capsules *CreateCapsules(const DrawCreateInfo *createInfo);
void      DestroyCapsules(Capsules *render);
void      AddCapsule(Capsules *render, b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor c);
void      FlushCapsules(Capsules *render, Camera *camera, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
