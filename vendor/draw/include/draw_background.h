// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

typedef struct Background Background;

#ifdef __cplusplus
extern "C" {
#endif

Background *CreateBackground(const DrawCreateInfo *createInfo);
void        DestroyBackground(Background *background);
void        RenderBackground(Background *background, Camera *camera);

#ifdef __cplusplus
}
#endif
