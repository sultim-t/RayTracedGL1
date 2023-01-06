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

#include "VertexCollector.h"

#include "GeomInfoManager.h"
#include "Utils.h"

#include "Generated/ShaderCommonC.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace
{

auto MakeName( std::string_view basename, RTGL1::VertexCollectorFilterTypeFlags filter )
{
    return std::format( "VC: {}-{}",
                        basename,
                        filter & RTGL1::VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? "Dynamic"
                                                                                      : "Static" );
}

auto MakeUsage( RTGL1::VertexCollectorFilterTypeFlags filter, bool accelStructureRead = true )
{
    VkBufferUsageFlags usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    if( filter & RTGL1::VertexCollectorFilterTypeFlagBits::CF_DYNAMIC )
    {
        // dynamic vertices need also be copied to previous frame buffer
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    else
    {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    if( accelStructureRead )
    {
        usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }

    return usage;
}

}

RTGL1::VertexCollector::VertexCollector( VkDevice         _device,
                                         MemoryAllocator& _allocator,
                                         const uint32_t ( &_maxVertsPerLayer )[ 4 ],
                                         VertexCollectorFilterTypeFlags _filters )
    : device( _device )
    , filtersFlags( _filters )
    , bufVertices( _allocator,
                   _maxVertsPerLayer[ 0 ],
                   MakeUsage( _filters ),
                   MakeName( "Vertices", _filters ) )
    , bufIndices( _allocator,
                  MAX_INDEXED_PRIMITIVE_COUNT * 3,
                  MakeUsage( _filters ),
                  MakeName( "Indices", _filters ) )
    , bufTransforms( _allocator,
                     MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT,
                     MakeUsage( _filters ),
                     MakeName( "BLAS Transforms", _filters ) )
    , bufTexcoordLayer1( _allocator,
                         _maxVertsPerLayer[ 1 ],
                         MakeUsage( _filters, false ),
                         MakeName( "Texcoords Layer1", _filters ) )
    , bufTexcoordLayer2( _allocator,
                         _maxVertsPerLayer[ 2 ],
                         MakeUsage( _filters, false ),
                         MakeName( "Texcoords Layer2", _filters ) )
    , bufTexcoordLayer3( _allocator,
                         _maxVertsPerLayer[ 3 ],
                         MakeUsage( _filters, false ),
                         MakeName( "Texcoords Layer3", _filters ) )
{
    InitFilters( filtersFlags );
}

// device local buffers are shared with the "src" vertex collector
RTGL1::VertexCollector::VertexCollector( const VertexCollector& _src, MemoryAllocator& _allocator )
    : device( _src.device )
    , filtersFlags( _src.filtersFlags )
    , bufVertices( _src.bufVertices, _allocator, MakeName( "Vertices", _src.filtersFlags ) )
    , bufIndices( _src.bufIndices, _allocator, MakeName( "Indices", _src.filtersFlags ) )
    , bufTransforms(
          _src.bufTransforms, _allocator, MakeName( "BLAS Transforms", _src.filtersFlags ) )
    , bufTexcoordLayer1(
          _src.bufTexcoordLayer1, _allocator, MakeName( "Texcoords Layer1", _src.filtersFlags ) )
    , bufTexcoordLayer2(
          _src.bufTexcoordLayer2, _allocator, MakeName( "Texcoords Layer2", _src.filtersFlags ) )
    , bufTexcoordLayer3(
          _src.bufTexcoordLayer3, _allocator, MakeName( "Texcoords Layer3", _src.filtersFlags ) )
{
    InitFilters( filtersFlags );
}

namespace
{

uint32_t AlignUpBy3( uint32_t x )
{
    return ( ( x + 2 ) / 3 ) * 3;
}

}

bool RTGL1::VertexCollector::AddPrimitive( uint32_t                          frameIndex,
                                           bool                              isStatic,
                                           const RgMeshInfo&                 parentMesh,
                                           const RgMeshPrimitiveInfo&        info,
                                           uint64_t                          uniqueID,
                                           std::span< MaterialTextures, 4 >  layerTextures,
                                           std::span< RgColor4DPacked32, 4 > layerColors,
                                           GeomInfoManager&                  geomInfoManager )
{
    using FT = VertexCollectorFilterTypeFlagBits;
    const VertexCollectorFilterTypeFlags geomFlags =
        VertexCollectorFilterTypeFlags_GetForGeometry( parentMesh, info, isStatic );


    // if exceeds a limit of geometries in a group with specified geomFlags
    {
        if( GetGeometryCount( geomFlags ) + 1 >=
            VertexCollectorFilterTypeFlags_GetAmountInGlobalArray( geomFlags ) )
        {
            debug::Error( "Too many geometries in a group ({}-{}-{}). Limit is {}",
                          uint32_t( geomFlags & FT::MASK_CHANGE_FREQUENCY_GROUP ),
                          uint32_t( geomFlags & FT::MASK_PASS_THROUGH_GROUP ),
                          uint32_t( geomFlags & FT::MASK_PRIMARY_VISIBILITY_GROUP ),
                          VertexCollectorFilterTypeFlags_GetAmountInGlobalArray( geomFlags ) );
            return false;
        }
    }



    const uint32_t vertIndex      = AlignUpBy3( curVertexCount );
    const uint32_t indIndex       = AlignUpBy3( curIndexCount );
    const uint32_t transformIndex = curTransformCount;
    const uint32_t texcIndex_1    = curTexCoordCount_Layer1;
    const uint32_t texcIndex_2    = curTexCoordCount_Layer2;
    const uint32_t texcIndex_3    = curTexCoordCount_Layer3;

    const bool     useIndices    = info.indexCount != 0 && info.pIndices != nullptr;
    const uint32_t triangleCount = useIndices ? info.indexCount / 3 : info.vertexCount / 3;

    curVertexCount = vertIndex + info.vertexCount;
    curIndexCount  = indIndex + ( useIndices ? info.indexCount : 0 );
    curPrimitiveCount += triangleCount;
    curTransformCount += 1;
    curTexCoordCount_Layer1 += GeomInfoManager::LayerExists( info, 1 ) ? info.vertexCount : 0;
    curTexCoordCount_Layer2 += GeomInfoManager::LayerExists( info, 2 ) ? info.vertexCount : 0;
    curTexCoordCount_Layer3 += GeomInfoManager::LayerExists( info, 3 ) ? info.vertexCount : 0;



    if( isStatic )
    {
        if( curVertexCount >= MAX_STATIC_VERTEX_COUNT )
        {
            debug::Error( "Too many static vertices: the limit is {}", MAX_STATIC_VERTEX_COUNT );
            return false;
        }
        assert( geomFlags & FT::CF_STATIC_NON_MOVABLE );
    }
    else
    {
        if( curVertexCount >= MAX_DYNAMIC_VERTEX_COUNT )
        {
            debug::Error( "Too many dynamic vertices: the limit is {}", MAX_DYNAMIC_VERTEX_COUNT );
            return false;
        }
        assert( geomFlags & FT::CF_DYNAMIC );
    }

    if( curIndexCount >= MAX_INDEXED_PRIMITIVE_COUNT * 3 )
    {
        debug::Error( "Too many indices: the limit is {}", MAX_INDEXED_PRIMITIVE_COUNT * 3 );
        return false;
    }

    if( geomInfoManager.GetCount( frameIndex ) + 1 >= MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT )
    {
        debug::Error( "Too many geometry infos: the limit is {}",
                      MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT );
        return false;
    }



    // copy data to buffers
    CopyVertexDataToStaging( info, vertIndex );
    CopyTexCoordsToStaging( 1, info, texcIndex_1 );
    CopyTexCoordsToStaging( 2, info, texcIndex_2 );
    CopyTexCoordsToStaging( 3, info, texcIndex_3 );

    if( useIndices )
    {
        assert( bufIndices.mapped );
        memcpy(
            &bufIndices.mapped[ indIndex ], info.pIndices, info.indexCount * sizeof( uint32_t ) );
    }

    {
        static_assert( sizeof( parentMesh.transform ) == sizeof( VkTransformMatrixKHR ) );
        assert( bufTransforms.mapped );

        memcpy( &bufTransforms.mapped[ transformIndex ],
                &parentMesh.transform,
                sizeof( VkTransformMatrixKHR ) );
    }



    VkAccelerationStructureGeometryKHR geom = {
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .flags        = geomFlags & FT::PT_OPAQUE
                            ? VkGeometryFlagsKHR( VK_GEOMETRY_OPAQUE_BIT_KHR )
                            : VkGeometryFlagsKHR( VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR ),
    };
    {
        VkAccelerationStructureGeometryTrianglesDataKHR trData = {
            .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,

            .vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData    = {
                .deviceAddress = bufVertices.deviceLocal->GetAddress() + vertIndex * sizeof( ShVertex ) + offsetof( ShVertex, position ),
            },
            .vertexStride  = sizeof( ShVertex ),
            .maxVertex     = info.vertexCount,

            .indexType     = VK_INDEX_TYPE_NONE_KHR,
            .indexData     = {},

            .transformData = {
                .deviceAddress = bufTransforms.deviceLocal->GetAddress() + transformIndex * sizeof( VkTransformMatrixKHR ),
            },
        };

        if( useIndices )
        {
            trData.indexType = VK_INDEX_TYPE_UINT32;
            trData.indexData = {
                .deviceAddress =
                    bufIndices.deviceLocal->GetAddress() + indIndex * sizeof( uint32_t ),
            };
        }

        geom.geometry.triangles = trData;
    }


    uint32_t localIndex = PushGeometry( geomFlags, geom );


    {
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {
            .primitiveCount  = triangleCount,
            .primitiveOffset = 0,
            .firstVertex     = 0,
            .transformOffset = 0,
        };
        PushRangeInfo( geomFlags, rangeInfo );

        PushPrimitiveCount( geomFlags, triangleCount );
    }


    const RgEditorPBRInfo* pbrInfo = ( info.pEditorInfo && info.pEditorInfo->pbrInfoExists )
                                         ? &info.pEditorInfo->pbrInfo
                                         : nullptr;

    ShGeometryInstance geomInfo = {
        .model     = RG_MATRIX_TRANSPOSED( parentMesh.transform ),
        .prevModel = { /* set later */ },

        .flags = GeomInfoManager::GetPrimitiveFlags( info ),

        .texture_base = layerTextures[ 0 ].indices[ TEXTURE_ALBEDO_ALPHA_INDEX ],
        .texture_base_ORM =
            layerTextures[ 0 ].indices[ TEXTURE_OCCLUSION_ROUGHNESS_METALLIC_INDEX ],
        .texture_base_N = layerTextures[ 0 ].indices[ TEXTURE_NORMAL_INDEX ],
        .texture_base_E = layerTextures[ 0 ].indices[ TEXTURE_EMISSIVE_INDEX ],

        .texture_layer1   = layerTextures[ 1 ].indices[ 0 ],
        .texture_layer2   = layerTextures[ 2 ].indices[ 0 ],
        .texture_lightmap = layerTextures[ 3 ].indices[ 0 ],

        .colorFactor_base     = layerColors[ 0 ],
        .colorFactor_layer1   = layerColors[ 1 ],
        .colorFactor_layer2   = layerColors[ 2 ],
        .colorFactor_lightmap = layerColors[ 3 ],

        .baseVertexIndex     = vertIndex,
        .baseIndexIndex      = useIndices ? indIndex : UINT32_MAX,
        .prevBaseVertexIndex = { /* set later */ },
        .prevBaseIndexIndex  = { /* set later */ },
        .vertexCount         = info.vertexCount,
        .indexCount          = useIndices ? info.indexCount : UINT32_MAX,

        .roughnessDefault = pbrInfo ? Utils::Saturate( pbrInfo->roughnessDefault ) : 1.0f,
        .metallicDefault  = pbrInfo ? Utils::Saturate( pbrInfo->metallicDefault ) : 0.0f,

        .emissiveMult = Utils::Saturate( info.emissive ),

        // values ignored if doesn't exist
        .firstVertex_Layer1 = texcIndex_1,
        .firstVertex_Layer2 = texcIndex_2,
        .firstVertex_Layer3 = texcIndex_3,
    };


    // global geometry index -- for indexing in geom infos buffer
    // local geometry index -- index of geometry in BLAS
    geomInfoManager.WriteGeomInfo( frameIndex, uniqueID, localIndex, geomFlags, geomInfo );

    return true;
}

void RTGL1::VertexCollector::CopyVertexDataToStaging( const RgMeshPrimitiveInfo& info,
                                                      uint32_t                   vertIndex )
{
    assert( bufVertices.mapped );
    assert( ( vertIndex + info.vertexCount ) * sizeof( ShVertex ) < bufVertices.staging.GetSize() );

    ShVertex* const pDst = &bufVertices.mapped[ vertIndex ];

    // must be same to copy
    static_assert( std::is_same_v< decltype( info.pVertices ), const RgPrimitiveVertex* > );
    static_assert( sizeof( ShVertex ) == sizeof( RgPrimitiveVertex ) );
    static_assert( offsetof( ShVertex, position ) == offsetof( RgPrimitiveVertex, position ) );
    static_assert( offsetof( ShVertex, normal ) == offsetof( RgPrimitiveVertex, normal ) );
    static_assert( offsetof( ShVertex, tangent ) == offsetof( RgPrimitiveVertex, tangent ) );
    static_assert( offsetof( ShVertex, texCoord ) == offsetof( RgPrimitiveVertex, texCoord ) );
    static_assert( offsetof( ShVertex, color ) == offsetof( RgPrimitiveVertex, color ) );

    memcpy( pDst, info.pVertices, info.vertexCount * sizeof( ShVertex ) );
}

void RTGL1::VertexCollector::CopyTexCoordsToStaging( uint32_t                   layerIndex,
                                                     const RgMeshPrimitiveInfo& info,
                                                     uint32_t                   dstTexcoordIndex )
{
    SharedDeviceLocal< RgFloat2D >* txc = nullptr;
    switch( layerIndex )
    {
        case 1: txc = &bufTexcoordLayer1; break;
        case 2: txc = &bufTexcoordLayer2; break;
        case 3: txc = &bufTexcoordLayer3; break;
        default: assert( 0 ); return;
    }

    if( const RgFloat2D* src = GeomInfoManager::AccessLayerTexCoords( info, layerIndex ) )
    {
        if( txc->IsInitialized() && txc->mapped )
        {
            memcpy( &txc->mapped[ dstTexcoordIndex ], src, info.vertexCount * sizeof( RgFloat2D ) );
        }
        else
        {
            debug::Error(
                "Found Layer{} texture coords on a primitive, but buffer was not allocated. "
                "Recheck RgInstanceCreateInfo::{}",
                layerIndex,
                layerIndex == 1   ? "allowTexCoordLayer1"
                : layerIndex == 2 ? "allowTexCoordLayer2"
                : layerIndex == 3 ? "allowTexCoordLayerLightmap"
                                  : "<unknown>" );
        }
    }
}

void RTGL1::VertexCollector::Reset()
{
    curVertexCount    = 0;
    curIndexCount     = 0;
    curPrimitiveCount = 0;
    curTransformCount = 0;

    for( auto& f : filters )
    {
        f.second->Reset();
    }
}

bool RTGL1::VertexCollector::CopyVertexDataFromStaging( VkCommandBuffer cmd )
{
    if( curVertexCount == 0 )
    {
        return false;
    }

    VkBufferCopy info = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = curVertexCount * sizeof( ShVertex ),
    };

    vkCmdCopyBuffer(
        cmd, bufVertices.staging.GetBuffer(), bufVertices.deviceLocal->GetBuffer(), 1, &info );

    return true;
}

bool RTGL1::VertexCollector::CopyTexCoordsFromStaging( VkCommandBuffer cmd, uint32_t layerIndex )
{
    std::pair< SharedDeviceLocal< RgFloat2D >*, uint32_t > txc = {};

    switch( layerIndex )
    {
        case 1: txc = { &bufTexcoordLayer1, curTexCoordCount_Layer1 }; break;
        case 2: txc = { &bufTexcoordLayer2, curTexCoordCount_Layer2 }; break;
        case 3: txc = { &bufTexcoordLayer3, curTexCoordCount_Layer3 }; break;
        default: assert( 0 ); return false;
    }

    auto& [ buf, count ] = txc;

    if( count == 0 )
    {
        return false;
    }

    VkBufferCopy info = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = count * sizeof( RgFloat2D ),
    };

    vkCmdCopyBuffer( cmd, buf->staging.GetBuffer(), buf->deviceLocal->GetBuffer(), 1, &info );
    return true;
}

