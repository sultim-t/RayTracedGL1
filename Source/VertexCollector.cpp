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

#include <algorithm>
#include <array>
#include <cstring>

#include "Generated/ShaderCommonC.h"
#include "UniqueID.h"
#include "Utils.h"

constexpr uint32_t INDEX_BUFFER_SIZE = MAX_INDEXED_PRIMITIVE_COUNT * 3 * sizeof( uint32_t );
constexpr uint32_t TRANSFORM_BUFFER_SIZE =
    MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * sizeof( VkTransformMatrixKHR );


RTGL1::VertexCollector::VertexCollector( VkDevice                                  _device,
                                         const std::shared_ptr< MemoryAllocator >& _allocator,
                                         std::shared_ptr< GeomInfoManager >        _geomInfoManager,
                                         VkDeviceSize                              _bufferSize,
                                         VertexCollectorFilterTypeFlags            _filters )
    : device( _device )
    , filtersFlags( _filters )
    , geomInfoMgr( std::move( _geomInfoManager ) )
    , curVertexCount( 0 )
    , curIndexCount( 0 )
    , curPrimitiveCount( 0 )
    , curTransformCount( 0 )
    , mappedVertexData( nullptr )
    , mappedIndexData( nullptr )
    , mappedTransformData( nullptr )
{
    assert( filtersFlags != 0 );

    bool isDynamic = filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC;

    vertBuffer       = std::make_shared< Buffer >();
    indexBuffer      = std::make_shared< Buffer >();
    transformsBuffer = std::make_shared< Buffer >();

    // dynamic vertices need also be copied to previous frame buffer
    VkBufferUsageFlags transferUsage =
        isDynamic ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                  : VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    // vertex buffers
    vertBuffer->Init( _allocator,
                      _bufferSize,
                      transferUsage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      isDynamic ? "Dynamic Vertices data buffer" : "Static Vertices data buffer" );

    // index buffers
    indexBuffer->Init( _allocator,
                       INDEX_BUFFER_SIZE,
                       transferUsage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                           VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       isDynamic ? "Dynamic Index data buffer" : "Static Index data buffer" );

    // transforms buffer
    transformsBuffer->Init(
        _allocator,
        TRANSFORM_BUFFER_SIZE,
        transferUsage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        isDynamic ? "Dynamic BLAS transforms buffer" : "Static BLAS transforms buffer" );

    // device local buffers are
    InitStagingBuffers( _allocator );
    InitFilters( filtersFlags );
}

RTGL1::VertexCollector::VertexCollector( const std::shared_ptr< const VertexCollector >& _src,
                                         const std::shared_ptr< MemoryAllocator >& _allocator )
    : device( _src->device )
    , filtersFlags( _src->filtersFlags )
    , vertBuffer( _src->vertBuffer )
    , indexBuffer( _src->indexBuffer )
    , transformsBuffer( _src->transformsBuffer )
    , geomInfoMgr( _src->geomInfoMgr )
    , curVertexCount( 0 )
    , curIndexCount( 0 )
    , curPrimitiveCount( 0 )
    , curTransformCount( 0 )
    , mappedVertexData( nullptr )
    , mappedIndexData( nullptr )
    , mappedTransformData( nullptr )
{
    // device local buffers are shared with the "src" vertex collector
    InitStagingBuffers( _allocator );
    InitFilters( filtersFlags );
}

void RTGL1::VertexCollector::InitStagingBuffers(
    const std::shared_ptr< MemoryAllocator >& allocator )
{
    // device local buffers must not be empty
    assert( vertBuffer && vertBuffer->GetSize() > 0 );
    assert( indexBuffer && indexBuffer->GetSize() > 0 );
    assert( transformsBuffer && transformsBuffer->GetSize() > 0 );
    assert( geomInfoMgr );

    // vertex buffers
    stagingVertBuffer.Init( allocator,
                            vertBuffer->GetSize(),
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC
                                ? "Dynamic Vertices data staging buffer"
                                : "Static Vertices data staging buffer" );

    // index buffers
    stagingIndexBuffer.Init( allocator,
                             indexBuffer->GetSize(),
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                             filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC
                                 ? "Dynamic Index data staging buffer"
                                 : "Static Index data staging buffer" );

    // transforms buffer
    stagingTransformsBuffer.Init( allocator,
                                  transformsBuffer->GetSize(),
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC
                                      ? "Dynamic BLAS transforms staging buffer"
                                      : "Static BLAS transforms staging buffer" );

    mappedVertexData    = static_cast< ShVertex* >( stagingVertBuffer.Map() );
    mappedIndexData     = static_cast< uint32_t* >( stagingIndexBuffer.Map() );
    mappedTransformData = static_cast< VkTransformMatrixKHR* >( stagingTransformsBuffer.Map() );
}

