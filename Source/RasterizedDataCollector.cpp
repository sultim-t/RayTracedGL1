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

#include "GeomInfoManager.h"
#include "RgException.h"
#include "Utils.h"

#include "Generated/ShaderCommonC.h"

std::array< VkVertexInputAttributeDescription, 3 > RTGL1::RasterizedDataCollector::GetVertexLayout()
{
    return { {
        {
            .location = 0,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
            .offset   = offsetof( ShVertex, position ),
        },
        {
            .location = 1,
            .binding  = 0,
            .format   = VK_FORMAT_R8G8B8A8_UNORM,
            .offset   = offsetof( ShVertex, color ),
        },
        {
            .location = 2,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32_SFLOAT,
            .offset   = offsetof( ShVertex, texCoord ),
        },
    } };
}

uint32_t RTGL1::RasterizedDataCollector::GetVertexStride()
{
    return static_cast< uint32_t >( sizeof( ShVertex ) );
}

RTGL1::RasterizedDataCollector::RasterizedDataCollector(
    VkDevice                           _device,
    std::shared_ptr< MemoryAllocator > _allocator,
    std::shared_ptr< TextureManager >  _textureMgr,
    uint32_t                           _maxVertexCount,
    uint32_t                           _maxIndexCount )
    : device( _device )
    , textureMgr( std::move( _textureMgr ) )
    , curVertexCount( 0 )
    , curIndexCount( 0 )
{
    vertexBuffer = std::make_shared< AutoBuffer >( _allocator );
    indexBuffer  = std::make_shared< AutoBuffer >( _allocator );

    _maxVertexCount = std::max( _maxVertexCount, 64u );
    _maxIndexCount  = std::max( _maxIndexCount, 64u );

    vertexBuffer->Create( _maxVertexCount * sizeof( ShVertex ),
                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          "Rasterizer vertex buffer" );
    indexBuffer->Create( _maxIndexCount * sizeof( uint32_t ),
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         "Rasterizer index buffer" );
}

namespace RTGL1
{
namespace
{
    VkViewport ToVk( const RgViewport& v )
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

    PipelineStateFlags ToPipelineState( GeometryRasterType         rasterType,
                                        const RgMeshPrimitiveInfo& info )
    {
        PipelineStateFlags r = 0;

        if( info.flags & RG_MESH_PRIMITIVE_ALPHA_TESTED )
        {
            r = r | PipelineStateFlagBits::ALPHA_TEST;
        }

        if( info.flags & RG_MESH_PRIMITIVE_TRANSLUCENT )
        {
            r = r | PipelineStateFlagBits::TRANSLUCENT;
        }

        // if alpha specifies semi-transparency
        if( Utils::UnpackAlphaFromPacked32( info.color ) < MESH_TRANSLUCENT_ALPHA_THRESHOLD )
        {
            r = r | PipelineStateFlagBits::TRANSLUCENT;
        }

        if( info.emissive > std::numeric_limits< float >::epsilon() )
        {
            r = r | PipelineStateFlagBits::ADDITIVE;
        }

        // depth test for world / sky geometry
        if( rasterType != GeometryRasterType::SWAPCHAIN )
        {
            r = r | PipelineStateFlagBits::DEPTH_TEST;

            // depth write if not semi-transparent
            if( !( r & PipelineStateFlagBits::TRANSLUCENT ) )
            {
                r = r | PipelineStateFlagBits::DEPTH_WRITE;
            }
        }

        return r;
    }

    void CopyFromArrayOfStructs( const RgMeshPrimitiveInfo& info, ShVertex* dstVerts )
    {
        assert( info.pVertices && dstVerts );

        // must be same to copy
        static_assert( std::is_same_v< decltype( info.pVertices ), const RgPrimitiveVertex* > );
        static_assert( sizeof( ShVertex ) == sizeof( RgPrimitiveVertex ) );
        static_assert( offsetof( ShVertex, position ) == offsetof( RgPrimitiveVertex, position ) );
        static_assert( offsetof( ShVertex, normal ) == offsetof( RgPrimitiveVertex, normal ) );
        static_assert( offsetof( ShVertex, tangent ) == offsetof( RgPrimitiveVertex, tangent ) );
        static_assert( offsetof( ShVertex, texCoord ) == offsetof( RgPrimitiveVertex, texCoord ) );
        static_assert( offsetof( ShVertex, color ) == offsetof( RgPrimitiveVertex, color ) );

        memcpy( dstVerts, info.pVertices, sizeof( ShVertex ) * info.vertexCount );
    }

    bool IndicesExist( const RgMeshPrimitiveInfo& info )
    {
        return info.indexCount > 0 && info.pIndices != nullptr;
    }
    void CopyIndices( const RgMeshPrimitiveInfo& info, uint32_t* dstIndices )
    {
        assert( IndicesExist( info ) && dstIndices );
        memcpy( dstIndices, info.pIndices, info.indexCount * sizeof( uint32_t ) );
    }
}
}

