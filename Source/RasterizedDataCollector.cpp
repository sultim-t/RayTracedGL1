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

#include "Utils.h"

using namespace RTGL1;

struct RasterizedDataCollector::RasterizerVertex
{
    float       position[3];
    uint32_t    color;
    float       texCoord[2];
};

void RasterizedDataCollector::GetVertexLayout(VkVertexInputAttributeDescription *outAttrs, uint32_t *outAttrsCount)
{
    *outAttrsCount = 3;

    outAttrs[0].binding = 0;
    outAttrs[0].location = 0;
    outAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    outAttrs[0].offset = offsetof(RasterizerVertex, position);

    outAttrs[1].binding = 0;
    outAttrs[1].location = 1;
    outAttrs[1].format = VK_FORMAT_R32_UINT;
    outAttrs[1].offset = offsetof(RasterizerVertex, color);

    outAttrs[2].binding = 0;
    outAttrs[2].location = 2;
    outAttrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    outAttrs[2].offset = offsetof(RasterizerVertex, texCoord);
}

uint32_t RasterizedDataCollector::GetVertexStride()
{
    return static_cast<uint32_t>(sizeof(RasterizerVertex));
}

RasterizedDataCollector::RasterizedDataCollector(
    VkDevice _device,
    const std::shared_ptr<MemoryAllocator> &_allocator,
    std::shared_ptr<TextureManager> _textureMgr,
    uint32_t _maxVertexCount, uint32_t _maxIndexCount)
