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
#include "Buffer.h"
#include "Common.h"
#include "TextureManager.h"

namespace RTGL1
{

class RasterizedDataCollector
{
public:
    struct DrawInfo
    {
        float       viewProj[16];
        bool        isDefaultViewProjMatrix;
        VkViewport  viewport;
        bool        isDefaultViewport;
        uint32_t    vertexCount;
        uint32_t    firstVertex;
        uint32_t    indexCount;
        uint32_t    firstIndex;
        uint32_t    textureIndex;
    };

public:
    explicit RasterizedDataCollector(
        VkDevice device, 
        const std::shared_ptr<MemoryAllocator> &allocator,
        std::shared_ptr<TextureManager> textureMgr,
        uint32_t maxVertexCount, uint32_t maxIndexCount);
    ~RasterizedDataCollector();

    RasterizedDataCollector(const RasterizedDataCollector& other) = delete;
    RasterizedDataCollector(RasterizedDataCollector&& other) noexcept = delete;
    RasterizedDataCollector& operator=(const RasterizedDataCollector& other) = delete;
    RasterizedDataCollector& operator=(RasterizedDataCollector&& other) noexcept = delete;

    void AddGeometry(const RgRasterizedGeometryUploadInfo &info, 
                     const float *viewProjection, const RgViewport *viewport);
    void Clear();

    VkBuffer GetVertexBuffer() const;
    VkBuffer GetIndexBuffer() const;
    const std::vector<DrawInfo> &GetDrawInfos() const;

    static uint32_t GetVertexStride();
    static void GetVertexLayout(VkVertexInputAttributeDescription *outAttrs, uint32_t *outAttrsCount);

private:
    struct RasterizerVertex;

private:
    static void CopyFromSeparateArrays(const RgRasterizedGeometryUploadInfo &info, RasterizerVertex *dstVerts);
    static void CopyFromArrayOfStructs(const RgRasterizedGeometryUploadInfo &info, RasterizerVertex *dstVerts);

private:
    VkDevice device;
    std::weak_ptr<TextureManager> textureMgr;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    RasterizerVertex *mappedVertexData;
    uint32_t *mappedIndexData;

    uint32_t curVertexCount;
    uint32_t curIndexCount;

    std::vector<DrawInfo> drawInfos;
};

}