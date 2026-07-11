// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

typedef struct LineRender LineRender;

#ifdef __cplusplus
extern "C" {
#endif

LineRender *CreateLineRender(const DrawCreateInfo *createInfo);
void        DestroyLineRender(LineRender *render);
void        AddLine(LineRender *render, b2Vec2 p1, b2Vec2 p2, b2HexColor c);
void        FlushLines(LineRender *render, Camera *camera);

#ifdef __cplusplus
}
#endif
