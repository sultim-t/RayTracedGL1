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

RasterizedDataCollector::RasterizedDataCollector( VkDevice                            _device,
                                                  std::shared_ptr< MemoryAllocator >& _allocator,
                                                  std::shared_ptr< TextureManager >   _textureMgr,
                                                  uint32_t _maxVertexCount,
                                                  uint32_t _maxIndexCount )
    : device( _device )
    , textureMgr( std::move( _textureMgr ) )
    , curVertexCount( 0 )
    , curIndexCount( 0 )
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

namespace 
{
    bool IsWorld( RgRasterizedGeometryRenderType type )
    {
        return type == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT;
    }

    bool IsSwapchain( RgRasterizedGeometryRenderType type )
    {
        return type == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN;
    }

    bool IsSky( RgRasterizedGeometryRenderType type)
    {
        return type == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY;
    }

    VkViewport ToVk(const RgViewport &v)
    {
        return VkViewport{
            .x        = v.x,
            .y        = v.y,
            .width    = v.width,
            .height   = v.height,
            .minDepth = v.minDepth,
            .maxDepth = v.maxDepth,
        };
    }

    uint32_t ResolveTextureIndex_AlbedoAlpha( const RTGL1::TextureManager& manager,
                                              const RgRasterizedGeometryUploadInfo& info )
    {
        if( info.material == RG_NO_MATERIAL )
        {
            return EMPTY_TEXTURE_INDEX;
        }

        return manager.GetMaterialTextures( info.material )
            .indices[ MATERIAL_ALBEDO_ALPHA_INDEX ];
    }

    uint32_t ResolveTextureIndex_RME( const RTGL1::TextureManager&          manager,
                                              const RgRasterizedGeometryUploadInfo& info )
    {
        if( info.material == RG_NO_MATERIAL )
        {
            return EMPTY_TEXTURE_INDEX;
        }

        if( !IsWorld( info.renderType ) )
        {
            return EMPTY_TEXTURE_INDEX;
        }

        return manager.GetMaterialTextures( info.material )
            .indices[ MATERIAL_ROUGHNESS_METALLIC_EMISSION_INDEX ];
    }
}

void RasterizedDataCollector::AddGeometry(uint32_t frameIndex, 
                                          const RgRasterizedGeometryUploadInfo &info, 
                                          const float *pViewProjection, const RgViewport *pViewport)
{
    assert(info.vertexCount > 0);
    assert(info.pVertices != nullptr);

    // for swapchain, depth data is not available
    if( IsSwapchain( info.renderType ) )
    {
        if( info.pipelineState & RG_RASTERIZED_GEOMETRY_STATE_DEPTH_TEST )
        {
            assert( 0 );
            return;
        }

        if( info.pipelineState & RG_RASTERIZED_GEOMETRY_STATE_DEPTH_WRITE )
        {
            assert( 0 );
            return;
        }
    }

    // for sky, default pViewProjection and pViewport must be used,
    // as sky geometry can be updated not in each frame
    if( IsSky( info.renderType ) )
    {
        if( pViewProjection != nullptr || pViewport != nullptr )
        {
            throw RgException(RG_CANT_UPLOAD_RASTERIZED_GEOMETRY, "pViewProjection and pViewport must be null if renderType is RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY");
        }
    }

    if (curVertexCount + info.vertexCount >= vertexBuffer->GetSize() / sizeof(RgVertex))
    {
        assert(0 && "Increase the size of \"rasterizedMaxVertexCount\". Vertex buffer size reached the limit.");
        return;
    }

    if (curIndexCount + info.indexCount >= indexBuffer->GetSize() / sizeof(uint32_t))
    {
        assert(0 && "Increase the size of \"rasterizedMaxIndexCount\". Index buffer size reached the limit.");
        return;
    }


    DrawInfo &drawInfo = PushInfo(info.renderType);

    ShVertex* const vertsBase   = static_cast< ShVertex* >( vertexBuffer->GetMapped( frameIndex ) );
    uint32_t* const indicesBase = static_cast< uint32_t* >( indexBuffer->GetMapped( frameIndex ) );


    drawInfo = {
        .transform            = info.transform,
        .viewProj             = IfNotNull( pViewProjection, Float16D( pViewProjection ) ),
        .viewport             = IfNotNull( pViewport, ToVk( *pViewport ) ),
        .color                = Float4D( info.color.data ),
        .textureIndex         = ResolveTextureIndex_AlbedoAlpha( *textureMgr, info ),
        .emissionTextureIndex = ResolveTextureIndex_RME( *textureMgr, info ),
        .pipelineState        = info.pipelineState,
        .blendFuncSrc         = info.blendFuncSrc,
        .blendFuncDst         = info.blendFuncDst,
    };


    // copy vertex data
    CopyFromArrayOfStructs( info, &vertsBase[ curVertexCount ] );

    drawInfo.vertexCount = info.vertexCount;
    drawInfo.firstVertex  = static_cast< uint32_t >( curVertexCount );
    curVertexCount += info.vertexCount;


    // copy index data
    if( info.indexCount != 0 && info.pIndices != nullptr )
    {
        if( curIndexCount + info.indexCount >= indexBuffer->GetSize() / sizeof( uint32_t ) )
        {
            assert( 0 );
            return;
        }

        memcpy(
            &indicesBase[ curIndexCount ], info.pIndices, info.indexCount * sizeof( uint32_t ) );

        drawInfo.indexCount   = info.indexCount;
        drawInfo.firstIndex = static_cast< uint32_t >( curIndexCount );

        curIndexCount += info.indexCount;
    }
}

RasterizedDataCollector::DrawInfo& RasterizedDataCollector::PushInfo(
    RgRasterizedGeometryRenderType renderType )
{
    if( renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT )
    {
        return rasterDrawInfos.emplace_back();
    }

    if( renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN )
    {
        return swapchainDrawInfos.emplace_back();
    }

    if( renderType == RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY )
    {
        return skyDrawInfos.emplace_back();
    }

    throw RgException( RG_GRAPHICS_API_ERROR, "RasterizedDataCollector::PushInfo error" );
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
    rasterDrawInfos.clear();
    swapchainDrawInfos.clear();
    skyDrawInfos.clear();

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

const std::vector< RasterizedDataCollector::DrawInfo >& RasterizedDataCollector::
    GetRasterDrawInfos() const
{
    return rasterDrawInfos;
}

const std::vector< RasterizedDataCollector::DrawInfo >& RasterizedDataCollector::
    GetSwapchainDrawInfos() const
{
    return swapchainDrawInfos;
}

const std::vector< RasterizedDataCollector::DrawInfo >& RasterizedDataCollector::
    GetSkyDrawInfos() const
{
    return skyDrawInfos;
}
