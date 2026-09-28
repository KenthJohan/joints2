#version 450 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in uint aWidgetIndex;

struct WidgetData
{
    vec4 color;
    vec4 clip;
    uint texture_layer;
};

layout(set=0, binding=0, std430) readonly buffer WidgetBuffer
{
    WidgetData widgets[];
} widget_buffer;

layout(set=1,binding=0) uniform UBO
{
    vec2 uScale;
    vec2 uTranslate;
} ubo;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec4 vClipRect;
layout(location = 2) out vec2 vUV;
layout(location = 3) flat out uint vTextureIndex;

void main()
{
    WidgetData widget = widget_buffer.widgets[aWidgetIndex];
    vColor = widget.color;
    vClipRect = widget.clip;
    vUV = aUV;
    vTextureIndex = widget.texture_layer;
    gl_Position = vec4(aPos * ubo.uScale + ubo.uTranslate, 0, 1);
    gl_Position.y *= -1.0f;
}