void RTGL1::RasterizedDataCollector::AddPrimitive( uint32_t                   frameIndex,
                                                   GeometryRasterType         rasterType,
                                                   const RgMeshPrimitiveInfo& info,
                                                   const float*               pViewProjection,
                                                   const RgViewport*          pViewport )
{
    assert( info.vertexCount > 0 && info.pVertices != nullptr );

    if( curVertexCount + info.vertexCount >= vertexBuffer->GetSize() / sizeof( ShVertex ) )
    {
        assert( 0 && "Increase the size of \"rasterizedMaxVertexCount\". Vertex buffer size "
                     "reached the limit." );
        return;
    }

    if( IndicesExist( info ) )
    {
        if( curIndexCount + info.indexCount >= indexBuffer->GetSize() / sizeof( uint32_t ) )
        {
            assert( 0 &&
                    "Increase the size of \"rasterizedMaxIndexCount\". Index buffer size reached "
                    "the limit." );
            return;
        }
    }


    // copy vertex data
    const uint32_t vertexCount = info.vertexCount;
    const uint32_t firstVertex = curVertexCount;

    {
        auto* vertsBase = vertexBuffer->GetMappedAs< ShVertex* >( frameIndex );
        CopyFromArrayOfStructs( info, &vertsBase[ firstVertex ] );
    }


    // copy index data
    const uint32_t indexCount = IndicesExist( info ) ? info.indexCount : 0;
    const uint32_t firstIndex = IndicesExist( info ) ? curIndexCount : 0;

    if( IndicesExist( info ) )
    {
        auto* indicesBase = indexBuffer->GetMappedAs< uint32_t* >( frameIndex );
        CopyIndices( info, &indicesBase[ firstIndex ] );
    }


    const auto textures = textureMgr->GetTexturesForLayers( info );
    const auto colors   = textureMgr->GetColorForLayers( info );

    PushInfo( rasterType ) = {
        .transform = info.transform,
        .flags     = GeomInfoManager::GetPrimitiveFlags( info ),

        .base_textureA = textures[ 0 ].indices[ 0 ],
        .base_textureB = textures[ 0 ].indices[ 1 ],
        .base_textureC = textures[ 0 ].indices[ 2 ],
        .base_color    = colors[ 0 ],

        .layer1_texture = textures[ 1 ].indices[ 0 ],
        .layer1_color   = colors[ 1 ],

        .layer2_texture = textures[ 2 ].indices[ 0 ],
        .layer2_color   = colors[ 2 ],

        .lightmap_texture = textures[ 3 ].indices[ 0 ],
        .lightmap_color   = colors[ 3 ],

        .vertexCount = vertexCount,
        .firstVertex = firstVertex,
        .indexCount  = indexCount,
        .firstIndex  = firstIndex,

        .viewProj = IfNotNull( pViewProjection, Float16D( pViewProjection ) ),
        .viewport = IfNotNull( pViewport, ToVk( *pViewport ) ),

        .pipelineState = ToPipelineState( rasterType, info ),
    };

    curVertexCount += info.vertexCount;
    curIndexCount += info.indexCount;
}

RTGL1::RasterizedDataCollector::DrawInfo& RTGL1::RasterizedDataCollector::PushInfo(
    GeometryRasterType rasterType )
{
    switch( rasterType )
    {
        case GeometryRasterType::WORLD: {
            return rasterDrawInfos.emplace_back();
        }
        case GeometryRasterType::SKY: {
            return skyDrawInfos.emplace_back();
        }
        case GeometryRasterType::SWAPCHAIN: {
            return swapchainDrawInfos.emplace_back();
        }
        default: {
            throw RgException( RG_RESULT_GRAPHICS_API_ERROR,
                               "RasterizedDataCollector::PushInfo error" );
        }
    }
}

void RTGL1::RasterizedDataCollector::Clear( uint32_t frameIndex )
{
    rasterDrawInfos.clear();
    swapchainDrawInfos.clear();
    skyDrawInfos.clear();

    curVertexCount = 0;
    curIndexCount  = 0;
}

void RTGL1::RasterizedDataCollector::CopyFromStaging( VkCommandBuffer cmd, uint32_t frameIndex )
{
    vertexBuffer->CopyFromStaging( cmd, frameIndex, sizeof( ShVertex ) * curVertexCount );
    indexBuffer->CopyFromStaging( cmd, frameIndex, sizeof( uint32_t ) * curIndexCount );
}

VkBuffer RTGL1::RasterizedDataCollector::GetVertexBuffer() const
{
    return vertexBuffer->GetDeviceLocal();
}

VkBuffer RTGL1::RasterizedDataCollector::GetIndexBuffer() const
{
    return indexBuffer->GetDeviceLocal();
}

const std::vector< RTGL1::RasterizedDataCollector::DrawInfo >& RTGL1::RasterizedDataCollector::
    GetRasterDrawInfos() const
{
    return rasterDrawInfos;
}

const std::vector< RTGL1::RasterizedDataCollector::DrawInfo >& RTGL1::RasterizedDataCollector::
    GetSwapchainDrawInfos() const
{
    return swapchainDrawInfos;
}

const std::vector< RTGL1::RasterizedDataCollector::DrawInfo >& RTGL1::RasterizedDataCollector::
    GetSkyDrawInfos() const
{
    return skyDrawInfos;
}
