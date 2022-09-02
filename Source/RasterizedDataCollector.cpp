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

#include <algorithm>

#include "Utils.h"
#include "RgException.h"
#include "Generated/ShaderCommonC.h"

using namespace RTGL1;

void RasterizedDataCollector::GetVertexLayout(VkVertexInputAttributeDescription *outAttrs, uint32_t *outAttrsCount)
{
    *outAttrsCount = 3;

    outAttrs[0].binding = 0;
    outAttrs[0].location = 0;
    outAttrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    outAttrs[0].offset = offsetof(RgVertex, position);

    outAttrs[1].binding = 0;
    outAttrs[1].location = 1;
    outAttrs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
    outAttrs[1].offset = offsetof(RgVertex, packedColor);

    outAttrs[2].binding = 0;
    outAttrs[2].location = 2;
    outAttrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    outAttrs[2].offset = offsetof(RgVertex, texCoord);
}

uint32_t RasterizedDataCollector::GetVertexStride()
{
    return static_cast<uint32_t>(sizeof(RgVertex));
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
    vertexBuffer = std::make_shared<AutoBuffer>(_device, _allocator);
    indexBuffer = std::make_shared<AutoBuffer>(_device, _allocator);

    _maxVertexCount = std::max(_maxVertexCount, 64u);
    _maxIndexCount = std::max(_maxIndexCount, 64u);

    vertexBuffer->Create(_maxVertexCount * sizeof(RgVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "Rasterizer vertex buffer");
    indexBuffer->Create(_maxIndexCount * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "Rasterizer index buffer");
}

RasterizedDataCollector::~RasterizedDataCollector()
{}

void RasterizedDataCollector::AddGeometry(uint32_t frameIndex, 
                                          const RgRasterizedGeometryUploadInfo &info, 
                                          const float *pViewProjection, const RgViewport *pViewport)
{
    assert(info.vertexCount > 0);
    assert(info.pVertices != nullptr);

    const bool renderToSwapchain = info.renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN;
    const bool renderToSky = info.renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY;

    // if renderToSwapchain, depth data is not available
    if (renderToSwapchain)
    {
        assert(!(info.pipelineState & RG_RASTERIZED_GEOMETRY_STATE_DEPTH_TEST));
        assert(!(info.pipelineState & RG_RASTERIZED_GEOMETRY_STATE_DEPTH_WRITE));
    }

    // if renderToSky, default pViewProjection and pViewport are used,
    // as sky geometry can be updated not in each frame
    if (renderToSky && (pViewProjection != nullptr || pViewport != nullptr))
    {
        throw RgException(RG_CANT_UPLOAD_RASTERIZED_GEOMETRY, "pViewProjection and pViewport must be null if renderType is RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY");
    }

    if ((uint64_t)curVertexCount + info.vertexCount >= vertexBuffer->GetSize() / sizeof(RgVertex))
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
    drawInfo.pipelineState = info.pipelineState;
    drawInfo.blendFuncSrc = info.blendFuncSrc;
    drawInfo.blendFuncDst = info.blendFuncDst;

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


    drawInfo.textureIndex = EMPTY_TEXTURE_INDEX;
    drawInfo.emissionTextureIndex = EMPTY_TEXTURE_INDEX;

    // copy texture indices
    if (const auto mgr = textureMgr.lock())
    {
        // get only the first (albedo-alpha) texture index from texture manager
        // and ignore roughness, metallic, etc
        drawInfo.textureIndex = mgr->GetMaterialTextures(info.material).indices[MATERIAL_ALBEDO_ALPHA_INDEX];

        if (!renderToSky && !renderToSwapchain)
        {
            drawInfo.emissionTextureIndex = mgr->GetMaterialTextures(info.material).indices[MATERIAL_ROUGHNESS_METALLIC_EMISSION_INDEX];
        }
    }


    // copy vertex data
    ShVertex *dstVerts = static_cast<ShVertex *>(vertexBuffer->GetMapped(frameIndex)) + curVertexCount;
    CopyFromArrayOfStructs(info, dstVerts);

    drawInfo.vertexCount = info.vertexCount;
    drawInfo.firstVertex = curVertexCount;
    curVertexCount += info.vertexCount;


    // copy index data
    bool useIndices = info.indexCount != 0 && info.pIndices != nullptr;
    if (useIndices)
    {
        if ((uint64_t)curIndexCount + info.indexCount >= indexBuffer->GetSize() / sizeof(uint32_t))
        {
            assert(0);
            return;
        }

        uint32_t *dstIndices = (uint32_t*)indexBuffer->GetMapped(frameIndex) + curIndexCount;
        memcpy(dstIndices, info.pIndices, info.indexCount * sizeof(uint32_t));

        drawInfo.indexCount = info.indexCount;
        drawInfo.firstIndex = curIndexCount;

        curIndexCount += info.indexCount;
    }
}

void RasterizedDataCollector::CopyFromArrayOfStructs(const RgRasterizedGeometryUploadInfo &info, ShVertex *dstVerts)
{
    assert(info.pVertices != nullptr);

    // must be same to copy
    static_assert(std::is_same_v<decltype(info.pVertices), const RgVertex * >);
    static_assert(sizeof(ShVertex)                      == sizeof(RgVertex));
    static_assert(offsetof(ShVertex, position)          == offsetof(RgVertex, position));
    static_assert(offsetof(ShVertex, normal)            == offsetof(RgVertex, normal));
    static_assert(offsetof(ShVertex, texCoord)          == offsetof(RgVertex, texCoord));
    static_assert(offsetof(ShVertex, texCoordLayer1)    == offsetof(RgVertex, texCoordLayer1));
    static_assert(offsetof(ShVertex, texCoordLayer2)    == offsetof(RgVertex, texCoordLayer2));
    static_assert(offsetof(ShVertex, packedColor)       == offsetof(RgVertex, packedColor));

    memcpy(dstVerts, info.pVertices, sizeof(RgVertex) * info.vertexCount);
}

void RasterizedDataCollector::Clear(uint32_t frameIndex)
{
    curVertexCount = 0;
    curIndexCount = 0;
}

void RasterizedDataCollector::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex)
{
    vertexBuffer->CopyFromStaging(cmd, frameIndex, sizeof(RgVertex) * curVertexCount);
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

RasterizedDataCollector::DrawInfo *RasterizedDataCollectorGeneral::PushInfo(RgRasterizedGeometryRenderType renderType)
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

RasterizedDataCollector::DrawInfo *RasterizedDataCollectorSky::PushInfo(RgRasterizedGeometryRenderType renderType)
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
