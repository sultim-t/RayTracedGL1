#version 460

layout (location = 0) in vec3 position;
layout (location = 1) in uint color;
layout (location = 2) in vec2 texCoord;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outTexCoord;

layout(push_constant) uniform RasterizerVert_BT 
{
    layout(offset = 0) mat4 viewProj;
} rasterizerVertInfo;

vec4 unpackUintColor(uint c)
{
    return vec4(
         (c & 0x000000FF)        / 255.0,
        ((c & 0x0000FF00) >> 8)  / 255.0,
        ((c & 0x00FF0000) >> 16) / 255.0,
        ((c & 0xFF000000) >> 24) / 255.0
    );
}

void main()
{
    outColor = unpackUintColor(color);
    outTexCoord = texCoord;
    gl_Position = rasterizerVertInfo.viewProj * vec4(position, 1.0);
}