bool RTGL1::VertexCollector::CopyIndexDataFromStaging( VkCommandBuffer cmd )
{
    if( curIndexCount == 0 )
    {
        return false;
    }

    VkBufferCopy info = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = curIndexCount * sizeof( uint32_t ),
    };

    vkCmdCopyBuffer(
        cmd, bufIndices.staging.GetBuffer(), bufIndices.deviceLocal->GetBuffer(), 1, &info );

    return true;
}

bool RTGL1::VertexCollector::CopyTransformsFromStaging( VkCommandBuffer cmd, bool insertMemBarrier )
{
    if( curTransformCount == 0 )
    {
        return false;
    }

    VkBufferCopy info = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size      = curTransformCount * sizeof( VkTransformMatrixKHR ),
    };

    vkCmdCopyBuffer(
        cmd, bufTransforms.staging.GetBuffer(), bufTransforms.deviceLocal->GetBuffer(), 1, &info );

    if( insertMemBarrier )
    {
        VkBufferMemoryBarrier trnBr = {
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = bufTransforms.deviceLocal->GetBuffer(),
            .size                = curTransformCount * sizeof( VkTransformMatrixKHR ),
        };

        vkCmdPipelineBarrier( cmd,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                              0,
                              0,
                              nullptr,
                              1,
                              &trnBr,
                              0,
                              nullptr );
    }

    return true;
}

