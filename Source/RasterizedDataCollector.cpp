// Copyright (c) 2020-2021 Sultim Tsyrendashiev
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "RasterizedDataCollector.h"

struct RasterizedDataCollector::RasterizerVertex
{
    float       position[3];
    uint32_t    color;
    uint32_t    textureIds[4];
    float       texCoord[2];
};

void RasterizedDataCollector::GetVertexLayout(std::array<VkVertexInputAttributeDescription, 4> &attrs)
{
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(RasterizerVertex, position);

    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32_UINT;
    attrs[1].offset = offsetof(RasterizerVertex, color);

    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32A32_UINT;
    attrs[2].offset = offsetof(RasterizerVertex, textureIds);

    attrs[3].binding = 0;
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = offsetof(RasterizerVertex, texCoord);
}

uint32_t RasterizedDataCollector::GetVertexStride()
{
    return static_cast<uint32_t>(sizeof(RasterizerVertex));
}

RasterizedDataCollector::RasterizedDataCollector(
    VkDevice device, 
    const std::shared_ptr<PhysicalDevice> &physDevice,
    uint32_t maxVertexCount, uint32_t maxIndexCount) :
    curVertexCount(0), curIndexCount(0)
{
    this->device = device;

    vertexBuffer.Init(
        device, *physDevice,
        maxVertexCount * sizeof(RasterizerVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    indexBuffer.Init(
        device, *physDevice,
        maxIndexCount * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    mappedVertexData = static_cast<RasterizerVertex*>(vertexBuffer.Map());
    mappedIndexData = static_cast<uint32_t*>(indexBuffer.Map());
}

RasterizedDataCollector::~RasterizedDataCollector()
{
    vertexBuffer.Unmap();
    indexBuffer.Unmap();
}

void RasterizedDataCollector::AddGeometry(const RgRasterizedGeometryUploadInfo &info)
{
    assert(info.vertexCount > 0);
    assert(info.vertexStride >= 3 * sizeof(float));
    assert(info.colorData == nullptr || info.colorStride >= sizeof(uint32_t));
    assert(info.texCoordData == nullptr || info.texCoordStride >= 2 * sizeof(float));

    if (curVertexCount + info.vertexCount >= vertexBuffer.GetSize() / sizeof(RasterizerVertex))
    {
        assert(0);
        return;
    }

    DrawInfo drawInfo = {};

    RasterizerVertex *dstVerts = mappedVertexData + curVertexCount;
    drawInfo.vertexCount = info.vertexCount;
    drawInfo.firstVertex = curVertexCount;

    curVertexCount += info.vertexCount;

    for (uint32_t i = 0; i < info.vertexCount; i++)
    {
        float *srcPos = (float *) ((uint8_t *) info.vertexData + info.vertexStride * i);
        uint32_t *srcColor = (uint32_t *) ((uint8_t *) info.colorData + info.colorStride * i);
        float *srcTexCoord = (float *) ((uint8_t *) info.texCoordData + info.texCoordStride * i);

        RasterizerVertex vert = {};

        vert.position[0] = srcPos[0];
        vert.position[1] = srcPos[1];
        vert.position[2] = srcPos[2];

        vert.color = info.colorData ? *srcColor : UINT32_MAX;

        vert.texCoord[0] = info.texCoordData ? srcTexCoord[0] : 0;
        vert.texCoord[1] = info.texCoordData ? srcTexCoord[1] : 0;

        for (uint32_t t = 0; t < info.textureCount; t++)
        {
            vert.textureIds[t] = info.textures[t].staticTexture;
        }
        for (uint32_t t = info.textureCount; t < 4; t++)
        {
            vert.textureIds[t] = 0;
        }

        // write to mapped memory
        dstVerts[i] = vert;
    }

    bool useIndices = info.indexCount != 0 && info.indexData != nullptr;
    if (useIndices)
    {
        if (curIndexCount + info.indexCount >= indexBuffer.GetSize() / sizeof(uint32_t))
        {
            assert(0);
            return;
        }

        uint32_t *dstIndices = mappedIndexData + curIndexCount;
        memcpy(dstIndices, info.indexData, info.indexCount * sizeof(uint32_t));

        drawInfo.indexCount = info.indexCount;
        drawInfo.firstIndex = curIndexCount;

        curIndexCount += info.indexCount;
    }

    drawInfos.push_back(drawInfo);
}

void RasterizedDataCollector::Clear()
{
    drawInfos.clear();
    curVertexCount = 0;
    curIndexCount = 0;
}

VkBuffer RasterizedDataCollector::GetVertexBuffer() const
{
    return vertexBuffer.GetBuffer();
}

VkBuffer RasterizedDataCollector::GetIndexBuffer() const
{
    return indexBuffer.GetBuffer();
}

const std::vector<RasterizedDataCollector::DrawInfo> &RasterizedDataCollector::GetDrawInfos() const
{
    return drawInfos;
}

