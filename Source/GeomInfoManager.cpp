// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include "GeomInfoManager.h"

#include <algorithm>

#include "Matrix.h"
#include "VertexCollectorFilterType.h"
#include "Generated/ShaderCommonC.h"
#include "CmdLabel.h"
#include "Utils.h"

static_assert( sizeof( RTGL1::ShGeometryInstance ) % 16 == 0,
               "Std430 structs must be aligned by 16 bytes" );

namespace
{

uint32_t GetMaterialBlendFlags( const RgEditorTextureLayerInfo* layerInfo, uint32_t layerIndex )
{
    if( layerInfo == nullptr )
    {
        return 0;
    }

    uint32_t bitOffset = MATERIAL_BLENDING_TYPE_BIT_COUNT * layerIndex;

    switch( layerInfo->blend )
    {
        case RG_TEXTURE_LAYER_BLEND_TYPE_OPAQUE: return MATERIAL_BLENDING_TYPE_OPAQUE << bitOffset;
        case RG_TEXTURE_LAYER_BLEND_TYPE_ALPHA: return MATERIAL_BLENDING_TYPE_ALPHA << bitOffset;
        case RG_TEXTURE_LAYER_BLEND_TYPE_ADD: return MATERIAL_BLENDING_TYPE_ADD << bitOffset;
        case RG_TEXTURE_LAYER_BLEND_TYPE_SHADE: return MATERIAL_BLENDING_TYPE_SHADE << bitOffset;
        default: assert( 0 ); return 0;
    }
}

}

uint32_t RTGL1::GeomInfoManager::GetPrimitiveFlags( const RgMeshPrimitiveInfo& info )
{
    uint32_t f = 0;

    if( info.pEditorInfo )
    {
        f |= GetMaterialBlendFlags( info.pEditorInfo->pLayerBase, 0 );
        f |= GetMaterialBlendFlags( info.pEditorInfo->pLayer1, 1 );
        f |= GetMaterialBlendFlags( info.pEditorInfo->pLayer2, 2 );
        f |= GetMaterialBlendFlags( info.pEditorInfo->pLayerLightmap, 3 );
    }

    if( info.flags & RG_MESH_PRIMITIVE_MIRROR )
    {
        f |= GEOM_INST_FLAG_REFLECT;
    }

    if( info.flags & RG_MESH_PRIMITIVE_WATER )
    {
        f |= GEOM_INST_FLAG_MEDIA_TYPE_WATER;
        f |= GEOM_INST_FLAG_REFLECT;
        f |= GEOM_INST_FLAG_REFRACT;
    }

    return f;
}


RTGL1::GeomInfoManager::GeomInfoManager( VkDevice                            _device,
                                         std::shared_ptr< MemoryAllocator >& _allocator )
    : device( _device )
{
    buffer    = std::make_shared< AutoBuffer >( _allocator );
    matchPrev = std::make_shared< AutoBuffer >( _allocator );

    const uint32_t allBottomLevelGeomsCount =
        VertexCollectorFilterTypeFlags_GetAllBottomLevelGeomsCount();

    buffer->Create( allBottomLevelGeomsCount * sizeof( ShGeometryInstance ),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    "Geometry info buffer" );


    for( auto frameIndex = 0u; frameIndex < MAX_FRAMES_IN_FLIGHT; frameIndex++ )
    {
        auto baseSpan = std::span( buffer->GetMappedAs< ShGeometryInstance* >( frameIndex ),
                                   allBottomLevelGeomsCount );

        VertexCollectorFilterTypeFlags_IterateOverFlags_T(
            [ & ]( VertexCollectorFilterTypeFlags flags ) {
                auto from  = VertexCollectorFilterTypeFlags_GetOffsetInGlobalArray( flags );
                auto count = VertexCollectorFilterTypeFlags_GetAmountInGlobalArray( flags );

                AccessGeometryInstanceGroup( frameIndex, flags ) = baseSpan.subspan( from, count );
            } );
    }


    matchPrev->Create( allBottomLevelGeomsCount * sizeof( int32_t ),
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                       "Match previous Geometry infos buffer" );
    matchPrevShadow = std::make_unique< int32_t[] >( allBottomLevelGeomsCount );
}