bool RTGL1::VertexCollector::CopyFromStaging( VkCommandBuffer cmd )
{
    bool copiedAny = false;

    // just prepare for preprocessing - so no AS as the destination for this moment
    {
        std::array< VkBufferMemoryBarrier, 2 > barriers     = {};
        uint32_t                               barrierCount = 0;

        if( CopyVertexDataFromStaging( cmd ) )
        {
            barriers[ barrierCount++ ] = {
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer              = bufVertices.deviceLocal->GetBuffer(),
                .offset              = 0,
                .size                = curVertexCount * sizeof( ShVertex ),
            };
            copiedAny = true;
        }

        if( CopyIndexDataFromStaging( cmd ) )
        {
            barriers[ barrierCount++ ] = {
                .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer              = bufIndices.deviceLocal->GetBuffer(),
                .offset              = 0,
                .size                = curIndexCount * sizeof( uint32_t ),
            };
            copiedAny = true;
        }

        if( barrierCount > 0 )
        {
            vkCmdPipelineBarrier( cmd,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                      VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                  0,
                                  0,
                                  nullptr,
                                  barrierCount,
                                  barriers.data(),
                                  0,
                                  nullptr );
        }
    }

    // copy for read-only
    {
        std::array< VkBufferMemoryBarrier, 3 > barriers     = {};
        uint32_t                               barrierCount = 0;

        for( uint32_t layerIndex : { 1, 2, 3 } )
        {
            std::pair< SharedDeviceLocal< RgFloat2D >*, uint32_t /* elem count */ > txc = {};

            switch( layerIndex )
            {
                case 1: txc = { &bufTexcoordLayer1, curTexCoordCount_Layer1 }; break;
                case 2: txc = { &bufTexcoordLayer2, curTexCoordCount_Layer2 }; break;
                case 3: txc = { &bufTexcoordLayer3, curTexCoordCount_Layer3 }; break;
                default: assert( 0 ); continue;
            }

            if( CopyTexCoordsFromStaging( cmd, layerIndex ) )
            {
                barriers[ barrierCount++ ] = {
                    .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer              = txc.first->deviceLocal->GetBuffer(),
                    .offset              = 0,
                    .size                = txc.second * sizeof( RgFloat2D ),
                };
                copiedAny = true;
            }
        }

        if( barrierCount > 0 )
        {
            vkCmdPipelineBarrier( cmd,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  0,
                                  0,
                                  nullptr,
                                  barrierCount,
                                  barriers.data(),
                                  0,
                                  nullptr );
        }
    }

    if( CopyTransformsFromStaging( cmd, false ) )
    {
        VkBufferMemoryBarrier br = {
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = bufTransforms.deviceLocal->GetBuffer(),
            .size                = curTransformCount * sizeof( VkTransformMatrixKHR ),
        };

        vkCmdPipelineBarrier( cmd,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                              0,
                              0,
                              nullptr,
                              1,
                              &br,
                              0,
                              nullptr );
        copiedAny = true;
    }

    return copiedAny;
}

