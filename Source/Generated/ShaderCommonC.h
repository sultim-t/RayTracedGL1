#pragma once
#include <stdint.h>

#define MAX_STATIC_VERTEX_COUNT (2097152)
#define MAX_DYNAMIC_VERTEX_COUNT (2097152)
#define MAX_VERTEX_COLLECTOR_INDEX_COUNT (4194304)
#define MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT (262144)
#define MAX_TOP_LEVEL_INSTANCE_COUNT (4096)
#define BINDING_VERTEX_BUFFER_STATIC (0)
#define BINDING_VERTEX_BUFFER_DYNAMIC (1)
#define BINDING_GLOBAL_UNIFORM (0)
#define BINDING_ACCELERATION_STRUCTURE (0)

struct ShVertexBufferStatic
{
    float positions[6291456];
    float normals[6291456];
    float texCoords[4194304];
    uint32_t colors[2097152];
    uint32_t materialIds[699052];
};

struct ShVertexBufferDynamic
{
    float positions[6291456];
    float normals[6291456];
    float texCoords[4194304];
    uint32_t colors[2097152];
    uint32_t materialIds[699052];
};

struct ShGlobalUniform
{
    float viewProj[16];
    float viewProjPrev[16];
    uint32_t positionsStride;
    uint32_t normalsStride;
    uint32_t texCoordsStride;
    uint32_t colorsStride;
};