bool RTGL1::GeomInfoManager::CopyFromStaging( VkCommandBuffer cmd,
                                              uint32_t        frameIndex,
                                              bool            insertBarrier )
{
    CmdLabel label( cmd, "Copying geom infos" );

    {
        VkBufferCopy          copyInfos[ MAX_TOP_LEVEL_INSTANCE_COUNT ];
        VkBufferMemoryBarrier barriers[ MAX_TOP_LEVEL_INSTANCE_COUNT ];

        uint32_t infoCount = 0;

        VertexCollectorFilterTypeFlags_IterateOverFlags_T(
            [ & ]( VertexCollectorFilterTypeFlags flags ) {
                //
                const auto groupOffsetInElements =
                    VertexCollectorFilterTypeFlags_GetOffsetInGlobalArray( flags );

                rgl::index_subspan elementsToCopy =
                    AccessGeometryInstanceGroup( Utils::PrevFrame( frameIndex ), flags )
                        .resolve_index_subspan( groupOffsetInElements );

                using MatchPrevIndexType =
                    std::remove_pointer_t< decltype( matchPrevShadow.get() ) >;


                if( elementsToCopy.elementsCount == 0 )
                {
                    return;
                }

                // copy to staging
                {
                    auto* dst = matchPrev->GetMappedAs< MatchPrevIndexType* >( frameIndex );
                    MatchPrevIndexType* src = matchPrevShadow.get();

                    memcpy( &dst[ elementsToCopy.elementsOffset ],
                            &src[ elementsToCopy.elementsOffset ],
                            elementsToCopy.elementsCount * sizeof( MatchPrevIndexType ) );
                }

                // copy from staging
                copyInfos[ infoCount ] = {
                    .srcOffset = elementsToCopy.elementsOffset * sizeof( MatchPrevIndexType ),
                    .dstOffset = elementsToCopy.elementsOffset * sizeof( MatchPrevIndexType ),
                    .size      = elementsToCopy.elementsCount * sizeof( MatchPrevIndexType ),
                };

                barriers[ infoCount ] = VkBufferMemoryBarrier{
                    .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer              = matchPrev->GetDeviceLocal(),
                    .offset              = copyInfos[ infoCount ].dstOffset,
                    .size                = copyInfos[ infoCount ].size,
                };

                infoCount++;
            } );


        if( infoCount > 0 )
        {
            matchPrev->CopyFromStaging( cmd, frameIndex, copyInfos, infoCount );

            if( insertBarrier )
            {
                vkCmdPipelineBarrier( cmd,
                                      VK_PIPELINE_STAGE_TRANSFER_BIT,
                                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                      0,
                                      0,
                                      nullptr,
                                      infoCount,
                                      barriers,
                                      0,
                                      nullptr );
            }
        }
    }


    {
        VkBufferCopy          copyInfos[ MAX_TOP_LEVEL_INSTANCE_COUNT ];
        VkBufferMemoryBarrier barriers[ MAX_TOP_LEVEL_INSTANCE_COUNT ];

        uint32_t infoCount = 0;

        VertexCollectorFilterTypeFlags_IterateOverFlags_T(
            [ & ]( VertexCollectorFilterTypeFlags flags ) {
                //
                const auto groupOffsetInBytes =
                    VertexCollectorFilterTypeFlags_GetOffsetInGlobalArray( flags ) *
                    sizeof( ShGeometryInstance );

                rgl::byte_subspan toCopy = AccessGeometryInstanceGroup( frameIndex, flags )
                                               .resolve_byte_subspan( groupOffsetInBytes );

                if( toCopy.sizeInBytes > 0 )
                {
                    copyInfos[ infoCount ] = VkBufferCopy{

                        .srcOffset = toCopy.offsetInBytes,
                        .dstOffset = toCopy.offsetInBytes,
                        .size      = toCopy.sizeInBytes,
                    };

                    barriers[ infoCount ] = VkBufferMemoryBarrier{
                        .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .buffer              = buffer->GetDeviceLocal(),
                        .offset              = copyInfos[ infoCount ].dstOffset,
                        .size                = copyInfos[ infoCount ].size,
                    };

                    infoCount++;
                }
            } );

        if( infoCount == 0 )
        {
            return false;
        }

        buffer->CopyFromStaging( cmd, frameIndex, copyInfos, infoCount );

        if( insertBarrier )
        {
            vkCmdPipelineBarrier( cmd,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                      VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                  0,
                                  0,
                                  nullptr,
                                  infoCount,
                                  barriers,
                                  0,
                                  nullptr );
        }
    }

    return true;
}

void RTGL1::GeomInfoManager::ResetMatchPrevForGroup( uint32_t                       frameIndex,
                                                     VertexCollectorFilterTypeFlags flags )
{
    int32_t* prevIndexToCurIndexArr = matchPrevShadow.get();

    auto offset = VertexCollectorFilterTypeFlags_GetOffsetInGlobalArray( flags );
    auto count  = VertexCollectorFilterTypeFlags_GetAmountInGlobalArray( flags );

    auto region = std::span( &prevIndexToCurIndexArr[ offset ], count );

    static_assert( std::is_same_v< decltype( region )::value_type, int >,
                   "Recheck usage of geomIndexPrevToCur in shaders if not int32" );

    // set each entry to invalid
    std::ranges::fill( region, -1 );
}