VkBuffer RTGL1::VertexCollector::GetVertexBuffer() const
{
    return bufVertices.deviceLocal->GetBuffer();
}

VkBuffer RTGL1::VertexCollector::GetIndexBuffer() const
{
    return bufIndices.deviceLocal->GetBuffer();
}

const std::vector< uint32_t >& RTGL1::VertexCollector::GetPrimitiveCounts(
    VertexCollectorFilterTypeFlags filter ) const
{
    auto f = filters.find( filter );
    assert( f != filters.end() );

    return f->second->GetPrimitiveCounts();
}

const std::vector< VkAccelerationStructureGeometryKHR >& RTGL1::VertexCollector::GetASGeometries(
    VertexCollectorFilterTypeFlags filter ) const
{
    auto f = filters.find( filter );
    assert( f != filters.end() );

    return f->second->GetASGeometries();
}

const std::vector< VkAccelerationStructureBuildRangeInfoKHR >& RTGL1::VertexCollector::
    GetASBuildRangeInfos( VertexCollectorFilterTypeFlags filter ) const
{
    auto f = filters.find( filter );
    assert( f != filters.end() );

    return f->second->GetASBuildRangeInfos();
}

bool RTGL1::VertexCollector::AreGeometriesEmpty( VertexCollectorFilterTypeFlags flags ) const
{
    for( const auto& p : filters )
    {
        const auto& f = p.second;

        // if filter includes any type from flags
        // and it's not empty
        if( ( f->GetFilter() & flags ) && f->GetGeometryCount() > 0 )
        {
            return false;
        }
    }

    return true;
}

