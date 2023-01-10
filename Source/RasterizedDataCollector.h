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
enum class GeometryRasterType
{
    WORLD,
    SKY,
    SWAPCHAIN,
};

enum class PipelineStateFlagBits
{
    ALPHA_TEST    = 0b000001,
    TRANSLUCENT   = 0b000010,
    ADDITIVE      = 0b000100,
    DEPTH_TEST    = 0b001000,
    DEPTH_WRITE   = 0b010000,
    DRAW_AS_LINES = 0b100000,
};
using PipelineStateFlags = uint32_t;

inline PipelineStateFlags operator|( PipelineStateFlagBits a, PipelineStateFlagBits b )
{
    return static_cast< PipelineStateFlags >( a ) | static_cast< PipelineStateFlags >( b );
}
inline PipelineStateFlags operator|( PipelineStateFlags a, PipelineStateFlagBits b )
{
    return a | static_cast< PipelineStateFlags >( b );
}
inline bool operator&( PipelineStateFlags a, PipelineStateFlagBits b )
{
    return !!( a & static_cast< PipelineStateFlags >( b ) );
}



// This class collects vertex and draw info for further rasterization.
class RasterizedDataCollector final
{
public:
    struct DrawInfo
    {
        RgTransform                 transform = {};
        uint32_t                    flags     = 0;

        uint32_t                    texture_base     = EMPTY_TEXTURE_INDEX;
        uint32_t                    texture_base_ORM = EMPTY_TEXTURE_INDEX;
        uint32_t                    texture_base_N   = EMPTY_TEXTURE_INDEX;
        uint32_t                    texture_base_E   = EMPTY_TEXTURE_INDEX;

        uint32_t                    texture_layer1   = EMPTY_TEXTURE_INDEX;
        uint32_t                    texture_layer2   = EMPTY_TEXTURE_INDEX;
        uint32_t                    texture_lightmap = EMPTY_TEXTURE_INDEX;

        RgColor4DPacked32           colorFactor_base     = Utils::PackColor( 255, 255, 255, 255 );
        RgColor4DPacked32           colorFactor_layer1   = Utils::PackColor( 255, 255, 255, 255 );
        RgColor4DPacked32           colorFactor_layer2   = Utils::PackColor( 255, 255, 255, 255 );
        RgColor4DPacked32           colorFactor_lightmap = Utils::PackColor( 255, 255, 255, 255 );

        uint32_t                    vertexCount = 0;
        uint32_t                    firstVertex = 0;
        uint32_t                    indexCount  = 0;
        uint32_t                    firstIndex  = 0;

        float                       roughnessFactor = 1.0f;
        float                       metallicFactor  = 0.0f;

        float                       emissive = 0.0f;

        // Raster-specific
        std::optional< Float16D >   viewProj      = std::nullopt;
        std::optional< VkViewport > viewport      = std::nullopt;
        PipelineStateFlags          pipelineState = 0;
    };

public:
    explicit RasterizedDataCollector( VkDevice                           device,
                                      std::shared_ptr< MemoryAllocator > allocator,
                                      std::shared_ptr< TextureManager >  textureMgr,
                                      uint32_t                           maxVertexCount,
                                      uint32_t                           maxIndexCount );
    ~RasterizedDataCollector() = default;

    RasterizedDataCollector( const RasterizedDataCollector& other )     = delete;
    RasterizedDataCollector( RasterizedDataCollector&& other ) noexcept = delete;
    RasterizedDataCollector& operator=( const RasterizedDataCollector& other ) = delete;
    RasterizedDataCollector& operator=( RasterizedDataCollector&& other ) noexcept = delete;

    void                     AddPrimitive( uint32_t                   frameIndex,
                                           GeometryRasterType         rasterType,
                                           const RgTransform&         transform,
                                           const RgMeshPrimitiveInfo& info,
                                           const float*               pViewProjection,
                                           const RgViewport*          pViewport );

    void                     Clear( uint32_t frameIndex );

    void                     CopyFromStaging( VkCommandBuffer cmd, uint32_t frameIndex );

    [[nodiscard]] VkBuffer   GetVertexBuffer() const;
    [[nodiscard]] VkBuffer   GetIndexBuffer() const;

    static uint32_t          GetVertexStride();
    static std::array< VkVertexInputAttributeDescription, 3 > GetVertexLayout();

    const std::vector< DrawInfo >&                            GetRasterDrawInfos() const;
    const std::vector< DrawInfo >&                            GetSwapchainDrawInfos() const;
    const std::vector< DrawInfo >&                            GetSkyDrawInfos() const;

protected:
    DrawInfo& PushInfo( GeometryRasterType rasterType );

private:
    VkDevice                          device;
    std::shared_ptr< TextureManager > textureMgr;

    std::shared_ptr< AutoBuffer >     vertexBuffer;
    std::shared_ptr< AutoBuffer >     indexBuffer;

    uint32_t                          curVertexCount;
    uint32_t                          curIndexCount;

    std::vector< DrawInfo >           rasterDrawInfos;
    std::vector< DrawInfo >           swapchainDrawInfos;
    std::vector< DrawInfo >           skyDrawInfos;
};

}