void RTGL1::GeomInfoManager::ResetOnlyDynamic( uint32_t frameIndex )
{
    VertexCollectorFilterTypeFlags_IterateOverFlags_T(
        [ & ]( VertexCollectorFilterTypeFlags flags ) {
            //
            if( flags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC )
            {
                ResetMatchPrevForGroup( frameIndex, flags );
                AccessGeometryInstanceGroup( frameIndex, flags ).reset_subspan();
            }
        } );
}

void RTGL1::GeomInfoManager::ResetOnlyStatic()
{
    movableIDToGeomFrameInfo.clear();

    for( auto frameIndex = 0u; frameIndex < MAX_FRAMES_IN_FLIGHT; frameIndex++ )
    {
        VertexCollectorFilterTypeFlags_IterateOverFlags_T(
            [ & ]( VertexCollectorFilterTypeFlags flags ) {
                //
                if( !( flags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ) )
                {
                    ResetMatchPrevForGroup( frameIndex, flags );
                    AccessGeometryInstanceGroup( frameIndex, flags ).reset_subspan();
                }
            } );
    }
}

uint32_t RTGL1::GeomInfoManager::GetGlobalGeomIndex( uint32_t                       localGeomIndex,
                                                     VertexCollectorFilterTypeFlags flags )
{
    return VertexCollectorFilterTypeFlags_GetOffsetInGlobalArray( flags ) + localGeomIndex;
}

void RTGL1::GeomInfoManager::PrepareForFrame( uint32_t frameIndex )
{
    dynamicIDToGeomFrameInfo[ frameIndex ].clear();
    ResetOnlyDynamic( frameIndex );
}

void RTGL1::GeomInfoManager::WriteGeomInfo( uint32_t                       frameIndex,
                                            uint64_t                       geomUniqueID,
                                            uint32_t                       localGeomIndex,
                                            VertexCollectorFilterTypeFlags flags,
                                            ShGeometryInstance&            src )
{
    // must be aligned for per-triangle vertex attributes
    assert( src.baseVertexIndex % 3 == 0 );
    assert( src.baseIndexIndex % 3 == 0 );

    uint32_t frameBegin, frameEnd;
    if( flags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC )
    {
        frameBegin = frameIndex;
        frameEnd   = frameIndex + 1;
    }
    else
    {
        // if static, copy to all staging buffers
        frameBegin = 0;
        frameEnd   = MAX_FRAMES_IN_FLIGHT;
    }

    uint32_t globalGeomIndex = GetGlobalGeomIndex( localGeomIndex, flags );

    for( uint32_t i = frameBegin; i < frameEnd; i++ )
    {
        FillWithPrevFrameData( flags, geomUniqueID, globalGeomIndex, src, i );

        auto &geomInstSpan = AccessGeometryInstanceGroup( i, flags );

        memcpy( &geomInstSpan[ localGeomIndex ], &src, sizeof( ShGeometryInstance ) );
        geomInstSpan.add_to_subspan( localGeomIndex );
    }

    WriteInfoForNextUsage( flags, geomUniqueID, globalGeomIndex, src, frameIndex );
}

void RTGL1::GeomInfoManager::FillWithPrevFrameData( VertexCollectorFilterTypeFlags flags,
                                                    uint64_t                       geomUniqueID,
                                                    uint32_t            currentGlobalGeomIndex,
                                                    ShGeometryInstance& dst,
                                                    uint32_t            frameIndex )
{
    assert( currentGlobalGeomIndex <
            static_cast< uint32_t >( std::numeric_limits< int32_t >::max() ) );

    int32_t* prevIndexToCurIndex = matchPrevShadow.get();

    const rgl::unordered_map< uint64_t, GeomFrameInfo >* prevIdToInfo = nullptr;

    bool isMovable = flags & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE;
    bool isDynamic = flags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC;

    // fill prev info, but only for movable and dynamic geoms
    if( isDynamic )
    {
        prevIdToInfo = &dynamicIDToGeomFrameInfo[ Utils::PrevFrame( frameIndex ) ];
    }
    else
    {
        // global geom indices are not changing for static geometry
        prevIndexToCurIndex[ currentGlobalGeomIndex ] =
            static_cast< int32_t >( currentGlobalGeomIndex );

        if( isMovable )
        {
            prevIdToInfo = &movableIDToGeomFrameInfo;
        }
        else
        {
            MarkNoPrevInfo( dst );
            return;
        }
    }

    const auto prev = prevIdToInfo->find( geomUniqueID );

    // if no previous info
    if( prev == prevIdToInfo->end() )
    {
        MarkNoPrevInfo( dst );
        return;
    }

    // if counts are not the same
    if( prev->second.vertexCount != dst.vertexCount || prev->second.indexCount != dst.indexCount )
    {
        MarkNoPrevInfo( dst );
        return;
    }

    // copy data from previous frame to current ShGeometryInstance
    dst.prevBaseVertexIndex = prev->second.baseVertexIndex;
    dst.prevBaseIndexIndex  = prev->second.baseIndexIndex;
    memcpy( dst.prevModel, prev->second.model, sizeof( float ) * 16 );

    if( isDynamic )
    {
        // save index to access ShGeometryInfo using previous frame's global geom index
        prevIndexToCurIndex[ prev->second.prevGlobalGeomIndex ] =
            static_cast< int32_t >( currentGlobalGeomIndex );
    }
}

