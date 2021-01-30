#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT ShPayload payload;

void main()
{
    payload.color = vec4(0.85, 0.95, 1.0, 1.0);
}