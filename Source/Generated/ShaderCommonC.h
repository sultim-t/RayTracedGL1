#pragma once
#include <stdint.h>

#define MAX_STATIC_VERTEX_COUNT (4194304)
#define MAX_DYNAMIC_VERTEX_COUNT (2097152)
#define MAX_VERTEX_COLLECTOR_INDEX_COUNT (4194304)
#define MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT (16384)
#define MAX_TOP_LEVEL_INSTANCE_COUNT (32)
#define BINDING_VERTEX_BUFFER_STATIC (0)
#define BINDING_VERTEX_BUFFER_DYNAMIC (1)
#define BINDING_INDEX_BUFFER_STATIC (2)
#define BINDING_INDEX_BUFFER_DYNAMIC (3)
#define BINDING_GEOMETRY_INSTANCES_STATIC (4)
#define BINDING_GEOMETRY_INSTANCES_DYNAMIC (5)
#define BINDING_GLOBAL_UNIFORM (0)
#define BINDING_ACCELERATION_STRUCTURE (0)
#define BINDING_STORAGE_IMAGE (0)
#define BINDING_TEXTURES (0)
#define INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC (1 << 0)

struct ShVertexBufferStatic
{
    float positions[12582912];
    float normals[12582912];
    float texCoords[8388608];
    uint32_t colors[4194304];
};

struct ShVertexBufferDynamic
{
    float positions[6291456];
    float normals[6291456];
    float texCoords[4194304];
    uint32_t colors[2097152];
};

struct ShGlobalUniform
{
    float view[16];
    float invView[16];
    float viewPrev[16];
    float projection[16];
    float invProjection[16];
    float projectionPrev[16];
    int32_t instanceGeomInfoOffset[32];
    uint32_t positionsStride;
    uint32_t normalsStride;
    uint32_t texCoordsStride;
    uint32_t colorsStride;
};

struct ShGeometryInstance
{
    float model[16];
    uint32_t materials[3][4];
    uint32_t baseVertexIndex;
    uint32_t baseIndexIndex;
    uint32_t primitiveCount;
    uint32_t __pad0;
};