void RTGL1::GeomInfoManager::MarkNoPrevInfo( ShGeometryInstance& dst )
{
    dst.prevBaseVertexIndex = UINT32_MAX;
}

void RTGL1::GeomInfoManager::MarkMovableHasPrevInfo( ShGeometryInstance& dst )
{
    dst.prevBaseVertexIndex = dst.baseVertexIndex;
}

void RTGL1::GeomInfoManager::WriteInfoForNextUsage( VertexCollectorFilterTypeFlags flags,
                                                    uint64_t                       geomUniqueID,
                                                    uint32_t currentGlobalGeomIndex,
                                                    const ShGeometryInstance& src,
                                                    uint32_t                  frameIndex )
{
    bool isMovable = flags & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE;
    bool isDynamic = flags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC;

    rgl::unordered_map< uint64_t, GeomFrameInfo >* idToInfo = nullptr;

    if( isDynamic )
    {
        idToInfo = &dynamicIDToGeomFrameInfo[ frameIndex ];
    }
    else if( isMovable )
    {
        idToInfo = &movableIDToGeomFrameInfo;
    }
    else
    {
        return;
    }

    // IDs must be unique
    assert( idToInfo->find( geomUniqueID ) == idToInfo->end() );

    GeomFrameInfo f = {
        .baseVertexIndex     = src.baseVertexIndex,
        .baseIndexIndex      = src.baseIndexIndex,
        .vertexCount         = src.vertexCount,
        .indexCount          = src.indexCount,
        .prevGlobalGeomIndex = currentGlobalGeomIndex,
    };
    static_assert( sizeof f.model == sizeof( float ) * 16 );
    static_assert( sizeof src.model == sizeof( float ) * 16 );
    memcpy( f.model, src.model, sizeof( float ) * 16 );

    ( *idToInfo )[ geomUniqueID ] = f;
}

VkBuffer RTGL1::GeomInfoManager::GetBuffer() const
{
    return buffer->GetDeviceLocal();
}

VkBuffer RTGL1::GeomInfoManager::GetMatchPrevBuffer() const
{
    return matchPrev->GetDeviceLocal();
}

rgl::subspan_incremental< RTGL1::ShGeometryInstance >& RTGL1::GeomInfoManager::
    AccessGeometryInstanceGroup( uint32_t frameIndex, VertexCollectorFilterTypeFlags flagsForGroup )
{
    assert( frameIndex < std::size( mappedBufferRegions ) );
    auto& r = mappedBufferRegions[ frameIndex ][ flagsForGroup ];

    // span must always match GlobalGeomIndex as it's used in shaders to index the 'buffer'
    if( r.data() != nullptr )
    {
        assert( r.data() == &buffer->GetMappedAs< ShGeometryInstance* >(
                                frameIndex )[ GetGlobalGeomIndex( 0, flagsForGroup ) ] );
    }

    // must be in group bounds
    assert( r.size() <= VertexCollectorFilterTypeFlags_GetAmountInGlobalArray( flagsForGroup ) );
    assert( r.count_in_subspan() <=
            VertexCollectorFilterTypeFlags_GetAmountInGlobalArray( flagsForGroup ) );

    return r;
}

uint32_t RTGL1::GeomInfoManager::GetCount( uint32_t frameIndex ) const
{
    uint32_t count = 0;

    for( const auto& [ flags, span ] : mappedBufferRegions[ frameIndex ] )
    {
        count += span.count_in_subspan();
    }

    return count;
}
