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
#include "Utils.h"

namespace RTGL1
{
    struct ShVertex;

    // This class collects vertex and draw info for further rasterization.
    class RasterizedDataCollector final
    {
    public:
        struct DrawInfo
        {
            RgTransform                 transform = {};
            std::optional< Float16D >   viewProj  = std::nullopt;
            std::optional< VkViewport > viewport  = std::nullopt;

            uint32_t vertexCount = 0;
            uint32_t firstVertex = 0;
            uint32_t indexCount  = 0;
            uint32_t firstIndex  = 0;

            Float4D  color                = Float4D( NullifyToken );
            uint32_t textureIndex         = 0;
            uint32_t emissionTextureIndex = 0;

            RgRasterizedGeometryStateFlags pipelineState = 0;
            RgBlendFactor                  blendFuncSrc  = RG_BLEND_FACTOR_ONE;
            RgBlendFactor                  blendFuncDst  = RG_BLEND_FACTOR_ONE;
        };

    public:
        explicit RasterizedDataCollector( VkDevice                            device,
                                          std::shared_ptr< MemoryAllocator >& allocator,
                                          std::shared_ptr< TextureManager >   textureMgr,
                                          uint32_t                            maxVertexCount,
                                          uint32_t                            maxIndexCount );
        ~RasterizedDataCollector();

        RasterizedDataCollector( const RasterizedDataCollector& other )     = delete;
        RasterizedDataCollector( RasterizedDataCollector&& other ) noexcept = delete;
        RasterizedDataCollector& operator=( const RasterizedDataCollector& other ) = delete;

        RasterizedDataCollector& operator=( RasterizedDataCollector&& other ) noexcept = delete;
        void                     AddGeometry( uint32_t                              frameIndex,
                                              const RgRasterizedGeometryUploadInfo& info,
                                              const float*                          viewProjection,
                                              const RgViewport*                     viewport );

        void Clear( uint32_t frameIndex );

        void CopyFromStaging( VkCommandBuffer cmd, uint32_t frameIndex );

        VkBuffer GetVertexBuffer() const;
        VkBuffer GetIndexBuffer() const;

        static uint32_t GetVertexStride();
        static void     GetVertexLayout( VkVertexInputAttributeDescription* outAttrs,
                                         uint32_t*                          outAttrsCount );

        const std::vector< DrawInfo >& GetRasterDrawInfos() const;
        const std::vector< DrawInfo >& GetSwapchainDrawInfos() const;
        const std::vector< DrawInfo >& GetSkyDrawInfos() const;

    protected:
        DrawInfo& PushInfo( RgRasterizedGeometryRenderType renderType );

    private:
        static void CopyFromArrayOfStructs( const RgRasterizedGeometryUploadInfo& info,
                                            ShVertex*                             dstVerts );

    private:
        VkDevice                          device;
        std::shared_ptr< TextureManager > textureMgr;

        std::shared_ptr< AutoBuffer > vertexBuffer;
        std::shared_ptr< AutoBuffer > indexBuffer;

        uint64_t curVertexCount;
        uint64_t curIndexCount;

        std::vector< DrawInfo > rasterDrawInfos;
        std::vector< DrawInfo > swapchainDrawInfos;
        std::vector< DrawInfo > skyDrawInfos;
    };

}
