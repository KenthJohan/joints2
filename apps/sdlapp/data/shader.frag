#version 450 core
layout(location = 0) out vec4 fColor;

layout(set=2, binding=0) uniform sampler2DArray sTextureArray;

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec4 vClipRect;
layout(location = 2) in vec2 vUV;
layout(location = 3) flat in uint vTextureIndex;

void main()
{
    if (gl_FragCoord.x < vClipRect.x || gl_FragCoord.y < vClipRect.y ||
        gl_FragCoord.x >= vClipRect.z || gl_FragCoord.y >= vClipRect.w)
    {
        discard;
    }
    fColor = vColor * texture(sTextureArray, vec3(vUV, float(vTextureIndex)));
}