RTGL1::VertexCollector::~VertexCollector()
{
    // unmap buffers to destroy them
    stagingVertBuffer.TryUnmap();
    stagingIndexBuffer.TryUnmap();
    stagingTransformsBuffer.TryUnmap();
}

namespace
{

uint32_t AlignUpBy3( uint32_t x )
{
    return ( ( x + 2 ) / 3 ) * 3;
}

}

void RTGL1::VertexCollector::BeginCollecting( bool isStatic )
{
    assert( curVertexCount == 0 && curIndexCount == 0 && curPrimitiveCount == 0 );
    assert( ( isStatic && geomInfoMgr->GetStaticCount() == 0 ) ||
            ( !isStatic && geomInfoMgr->GetDynamicCount() == 0 ) );
    assert( GetAllGeometryCount() == 0 );
}

uint32_t RTGL1::VertexCollector::AddPrimitive( uint32_t                          frameIndex,
                                               const RgMeshInfo&                 parentMesh,
                                               const RgMeshPrimitiveInfo&        info,
                                               std::span< MaterialTextures, 4 >  layerTextures,
                                               std::span< RgColor4DPacked32, 4 > layerColors )
{
    using FT = VertexCollectorFilterTypeFlagBits;
    const VertexCollectorFilterTypeFlags geomFlags =
        VertexCollectorFilterTypeFlags_GetForGeometry( parentMesh, info );


    // if exceeds a limit of geometries in a group with specified geomFlags
    if( GetGeometryCount( geomFlags ) + 1 >=
        VertexCollectorFilterTypeFlags_GetAmountInGlobalArray( geomFlags ) )
    {
        assert( false && "Too many geometries in a group" );
        return UINT32_MAX;
    }


    const bool collectStatic = geomFlags & ( FT::CF_STATIC_NON_MOVABLE | FT::CF_STATIC_MOVABLE );

    const uint32_t maxVertexCount =
        collectStatic ? MAX_STATIC_VERTEX_COUNT : MAX_DYNAMIC_VERTEX_COUNT;


    const uint32_t vertIndex      = AlignUpBy3( curVertexCount );
    const uint32_t indIndex       = AlignUpBy3( curIndexCount );
    const uint32_t transformIndex = curTransformCount;

    const bool     useIndices    = info.indexCount != 0 && info.pIndices != nullptr;
    const uint32_t triangleCount = useIndices ? info.indexCount / 3 : info.vertexCount / 3;

    curVertexCount = vertIndex + info.vertexCount;
    curIndexCount  = indIndex + ( useIndices ? info.indexCount : 0 );
    curPrimitiveCount += triangleCount;
    curTransformCount += 1;



    // check bounds
    if( curVertexCount >= maxVertexCount )
    {
        assert( 0 );
        return UINT32_MAX;
    }

    if( curIndexCount >= MAX_INDEXED_PRIMITIVE_COUNT * 3 )
    {
        assert( 0 );
        return UINT32_MAX;
    }

    if( geomInfoMgr->GetCount() + 1 >= MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT )
    {
        assert( 0 );
        return UINT32_MAX;
    }



    // copy data to buffers
    {
        assert( stagingVertBuffer.IsMapped() );
        CopyDataToStaging( info, vertIndex );
    }

    if( useIndices )
    {
        assert( stagingIndexBuffer.IsMapped() );
        memcpy( mappedIndexData + indIndex, info.pIndices, info.indexCount * sizeof( uint32_t ) );
    }

    {
        static_assert( sizeof( RgTransform ) == sizeof( VkTransformMatrixKHR ) );
        memcpy(
            mappedTransformData + transformIndex, &info.transform, sizeof( VkTransformMatrixKHR ) );
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
                .deviceAddress = vertBuffer->GetAddress() + vertIndex * sizeof( ShVertex ) + offsetof( ShVertex, position ),
            },
            .vertexStride  = sizeof( ShVertex ),
            .maxVertex     = info.vertexCount,

            .indexType     = VK_INDEX_TYPE_NONE_KHR,
            .indexData     = {},

            .transformData = {
                .deviceAddress = transformsBuffer->GetAddress() + transformIndex * sizeof( VkTransformMatrixKHR ),
            },
        };

        if( useIndices )
        {
            trData.indexType = VK_INDEX_TYPE_UINT32;
            trData.indexData = {
                .deviceAddress = indexBuffer->GetAddress() + indIndex * sizeof( uint32_t ),
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



    ShGeometryInstance geomInfo = {
        .model     = RG_MATRIX_TRANSPOSED( info.transform ),
        .prevModel = { /* set later */ },

        .flags = GeomInfoManager::GetPrimitiveFlags( info ),

        .base_textureA = layerTextures[ 0 ].indices[ 0 ],
        .base_textureB = layerTextures[ 0 ].indices[ 1 ],
        .base_textureC = layerTextures[ 0 ].indices[ 2 ],
        .base_color    = layerColors[ 0 ],

        .layer1_texture = layerTextures[ 1 ].indices[ 0 ],
        .layer1_color   = layerColors[ 1 ],

        .layer2_texture = layerTextures[ 2 ].indices[ 0 ],
        .layer2_color   = layerColors[ 2 ],

        .lightmap_texture = layerTextures[ 3 ].indices[ 0 ],
        .lightmap_color   = layerColors[ 3 ],

        .baseVertexIndex     = vertIndex,
        .baseIndexIndex      = useIndices ? indIndex : UINT32_MAX,
        .prevBaseVertexIndex = { /* set later */ },
        .prevBaseIndexIndex  = { /* set later */ },
        .vertexCount         = info.vertexCount,
        .indexCount          = useIndices ? info.indexCount : UINT32_MAX,

        .defaultRoughness   = 1.0f,
        .defaultMetallicity = 0.0f,
        .defaultEmission    = 0.0f,
    };


    // simple index -- calculated as (global cur static count + global cur dynamic count)
    // global geometry index -- for indexing in geom infos buffer
    // local geometry index -- index of geometry in BLAS
    uint32_t simpleIndex =
        geomInfoMgr->WriteGeomInfo( frameIndex,
                                    UniqueID::MakeForPrimitive( parentMesh, info ),
                                    localIndex,
                                    geomFlags,
                                    geomInfo );


    if( collectStatic )
    {
        // save transform index for updating static movable's transforms
        simpleIndexToTransformIndex[ simpleIndex ] = transformIndex;

        /*
        // add material dependency but only for static geometry,
        // dynamic is updated each frame, so their materials will be updated anyway
        const std::tuple<uint32_t, RgMaterial, std::array<uint32_t, TEXTURES_PER_MATERIAL_COUNT>>
        layerDependencies[] =
        {
            // layer index - its material - corresponding texture indices
            { 0, info.layerMaterials[0], { materials[0].indices[0], materials[0].indices[1],
        materials[0].indices[2] } }, { 1, info.layerMaterials[1], { materials[1].indices[0],
        EMPTY_TEXTURE_INDEX,     EMPTY_TEXTURE_INDEX     } }, { 2, info.layerMaterials[2], {
        materials[2].indices[0], EMPTY_TEXTURE_INDEX,     EMPTY_TEXTURE_INDEX     } }, { 3,
        info.layerMaterials[3], { materials[3].indices[0], EMPTY_TEXTURE_INDEX, EMPTY_TEXTURE_INDEX
        } },
        };
        for( const auto& [ layerIndex, materialIndex, textureIndices ] : layerDependencies )
        {
            // if at least one texture is not empty on this layer, add dependency to the material
        layer for( uint32_t textureIndex : textureIndices )
            {
                if( textureIndex != EMPTY_TEXTURE_INDEX )
                {
                    AddMaterialDependency( simpleIndex, layerIndex, materialIndex );
                    break;
                }
            }
        }
        */
    }

    return simpleIndex;
}

void RTGL1::VertexCollector::CopyDataToStaging( const RgMeshPrimitiveInfo& info,
                                                uint32_t                   vertIndex )
{
    assert( ( vertIndex + info.vertexCount ) * sizeof( ShVertex ) < vertBuffer->GetSize() );

    ShVertex* const pDst = &mappedVertexData[ vertIndex ];

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

void RTGL1::VertexCollector::EndCollecting() {}

void RTGL1::VertexCollector::Reset()
{
    curVertexCount    = 0;
    curIndexCount     = 0;
    curPrimitiveCount = 0;
    curTransformCount = 0;

    simpleIndexToTransformIndex.clear();

    materialDependencies.clear();

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

    vkCmdCopyBuffer( cmd, stagingVertBuffer.GetBuffer(), vertBuffer->GetBuffer(), 1, &info );

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

    vkCmdCopyBuffer( cmd, stagingIndexBuffer.GetBuffer(), indexBuffer->GetBuffer(), 1, &info );

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
        cmd, stagingTransformsBuffer.GetBuffer(), transformsBuffer->GetBuffer(), 1, &info );

    if( insertMemBarrier )
    {
        VkBufferMemoryBarrier trnBr = {};
        trnBr.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        trnBr.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        trnBr.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        trnBr.srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
        trnBr.dstAccessMask         = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        trnBr.buffer                = transformsBuffer->GetBuffer();
        trnBr.size                  = curTransformCount * sizeof( VkTransformMatrixKHR );

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

bool RTGL1::VertexCollector::RecopyTransformsFromStaging( VkCommandBuffer cmd )
{
    return CopyTransformsFromStaging( cmd, true );
}

bool RTGL1::VertexCollector::RecopyTexCoordsFromStaging( VkCommandBuffer cmd )
{
    if( texCoordsToCopy.empty() )
    {
        return false;
    }
    assert( curTransformCount > 0 );

    vkCmdCopyBuffer( cmd,
                     stagingVertBuffer.GetBuffer(),
                     vertBuffer->GetBuffer(),
                     texCoordsToCopy.size(),
                     texCoordsToCopy.data() );

    VkDeviceSize lowerBound = UINT64_MAX;
    VkDeviceSize upperBound = 0;
    for( const auto& c : texCoordsToCopy )
    {
        lowerBound = std::min( lowerBound, c.dstOffset );
        upperBound = std::max( upperBound, c.dstOffset + c.size );
    }
    assert( lowerBound < upperBound );

    VkBufferMemoryBarrier txcBr = {};
    txcBr.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    txcBr.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    txcBr.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    txcBr.srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
    txcBr.dstAccessMask         = VK_ACCESS_SHADER_READ_BIT;
    txcBr.buffer                = vertBuffer->GetBuffer();
    txcBr.offset                = lowerBound;
    txcBr.size                  = upperBound - lowerBound;

    vkCmdPipelineBarrier( cmd,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                          0,
                          0,
                          nullptr,
                          1,
                          &txcBr,
                          0,
                          nullptr );

    texCoordsToCopy.clear();

    return true;
}

bool RTGL1::VertexCollector::CopyFromStaging( VkCommandBuffer cmd )
{
    bool                                   vrtCopied = CopyVertexDataFromStaging( cmd );
    bool                                   indCopied = CopyIndexDataFromStaging( cmd );
    bool                                   trnCopied = CopyTransformsFromStaging( cmd, false );

    std::array< VkBufferMemoryBarrier, 2 > barriers     = {};
    uint32_t                               barrierCount = 0;

    // just prepare for preprocessing - so no AS for this moment
    if( vrtCopied )
    {
        VkBufferMemoryBarrier& vrtBr = barriers[ barrierCount ];
        barrierCount++;

        vrtBr                     = {};
        vrtBr.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vrtBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        vrtBr.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vrtBr.buffer              = vertBuffer->GetBuffer();
        vrtBr.offset              = 0;
        vrtBr.size                = curVertexCount * sizeof( ShVertex );
    }

    // just prepare for preprocessing - so no AS for this moment
    if( indCopied )
    {
        VkBufferMemoryBarrier& indBr = barriers[ barrierCount ];
        barrierCount++;

        indBr                     = {};
        indBr.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        indBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        indBr.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        indBr.buffer              = indexBuffer->GetBuffer();
        indBr.offset              = 0;
        indBr.size                = curIndexCount * sizeof( uint32_t );
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

    if( trnCopied )
    {
        VkBufferMemoryBarrier trnBr = {};
        trnBr.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        trnBr.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        trnBr.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        trnBr.srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
        trnBr.dstAccessMask         = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        trnBr.buffer                = transformsBuffer->GetBuffer();
        trnBr.size                  = curTransformCount * sizeof( VkTransformMatrixKHR );

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


    return vrtCopied || indCopied || trnCopied;
}

/*void VertexCollector::UpdateTransform( uint32_t                     simpleIndex,
                                       const RgUpdateTransformInfo& updateInfo )
{
    if( simpleIndex >= MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT )
    {
        assert( 0 );
        return;
    }

    assert( mappedTransformData != nullptr );

    static_assert( sizeof( RgTransform ) == sizeof( VkTransformMatrixKHR ),
                   "RgTransform and VkTransformMatrixKHR must have the same structure to be used "
                   "in AS building" );
    memcpy( mappedTransformData + simpleIndexToTransformIndex[ simpleIndex ],
            &updateInfo.transform,
            sizeof( VkTransformMatrixKHR ) );

    geomInfoMgr->WriteStaticGeomInfoTransform(
        simpleIndex, updateInfo.movableStaticUniqueID, updateInfo.transform );
}*/

void RTGL1::VertexCollector::AddMaterialDependency( uint32_t simpleIndex,
                                                    uint32_t layer,
                                                    uint32_t materialIndex )
{
    // ignore empty materials
    if( materialIndex != 0 )
    {
        auto it = materialDependencies.find( materialIndex );

        if( it == materialDependencies.end() )
        {
            materialDependencies[ materialIndex ] = {};
            it                                    = materialDependencies.find( materialIndex );
        }

        it->second.push_back( { simpleIndex, layer } );
    }
}
void RTGL1::VertexCollector::OnMaterialChange( uint32_t                materialIndex,
                                               const MaterialTextures& newInfo )
{
    // for each geom index that has this material, update geometry instance infos
    for( const auto& p : materialDependencies[ materialIndex ] )
    {
        geomInfoMgr->WriteStaticGeomInfoMaterials( p.simpleIndex, p.layer, newInfo );
    }
}


VkBuffer RTGL1::VertexCollector::GetVertexBuffer() const
{
    return vertBuffer->GetBuffer();
}

VkBuffer RTGL1::VertexCollector::GetIndexBuffer() const
{
    return indexBuffer->GetBuffer();
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
    std::array< VkBufferMemoryBarrier, 2 > barriers     = {};
    uint32_t                               barrierCount = 0;

    if( curVertexCount > 0 )
    {
        VkBufferMemoryBarrier& vrtBr = barriers[ barrierCount ];
        barrierCount++;

        vrtBr                     = {};
        vrtBr.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vrtBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.srcAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vrtBr.dstAccessMask =
            VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
        vrtBr.buffer = vertBuffer->GetBuffer();
        vrtBr.offset = 0;
        vrtBr.size   = curVertexCount * sizeof( ShVertex );
    }

    if( curIndexCount > 0 )
    {
        VkBufferMemoryBarrier& indBr = barriers[ barrierCount ];
        barrierCount++;

        indBr                     = {};
        indBr.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        indBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.srcAccessMask       = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        indBr.dstAccessMask =
            VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
        indBr.buffer = indexBuffer->GetBuffer();
        indBr.offset = 0;
        indBr.size   = curIndexCount * sizeof( uint32_t );
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
