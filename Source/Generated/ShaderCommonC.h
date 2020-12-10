#pragma once

#define MAX_STATIC_VERTEX_COUNT (2097152)
#define MAX_DYNAMIC_VERTEX_COUNT (2097152)
#define BINDING_VERTEX_BUFFER_STATIC (0)
#define BINDING_VERTEX_BUFFER_DYNAMIC (1)
#define BINDING_GLOBAL_UNIFORM (0)

struct VertexBufferStatic
{
    float positions[6291456];
    float normals[6291456];
    float texCoords[4194304];
    uint32_t colors[2097152];
    uint32_t materialIds[699052];
};

struct VertexBufferDynamic
{
    float positions[6291456];
    float normals[6291456];
    float texCoords[4194304];
    uint32_t colors[2097152];
    uint32_t materialIds[699052];
};

struct GlobalUniform
{
    float viewProj[4][4];
    float viewProjPrev[4][4];
    uint32_t positionsStride;
    uint32_t normalsStride;
    uint32_t texCoordsStride;
    uint32_t colorsStride;
};