bool RTGL1::VertexCollector::AreGeometriesEmpty( VertexCollectorFilterTypeFlagBits type ) const
{
    return AreGeometriesEmpty( ( VertexCollectorFilterTypeFlags )type );
}

void RTGL1::VertexCollector::InsertVertexPreprocessBeginBarrier( VkCommandBuffer cmd )
{
    // barriers were already inserted in CopyFromStaging()
}

void RTGL1::VertexCollector::InsertVertexPreprocessFinishBarrier( VkCommandBuffer cmd )
{
    std::array< VkBufferMemoryBarrier, 5 > barriers     = {};
    uint32_t                               barrierCount = 0;

    if( curVertexCount > 0 )
    {
        barriers[ barrierCount++ ] = {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask =
                VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = bufVertices.deviceLocal->GetBuffer(),
            .offset              = 0,
            .size                = curVertexCount * sizeof( ShVertex ),
        };
    }

    if( curIndexCount > 0 )
    {
        barriers[ barrierCount++ ] = {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask =
                VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = bufIndices.deviceLocal->GetBuffer(),
            .offset              = 0,
            .size                = curIndexCount * sizeof( uint32_t ),
        };
    }

    if( barrierCount == 0 )
    {
        return;
    }

    vkCmdPipelineBarrier( cmd,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
                              VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                          0,
                          0,
                          nullptr,
                          barrierCount,
                          barriers.data(),
                          0,
                          nullptr );
}

