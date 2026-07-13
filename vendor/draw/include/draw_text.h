// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#include "draw_types.h"

typedef struct TextRender TextRender;

#ifdef __cplusplus
extern "C" {
#endif

TextRender *CreateTextRender(const DrawCreateInfo *createInfo);
void        DestroyTextRender(TextRender *render);
void        AddText(TextRender *render, float x, float y, float fontSize, b2HexColor color, const char *string);
void        FlushText(TextRender *render, const float *projectionMatrix);

#ifdef __cplusplus
}
#endif
