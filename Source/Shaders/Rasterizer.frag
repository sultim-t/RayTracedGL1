#version 460

layout (location = 0) in vec4 color;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in flat uvec4 textureIds;

layout (location = 0) out vec4 outColor;

//layout (binding = , set = ) uniform sampler2D textures[];

void main()
{
    // TODO: get textures

    outColor = color;
}