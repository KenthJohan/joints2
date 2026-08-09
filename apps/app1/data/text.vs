// SPDX-FileCopyrightText: 2026 Erin Catto
// SPDX-License-Identifier: MIT

#version 330

uniform mat4 projectionMatrix;

layout(location = 0) in vec2 v_position;
layout(location = 1) in vec4 v_instanceTransform;
layout(location = 2) in vec2 v_uv;
layout(location = 3) in vec4 v_color;

out vec2 f_uv;
out vec4 f_color;

void main(void)
{
    f_uv = v_uv;
    f_color = v_color;
    float x = v_instanceTransform.x;
    float y = v_instanceTransform.y;
    float c = v_instanceTransform.z;
    float s = v_instanceTransform.w;
    vec2 p = vec2(v_position.x, v_position.y);
    p = vec2((c * p.x + s * p.y) + x, (-s * p.x + c * p.y) + y);
    gl_Position = projectionMatrix * vec4(p, 0.0f, 1.0f);
}
