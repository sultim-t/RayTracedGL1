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

#pragma once

#include <vector>

#include <RTGL1/RTGL1.h>
#include "AutoBuffer.h"
#include "Common.h"
#include "TextureManager.h"

namespace RTGL1
{

// This class collects vertex and draw info for further rasterization.
class RasterizedDataCollector
{
public:
    struct DrawInfo
    {
        float       viewProj[16];
        RgTransform transform;
        bool        isDefaultViewProjMatrix;

        VkViewport  viewport;
        bool        isDefaultViewport;

        uint32_t    vertexCount;
        uint32_t    firstVertex;
        uint32_t    indexCount;
        uint32_t    firstIndex;

        float       color[4];
        uint32_t    textureIndex;

        bool            blendEnable;
        RgBlendFactor   blendFuncSrc;
        RgBlendFactor   blendFuncDst;
        bool            depthTest;
        bool            depthWrite;
    };

public:
    explicit RasterizedDataCollector(
        VkDevice device, 
        const std::shared_ptr<MemoryAllocator> &allocator,
        std::shared_ptr<TextureManager> textureMgr,
        uint32_t maxVertexCount, uint32_t maxIndexCount);
    virtual ~RasterizedDataCollector() = 0;

    RasterizedDataCollector(const RasterizedDataCollector& other) = delete;
    RasterizedDataCollector(RasterizedDataCollector&& other) noexcept = delete;
    RasterizedDataCollector& operator=(const RasterizedDataCollector& other) = delete;
    RasterizedDataCollector& operator=(RasterizedDataCollector&& other) noexcept = delete;

    virtual bool TryAddGeometry(uint32_t frameIndex,
                                const RgRasterizedGeometryUploadInfo &info, 
                                const float *viewProjection, const RgViewport *viewport) = 0;
    virtual void Clear(uint32_t frameIndex);

    void CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex);

    VkBuffer GetVertexBuffer() const;
    VkBuffer GetIndexBuffer() const;

    static uint32_t GetVertexStride();
    static void GetVertexLayout(VkVertexInputAttributeDescription *outAttrs, uint32_t *outAttrsCount);

protected:
    void AddGeometry(uint32_t frameIndex,
                     const RgRasterizedGeometryUploadInfo &info, 
                     const float *viewProjection, const RgViewport *viewport);

    virtual DrawInfo *PushInfo(RgRaterizedGeometryRenderType renderType) = 0;

private:
    struct RasterizerVertex;

private:
    static void CopyFromSeparateArrays(const RgRasterizedGeometryUploadInfo &info, RasterizerVertex *dstVerts);
    static void CopyFromArrayOfStructs(const RgRasterizedGeometryUploadInfo &info, RasterizerVertex *dstVerts);

private:
    VkDevice device;
    std::weak_ptr<TextureManager> textureMgr;

    std::shared_ptr<AutoBuffer> vertexBuffer;
    std::shared_ptr<AutoBuffer> indexBuffer;

    uint32_t curVertexCount;
    uint32_t curIndexCount;
};



// Collects data for world geometry
class RasterizedDataCollectorGeneral final : public RasterizedDataCollector
{
public:
    RasterizedDataCollectorGeneral(VkDevice device, const std::shared_ptr<MemoryAllocator> &allocator,
                                   const std::shared_ptr<TextureManager> &textureMgr, uint32_t maxVertexCount,
                                   uint32_t maxIndexCount);

    RasterizedDataCollectorGeneral(const RasterizedDataCollectorGeneral &other) = delete;
    RasterizedDataCollectorGeneral(RasterizedDataCollectorGeneral &&other) noexcept = delete;
    RasterizedDataCollectorGeneral &operator=(const RasterizedDataCollectorGeneral &other) = delete;
    RasterizedDataCollectorGeneral &operator=(RasterizedDataCollectorGeneral &&other) noexcept = delete;

    ~RasterizedDataCollectorGeneral() override = default;

    bool TryAddGeometry(uint32_t frameIndex,
                        const RgRasterizedGeometryUploadInfo &info, 
                        const float *viewProjection, const RgViewport *viewport) override;
    void Clear(uint32_t frameIndex) override;

    const std::vector<DrawInfo> &GetRasterDrawInfos() const;
    const std::vector<DrawInfo> &GetSwapchainDrawInfos() const;

protected:
    DrawInfo *PushInfo(RgRaterizedGeometryRenderType renderType) override;

private:
    std::vector<DrawInfo> rasterDrawInfos;
    std::vector<DrawInfo> swapchainDrawInfos;
};



class RasterizedDataCollectorSky final : public RasterizedDataCollector
{
public:
    RasterizedDataCollectorSky(VkDevice device, const std::shared_ptr<MemoryAllocator> &allocator,
                               const std::shared_ptr<TextureManager> &textureMgr, uint32_t maxVertexCount,
                               uint32_t maxIndexCount);

    RasterizedDataCollectorSky(const RasterizedDataCollectorSky &other) = delete;
    RasterizedDataCollectorSky(RasterizedDataCollectorSky &&other) noexcept = delete;
    RasterizedDataCollectorSky & operator=(const RasterizedDataCollectorSky &other) = delete;
    RasterizedDataCollectorSky & operator=(RasterizedDataCollectorSky &&other) noexcept = delete;

    ~RasterizedDataCollectorSky() override = default;

    bool TryAddGeometry(uint32_t frameIndex,
                        const RgRasterizedGeometryUploadInfo &info, 
                        const float *viewProjection, const RgViewport *viewport) override;
    void Clear(uint32_t frameIndex) override;

    const std::vector<DrawInfo> &GetSkyDrawInfos() const;

protected:
    DrawInfo *PushInfo(RgRaterizedGeometryRenderType renderType) override;

private:
    std::vector<DrawInfo> skyDrawInfos;
};

}