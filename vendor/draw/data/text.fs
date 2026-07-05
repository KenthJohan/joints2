// SPDX-FileCopyrightText: 2026 Erin Catto
// SPDX-License-Identifier: MIT

#version 330

uniform sampler2D glyphAtlas;

in vec2 f_uv;
in vec4 f_color;
out vec4 color;

void main(void)
{
    float alpha = texture(glyphAtlas, f_uv).r;
    color = vec4(f_color.rgb, f_color.a * alpha);
}
