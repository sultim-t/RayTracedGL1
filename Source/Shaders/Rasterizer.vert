#version 460

layout (location = 0) in vec3 position;
layout (location = 1) in uint color;
layout (location = 2) in uvec4 textureIds;
layout (location = 3) in vec2 texCoord;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outTexCoord;
layout (location = 2) flat out uvec4 outTextureIds;

vec4 unpackUintColor(uint c)
{
    return vec4(
        ((c & 0xFF000000) >> 24) / 255.0,
        ((c & 0x00FF0000) >> 16) / 255.0,
        ((c & 0x0000FF00) >> 8) / 255.0,
        (c & 0x000000FF) / 255.0
    );
}

void main()
{
    outColor = unpackUintColor(color);
    outTexCoord = texCoord;
    outTextureIds = textureIds;
    gl_Position = vec4(position, 1.0);
}