uint32_t RTGL1::VertexCollector::PushGeometry( VertexCollectorFilterTypeFlags            type,
                                               const VkAccelerationStructureGeometryKHR& geom )
{
    assert( filters.find( type ) != filters.end() );

    return filters[ type ]->PushGeometry( type, geom );
}

void RTGL1::VertexCollector::PushPrimitiveCount( VertexCollectorFilterTypeFlags type,
                                                 uint32_t                       primCount )
{
    assert( filters.find( type ) != filters.end() );

    filters[ type ]->PushPrimitiveCount( type, primCount );
}

void RTGL1::VertexCollector::PushRangeInfo(
    VertexCollectorFilterTypeFlags type, const VkAccelerationStructureBuildRangeInfoKHR& rangeInfo )
{
    assert( filters.find( type ) != filters.end() );

    filters[ type ]->PushRangeInfo( type, rangeInfo );
}

uint32_t RTGL1::VertexCollector::GetGeometryCount( VertexCollectorFilterTypeFlags type )
{
    assert( filters.find( type ) != filters.end() );

    return filters[ type ]->GetGeometryCount();
}

uint32_t RTGL1::VertexCollector::GetAllGeometryCount() const
{
    uint32_t count = 0;

    for( const auto& f : filters )
    {
        count += f.second->GetGeometryCount();
    }

    return count;
}

uint32_t RTGL1::VertexCollector::GetCurrentVertexCount() const
{
    return curVertexCount;
}

uint32_t RTGL1::VertexCollector::GetCurrentIndexCount() const
{
    return curIndexCount;
}

void RTGL1::VertexCollector::AddFilter( VertexCollectorFilterTypeFlags filterGroup )
{
    if( filterGroup == ( VertexCollectorFilterTypeFlags )0 )
    {
        return;
    }

    assert( filters.find( filterGroup ) == filters.end() );

    filters[ filterGroup ] = std::make_shared< VertexCollectorFilter >( filterGroup );
}

// try create filters for each group (mask)
void RTGL1::VertexCollector::InitFilters( VertexCollectorFilterTypeFlags flags )
{
    assert( flags != 0 );

    typedef VertexCollectorFilterTypeFlags    FL;
    typedef VertexCollectorFilterTypeFlagBits FT;

    // iterate over all pairs of group bits
    VertexCollectorFilterTypeFlags_IterateOverFlags( [ this, flags ]( FL f ) {
        // if flags contain this pair of group bits
        if( ( flags & f ) == f )
        {
            AddFilter( f );
        }
    } );
}