:
    device(_device),
    textureMgr(_textureMgr),
    curVertexCount(0),
    curIndexCount(0)
{
    vertexBuffer = std::make_shared<AutoBuffer>(_device, _allocator, "Rasterizer vertex buffer staging", "Rasterizer vertex buffer");
    indexBuffer = std::make_shared<AutoBuffer>(_device, _allocator, "Rasterizer index buffer staging", "Rasterizer index buffer");

    _maxVertexCount = std::max(_maxVertexCount, 64u);
    _maxIndexCount = std::max(_maxIndexCount, 64u);

    vertexBuffer->Create(_maxVertexCount * sizeof(RasterizerVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indexBuffer->Create(_maxIndexCount * sizeof(RasterizerVertex), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

RasterizedDataCollector::~RasterizedDataCollector()
{}

void RasterizedDataCollector::AddGeometry(uint32_t frameIndex, 
                                          const RgRasterizedGeometryUploadInfo &info, 
                                          const float *pViewProjection, const RgViewport *pViewport)
{
    assert(info.vertexCount > 0);

    assert((info.structs != nullptr && info.arrays == nullptr) ||
           (info.structs == nullptr && info.arrays != nullptr));

    const bool renderToSwapchain = info.renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN;
    const bool renderToSky = info.renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY;

    // if renderToSwapchain, depth data is not available
    if (renderToSwapchain)
    {
        assert(!info.depthTest && !info.depthWrite);
    }

    // if renderToSky, default pViewProjection and pViewport should be used,
    // as sky geometry can be updated not in each frame
    if (renderToSky)
    {
        assert(pViewProjection == nullptr && pViewport == nullptr);
        pViewProjection = nullptr;
        pViewport = nullptr;
    }

    if (info.arrays != nullptr)
    {
        assert(info.arrays->vertexStride >= 3 * sizeof(float));
        assert(info.arrays->colorData == nullptr || info.arrays->colorStride >= sizeof(uint32_t));
        assert(info.arrays->texCoordData == nullptr || info.arrays->texCoordStride >= 2 * sizeof(float));
    }

    if ((uint64_t)curVertexCount + info.vertexCount >= vertexBuffer->GetSize() / sizeof(RasterizerVertex))
    {
        assert(0 && "Increase the size of \"rasterizedMaxVertexCount\". Vertex buffer size reached the limit.");
        return;
    }

    if ((uint64_t)curIndexCount + info.indexCount >= indexBuffer->GetSize() / sizeof(uint32_t))
    {
        assert(0 && "Increase the size of \"rasterizedMaxIndexCount\". Index buffer size reached the limit.");
        return;
    }


    DrawInfo *pDrawInfo = PushInfo(info.renderType);

    if (pDrawInfo == nullptr)
    {
        return;
    }

    DrawInfo &drawInfo = *pDrawInfo;

    drawInfo.isDefaultViewport = pViewport == nullptr;
    drawInfo.isDefaultViewProjMatrix = pViewProjection == nullptr;
    drawInfo.transform = info.transform;
    drawInfo.blendEnable = info.blendEnable;
    drawInfo.blendFuncSrc = info.blendFuncSrc;
    drawInfo.blendFuncDst = info.blendFuncDst;
    drawInfo.depthTest = info.depthTest;
    drawInfo.depthWrite = info.depthWrite;

    if (pViewport != nullptr)
    {
        drawInfo.viewport.x = pViewport->x;
        drawInfo.viewport.y = pViewport->y;
        drawInfo.viewport.width = pViewport->width;
        drawInfo.viewport.height = pViewport->height;
        drawInfo.viewport.minDepth = pViewport->minDepth;
        drawInfo.viewport.maxDepth = pViewport->maxDepth;
    }

    if (pViewProjection != nullptr)
    {
        memcpy(drawInfo.viewProj, pViewProjection, 16 * sizeof(float));
    }

    memcpy(drawInfo.color, info.color.data, 4 * sizeof(float));


    // copy texture indices
    if (const auto mgr = textureMgr.lock())
    {
        // get albedo-alpha texture index from texture manager
        drawInfo.textureIndex = mgr->GetMaterialTextures(info.material).albedoAlpha;
    }
    else
    {
        drawInfo.textureIndex = EMPTY_TEXTURE_INDEX;
    }


    // copy vertex data
    RasterizerVertex *dstVerts = (RasterizerVertex*)vertexBuffer->GetMapped(frameIndex) + curVertexCount;

    if (info.arrays != nullptr)
    {
        CopyFromSeparateArrays(info, dstVerts);
    }
    else
    {
        CopyFromArrayOfStructs(info, dstVerts);
    }

    drawInfo.vertexCount = info.vertexCount;
    drawInfo.firstVertex = curVertexCount;
    curVertexCount += info.vertexCount;


    // copy index data
    bool useIndices = info.indexCount != 0 && info.indexData != nullptr;
    if (useIndices)
    {
        if ((uint64_t)curIndexCount + info.indexCount >= indexBuffer->GetSize() / sizeof(uint32_t))
        {
            assert(0);
            return;
        }

        uint32_t *dstIndices = (uint32_t*)indexBuffer->GetMapped(frameIndex) + curIndexCount;
        memcpy(dstIndices, info.indexData, info.indexCount * sizeof(uint32_t));

        drawInfo.indexCount = info.indexCount;
        drawInfo.firstIndex = curIndexCount;

        curIndexCount += info.indexCount;
    }
}

void RasterizedDataCollector::CopyFromSeparateArrays(const RgRasterizedGeometryUploadInfo &info, RasterizerVertex *dstVerts)
{
    assert(info.arrays != nullptr);

    const auto &src = *info.arrays;

    for (uint32_t i = 0; i < info.vertexCount; i++)
    {
        auto *srcPos        = (float*)      ((uint8_t*)src.vertexData      + (uint64_t)i * src.vertexStride);
        auto *srcColor      = (uint32_t*)   ((uint8_t*)src.colorData       + (uint64_t)i * src.colorStride);
        auto *srcTexCoord   = (float*)      ((uint8_t*)src.texCoordData    + (uint64_t)i * src.texCoordStride);

        RasterizerVertex vert = {};

        vert.position[0] = srcPos[0];
        vert.position[1] = srcPos[1];
        vert.position[2] = srcPos[2];

        vert.color = src.colorData ? *srcColor : UINT32_MAX;

        vert.texCoord[0] = src.texCoordData ? srcTexCoord[0] : 0;
        vert.texCoord[1] = src.texCoordData ? srcTexCoord[1] : 0;

        // write to mapped memory
        memcpy(dstVerts + i, &vert, sizeof(RasterizerVertex));
    }
}

void RasterizedDataCollector::CopyFromArrayOfStructs(const RgRasterizedGeometryUploadInfo &info, RasterizerVertex *dstVerts)
{
    assert(info.structs != nullptr);

    static_assert(sizeof(RgRasterizedGeometryVertexStruct) == sizeof(RasterizerVertex), "");
    static_assert(offsetof(RgRasterizedGeometryVertexStruct, position) == offsetof(RasterizerVertex, position), "");
    static_assert(offsetof(RgRasterizedGeometryVertexStruct, packedColor) == offsetof(RasterizerVertex, color), "");
    static_assert(offsetof(RgRasterizedGeometryVertexStruct, texCoord) == offsetof(RasterizerVertex, texCoord), "");

    memcpy(dstVerts, info.structs, sizeof(RasterizerVertex) * info.vertexCount);
}

void RasterizedDataCollector::Clear(uint32_t frameIndex)
{
    curVertexCount = 0;
    curIndexCount = 0;
}

void RasterizedDataCollector::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex)
{
    vertexBuffer->CopyFromStaging(cmd, frameIndex, sizeof(RasterizerVertex) * curVertexCount);
    indexBuffer->CopyFromStaging(cmd, frameIndex, sizeof(uint32_t) * curIndexCount);
}

VkBuffer RasterizedDataCollector::GetVertexBuffer() const
{
    return vertexBuffer->GetDeviceLocal();
}

VkBuffer RasterizedDataCollector::GetIndexBuffer() const
{
    return indexBuffer->GetDeviceLocal();
}



RasterizedDataCollectorGeneral::RasterizedDataCollectorGeneral(
    VkDevice device, const std::shared_ptr<MemoryAllocator> &allocator, 
    const std::shared_ptr<TextureManager> &textureMgr, uint32_t maxVertexCount, uint32_t maxIndexCount)
:
    RasterizedDataCollector(device, allocator, textureMgr, maxVertexCount, maxIndexCount) {}

bool RasterizedDataCollectorGeneral::TryAddGeometry(uint32_t frameIndex, const RgRasterizedGeometryUploadInfo &info,
    const float *viewProjection, const RgViewport *viewport)
{
    if (info.renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT ||
        info.renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN)
    {
        AddGeometry(frameIndex, info, viewProjection, viewport);
        return true;
    }

    return false;
}

void RasterizedDataCollectorGeneral::Clear(uint32_t frameIndex)
{
    rasterDrawInfos.clear();
    swapchainDrawInfos.clear();

    RasterizedDataCollector::Clear(frameIndex);
}

const std::vector<RasterizedDataCollector::DrawInfo> &RasterizedDataCollectorGeneral::GetRasterDrawInfos() const
{
    return rasterDrawInfos;
}

const std::vector<RasterizedDataCollector::DrawInfo> &RasterizedDataCollectorGeneral::GetSwapchainDrawInfos() const
{
    return swapchainDrawInfos;
}

RasterizedDataCollector::DrawInfo *RasterizedDataCollectorGeneral::PushInfo(RgRaterizedGeometryRenderType renderType)
{
    if (renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT)
    {
        rasterDrawInfos.emplace_back();
        return &rasterDrawInfos.back();
    }
    else if (renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN)
    {
        swapchainDrawInfos.emplace_back();
        return &swapchainDrawInfos.back();
    }

    assert(0);
    return nullptr;
}



RasterizedDataCollectorSky::RasterizedDataCollectorSky(
    VkDevice device, const std::shared_ptr<MemoryAllocator> &allocator, 
    const std::shared_ptr<TextureManager> &textureMgr, uint32_t maxVertexCount, uint32_t maxIndexCount)
:
    RasterizedDataCollector(device, allocator, textureMgr, maxVertexCount, maxIndexCount) {}

bool RasterizedDataCollectorSky::TryAddGeometry(uint32_t frameIndex, const RgRasterizedGeometryUploadInfo &info,
    const float *viewProjection, const RgViewport *viewport)
{
    if (info.renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY)
    {
        AddGeometry(frameIndex, info, viewProjection, viewport);
        return true;
    }

    return false;
}

void RasterizedDataCollectorSky::Clear(uint32_t frameIndex)
{
    skyDrawInfos.clear();

    RasterizedDataCollector::Clear(frameIndex);
}

const std::vector<RasterizedDataCollector::DrawInfo> & RasterizedDataCollectorSky::GetSkyDrawInfos() const
{
    return skyDrawInfos;
}

RasterizedDataCollector::DrawInfo *RasterizedDataCollectorSky::PushInfo(RgRaterizedGeometryRenderType renderType)
{
    if (renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY)
    {
        // if renderToSky, default pViewProjection and pViewport should be used,
        // as sky geometry can be updated not in each frame

        skyDrawInfos.emplace_back();
        return &skyDrawInfos.back();
    }

    assert(0);
    return nullptr;
}
