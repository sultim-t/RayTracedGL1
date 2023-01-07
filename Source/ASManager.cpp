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

#include "ASManager.h"

#include "CmdLabel.h"
#include "GeomInfoManager.h"
#include "Matrix.h"
#include "Utils.h"

#include "Generated/ShaderCommonC.h"

#include <array>
#include <cstring>

namespace
{
constexpr uint32_t AdditionalTexCoordMaxCount = MAX_STATIC_VERTEX_COUNT;
}

RTGL1::ASManager::ASManager( VkDevice                                _device,
                             const PhysicalDevice&                   _physDevice,
                             std::shared_ptr< MemoryAllocator >      _allocator,
                             std::shared_ptr< CommandBufferManager > _cmdManager,
                             std::shared_ptr< GeomInfoManager >      _geomInfoManager,
                             bool                                    _enableTexCoordLayer1,
                             bool                                    _enableTexCoordLayer2,
                             bool                                    _enableTexCoordLayer3 )
    : device( _device )
    , allocator( std::move( _allocator ) )
    , staticCopyFence( VK_NULL_HANDLE )
    , cmdManager( std::move( _cmdManager ) )
    , geomInfoMgr( std::move( _geomInfoManager ) )
    , descPool( VK_NULL_HANDLE )
    , buffersDescSetLayout( VK_NULL_HANDLE )
    , buffersDescSets{}
    , asDescSetLayout( VK_NULL_HANDLE )
    , asDescSets{}
{
    typedef VertexCollectorFilterTypeFlags    FL;
    typedef VertexCollectorFilterTypeFlagBits FT;


    // init AS structs for each dimension
    VertexCollectorFilterTypeFlags_IterateOverFlags( [ this ]( FL filter ) {
        if( filter & FT::CF_DYNAMIC )
        {
            for( auto& b : allDynamicBlas )
            {
                b.emplace_back( std::make_unique< BLASComponent >( device, filter ) );
            }
        }
        else
        {
            allStaticBlas.emplace_back( std::make_unique< BLASComponent >( device, filter ) );
        }
    } );

    for( auto& t : tlas )
    {
        t = std::make_unique< TLASComponent >( device, "TLAS main" );
    }

    const uint32_t scratchOffsetAligment =
        _physDevice.GetASProperties().minAccelerationStructureScratchOffsetAlignment;
    scratchBuffer = std::make_shared< ScratchBuffer >( allocator, scratchOffsetAligment );
    asBuilder     = std::make_shared< ASBuilder >( device, scratchBuffer );


    uint32_t maxVertsPerLayer[] = {
        MAX_STATIC_VERTEX_COUNT,
        _enableTexCoordLayer1 ? AdditionalTexCoordMaxCount : 0,
        _enableTexCoordLayer2 ? AdditionalTexCoordMaxCount : 0,
        _enableTexCoordLayer3 ? AdditionalTexCoordMaxCount : 0,
    };

    // static and movable static vertices share the same buffer as their data won't be changing
    collectorStatic = std::make_shared< VertexCollector >(
        device,
        *allocator,
        maxVertsPerLayer,
        FT::CF_STATIC_NON_MOVABLE | FT::CF_STATIC_MOVABLE | FT::MASK_PASS_THROUGH_GROUP |
            FT::MASK_PRIMARY_VISIBILITY_GROUP );


    // dynamic vertices
    collectorDynamic[ 0 ] = std::make_shared< VertexCollector >(
        device,
        *allocator,
        maxVertsPerLayer,
        FT::CF_DYNAMIC | FT::MASK_PASS_THROUGH_GROUP | FT::MASK_PRIMARY_VISIBILITY_GROUP );

    // other dynamic vertex collectors should share the same device local buffers as the first one
    for( uint32_t i = 1; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        collectorDynamic[ i ] =
            std::make_shared< VertexCollector >( *( collectorDynamic[ 0 ] ), *allocator );
    }

    previousDynamicPositions.Init( *allocator,
                                   MAX_DYNAMIC_VERTEX_COUNT * sizeof( ShVertex ),
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   "Previous frame's vertex data" );
    previousDynamicIndices.Init( *allocator,
                                 MAX_DYNAMIC_VERTEX_COUNT * sizeof( uint32_t ),
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 "Previous frame's index data" );


    // instance buffer for TLAS
    instanceBuffer = std::make_unique< AutoBuffer >( allocator );

    VkDeviceSize instanceBufferSize =
        MAX_TOP_LEVEL_INSTANCE_COUNT * sizeof( VkAccelerationStructureInstanceKHR );
    instanceBuffer->Create(
        instanceBufferSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        "TLAS instance buffer" );

    static_assert( std::size( TLASPrepareResult{}.instances ) == MAX_TOP_LEVEL_INSTANCE_COUNT );


    CreateDescriptors();

    // buffers won't be changing, update once
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        UpdateBufferDescriptors( i );
    }


    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags             = 0;
    VkResult r                  = vkCreateFence( device, &fenceInfo, nullptr, &staticCopyFence );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, staticCopyFence, VK_OBJECT_TYPE_FENCE, "Static BLAS fence" );
}

namespace
{

template< size_t N >
constexpr bool CheckBindings( const VkDescriptorSetLayoutBinding ( &bindings )[ N ] )
{
    for( size_t i = 0; i < N; i++ )
    {
        if( bindings[ i ].binding != i )
        {
            return false;
        }
    }
    return true;
}

template< size_t N >
constexpr bool CheckBindings( const VkWriteDescriptorSet ( &bindings )[ N ] )
{
    for( size_t i = 0; i < N; i++ )
    {
        if( bindings[ i ].dstBinding != i )
        {
            return false;
        }
    }
    return true;
}

}

void RTGL1::ASManager::CreateDescriptors()
{
    VkResult                              r;
    std::array< VkDescriptorPoolSize, 2 > poolSizes{};

    {
        constexpr VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding         = BINDING_VERTEX_BUFFER_STATIC,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_VERTEX_BUFFER_DYNAMIC,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_INDEX_BUFFER_STATIC,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_INDEX_BUFFER_DYNAMIC,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_GEOMETRY_INSTANCES,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_GEOMETRY_INSTANCES_MATCH_PREV,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_PREV_POSITIONS_BUFFER_DYNAMIC,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_PREV_INDEX_BUFFER_DYNAMIC,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_STATIC_TEXCOORD_LAYER_1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_STATIC_TEXCOORD_LAYER_2,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_STATIC_TEXCOORD_LAYER_3,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_DYNAMIC_TEXCOORD_LAYER_1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_DYNAMIC_TEXCOORD_LAYER_2,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
            {
                .binding         = BINDING_DYNAMIC_TEXCOORD_LAYER_3,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            },
        };
        static_assert( CheckBindings( bindings ) );

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = std::size( bindings ),
            .pBindings    = bindings,
        };
        r = vkCreateDescriptorSetLayout( device, &layoutInfo, nullptr, &buffersDescSetLayout );
        VK_CHECKERROR( r );

        poolSizes[ 0 ] = {
            .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT * std::size( bindings ),
        };
    }

    {
        VkDescriptorSetLayoutBinding bnd = {
            .binding         = BINDING_ACCELERATION_STRUCTURE_MAIN,
            .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        };
        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings    = &bnd,
        };
        r = vkCreateDescriptorSetLayout( device, &layoutInfo, nullptr, &asDescSetLayout );
        VK_CHECKERROR( r );

        poolSizes[ 1 ] = {
            .type            = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        };
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets       = MAX_FRAMES_IN_FLIGHT * 2,
        .poolSizeCount = poolSizes.size(),
        .pPoolSizes    = poolSizes.data(),
    };
    r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &descPool );
    VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "AS manager Desc pool" );

    VkDescriptorSetAllocateInfo descSetInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descPool,
        .descriptorSetCount = 1,
    };

    SET_DEBUG_NAME( device,
                    buffersDescSetLayout,
                    VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                    "Vertex data Desc set layout" );
    SET_DEBUG_NAME(
        device, asDescSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "TLAS Desc set layout" );

    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        descSetInfo.pSetLayouts = &buffersDescSetLayout;
        r = vkAllocateDescriptorSets( device, &descSetInfo, &buffersDescSets[ i ] );
        VK_CHECKERROR( r );

        descSetInfo.pSetLayouts = &asDescSetLayout;
        r = vkAllocateDescriptorSets( device, &descSetInfo, &asDescSets[ i ] );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME(
            device, buffersDescSets[ i ], VK_OBJECT_TYPE_DESCRIPTOR_SET, "Vertex data Desc set" );
        SET_DEBUG_NAME( device, asDescSets[ i ], VK_OBJECT_TYPE_DESCRIPTOR_SET, "TLAS Desc set" );
    }
}

void RTGL1::ASManager::UpdateBufferDescriptors( uint32_t frameIndex )
{
    VkDescriptorBufferInfo infos[] = {
        {
            .buffer = collectorStatic->GetVertexBuffer(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = collectorDynamic[ frameIndex ]->GetVertexBuffer(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = collectorStatic->GetIndexBuffer(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = collectorDynamic[ frameIndex ]->GetIndexBuffer(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = geomInfoMgr->GetBuffer(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = geomInfoMgr->GetMatchPrevBuffer(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = previousDynamicPositions.GetBuffer(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = previousDynamicIndices.GetBuffer(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = collectorStatic->GetTexcoordBuffer_Layer1(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = collectorStatic->GetTexcoordBuffer_Layer2(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = collectorStatic->GetTexcoordBuffer_Layer3(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = collectorDynamic[ frameIndex ]->GetTexcoordBuffer_Layer1(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = collectorDynamic[ frameIndex ]->GetTexcoordBuffer_Layer2(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
        {
            .buffer = collectorDynamic[ frameIndex ]->GetTexcoordBuffer_Layer3(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        },
    };

    VkWriteDescriptorSet writes[] = {
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_VERTEX_BUFFER_STATIC,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_VERTEX_BUFFER_STATIC ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_VERTEX_BUFFER_DYNAMIC,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_VERTEX_BUFFER_DYNAMIC ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_INDEX_BUFFER_STATIC,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_INDEX_BUFFER_STATIC ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_INDEX_BUFFER_DYNAMIC,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_INDEX_BUFFER_DYNAMIC ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_GEOMETRY_INSTANCES,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_GEOMETRY_INSTANCES ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_GEOMETRY_INSTANCES_MATCH_PREV,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_GEOMETRY_INSTANCES_MATCH_PREV ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_PREV_POSITIONS_BUFFER_DYNAMIC,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_PREV_POSITIONS_BUFFER_DYNAMIC ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_PREV_INDEX_BUFFER_DYNAMIC,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_PREV_INDEX_BUFFER_DYNAMIC ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_STATIC_TEXCOORD_LAYER_1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_STATIC_TEXCOORD_LAYER_1 ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_STATIC_TEXCOORD_LAYER_2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_STATIC_TEXCOORD_LAYER_2 ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_STATIC_TEXCOORD_LAYER_3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_STATIC_TEXCOORD_LAYER_3 ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_DYNAMIC_TEXCOORD_LAYER_1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_DYNAMIC_TEXCOORD_LAYER_1 ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_DYNAMIC_TEXCOORD_LAYER_2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_DYNAMIC_TEXCOORD_LAYER_2 ],
        },
        {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = buffersDescSets[ frameIndex ],
            .dstBinding      = BINDING_DYNAMIC_TEXCOORD_LAYER_3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &infos[ BINDING_DYNAMIC_TEXCOORD_LAYER_3 ],
        },
    };
    assert( CheckBindings( writes ) );

    static_assert( std::size( infos ) == std::size( writes ) );

    vkUpdateDescriptorSets( device, std::size( writes ), writes, 0, nullptr );
}

void RTGL1::ASManager::UpdateASDescriptors( uint32_t frameIndex )
{
    VkAccelerationStructureKHR asHandle = tlas[ frameIndex ]->GetAS();
    assert( asHandle != VK_NULL_HANDLE );

    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures    = &asHandle,
    };

    VkWriteDescriptorSet wrt = {
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext           = &asInfo,
        .dstSet          = asDescSets[ frameIndex ],
        .dstBinding      = BINDING_ACCELERATION_STRUCTURE_MAIN,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    vkUpdateDescriptorSets( device, 1, &wrt, 0, nullptr );
}

RTGL1::ASManager::~ASManager()
{
    for( auto& as : allStaticBlas )
    {
        as->Destroy();
    }

    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        for( auto& as : allDynamicBlas[ i ] )
        {
            as->Destroy();
        }

        tlas[ i ]->Destroy();
    }

    vkDestroyDescriptorPool( device, descPool, nullptr );
    vkDestroyDescriptorSetLayout( device, buffersDescSetLayout, nullptr );
    vkDestroyDescriptorSetLayout( device, asDescSetLayout, nullptr );
    vkDestroyFence( device, staticCopyFence, nullptr );
}

bool RTGL1::ASManager::SetupBLAS( BLASComponent& blas, const VertexCollector& vertCollector )
{
    const auto  filter = blas.GetFilter();
    const auto& geoms  = vertCollector.GetASGeometries( filter );

    blas.SetGeometryCount( static_cast< uint32_t >( geoms.size() ) );

    if( blas.IsEmpty() )
    {
        return false;
    }

    const auto& ranges     = vertCollector.GetASBuildRangeInfos( filter );
    const auto& primCounts = vertCollector.GetPrimitiveCounts( filter );

    const bool fastTrace = !IsFastBuild( filter );
    const bool update    = false;

    // get AS size and create buffer for AS
    const auto buildSizes =
        asBuilder->GetBottomBuildSizes( geoms.size(), geoms.data(), primCounts.data(), fastTrace );

    // if no buffer, or it was created, but its size is too small for current AS
    blas.RecreateIfNotValid( buildSizes, allocator );

    assert( blas.GetAS() != VK_NULL_HANDLE );

    // add BLAS, all passed arrays must be alive until BuildBottomLevel() call
    asBuilder->AddBLAS( blas.GetAS(),
                        geoms.size(),
                        geoms.data(),
                        ranges.data(),
                        buildSizes,
                        fastTrace,
                        update,
                        blas.GetFilter() & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE );

    return true;
}

void RTGL1::ASManager::UpdateBLAS( BLASComponent& blas, const VertexCollector& vertCollector )
{
    const auto  filter = blas.GetFilter();
    const auto& geoms  = vertCollector.GetASGeometries( filter );

    blas.SetGeometryCount( static_cast< uint32_t >( geoms.size() ) );

    if( blas.IsEmpty() )
    {
        return;
    }

    const auto& ranges     = vertCollector.GetASBuildRangeInfos( filter );
    const auto& primCounts = vertCollector.GetPrimitiveCounts( filter );

    const bool fastTrace = !IsFastBuild( filter );
    // must be just updated
    const bool update = true;

    const auto buildSizes =
        asBuilder->GetBottomBuildSizes( geoms.size(), geoms.data(), primCounts.data(), fastTrace );

    assert( blas.IsValid( buildSizes ) );
    assert( blas.GetAS() != VK_NULL_HANDLE );

    // add BLAS, all passed arrays must be alive until BuildBottomLevel() call
    asBuilder->AddBLAS( blas.GetAS(),
                        geoms.size(),
                        geoms.data(),
                        ranges.data(),
                        buildSizes,
                        fastTrace,
                        update,
                        blas.GetFilter() & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE );
}

RTGL1::StaticGeometryToken RTGL1::ASManager::BeginStaticGeometry()
{
    // the whole static vertex data must be recreated, clear previous data
    collectorStatic->Reset();
    geomInfoMgr->ResetOnlyStatic();

    return StaticGeometryToken( InitAsExisting );
}

void RTGL1::ASManager::SubmitStaticGeometry( StaticGeometryToken& token )
{
    assert( token );
    token = {};

    // static geometry submission happens very infrequently, e.g. on level load
    vkDeviceWaitIdle( device );

    typedef VertexCollectorFilterTypeFlagBits FT;

    auto staticFlags = FT::CF_STATIC_NON_MOVABLE | FT::CF_STATIC_MOVABLE;

    // destroy previous static
    for( auto& staticBlas : allStaticBlas )
    {
        assert( !( staticBlas->GetFilter() & FT::CF_DYNAMIC ) );

        // if flags have any of static bits
        if( staticBlas->GetFilter() & staticFlags )
        {
            staticBlas->Destroy();
            staticBlas->SetGeometryCount( 0 );
        }
    }

    assert( asBuilder->IsEmpty() );

    // skip if all static geometries are empty
    if( collectorStatic->AreGeometriesEmpty( staticFlags ) )
    {
        return;
    }

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    // copy from staging with barrier
    collectorStatic->CopyFromStaging( cmd );

    // setup static blas
    for( auto& staticBlas : allStaticBlas )
    {
        // if flags have any of static bits
        if( staticBlas->GetFilter() & staticFlags )
        {
            SetupBLAS( *staticBlas, *collectorStatic );
        }
    }

    // build AS
    asBuilder->BuildBottomLevel( cmd );

    // submit geom info, in case if rgStartNewScene and rgSubmitStaticGeometries
    // were out of rgStartFrame - rgDrawFrame, so static geominfo-s won't be
    // erased on GeomInfoManager::PrepareForFrame
    geomInfoMgr->CopyFromStaging( cmd, 0, false );

    // submit and wait
    cmdManager->Submit( cmd, staticCopyFence );
    Utils::WaitAndResetFence( device, staticCopyFence );
}

RTGL1::DynamicGeometryToken RTGL1::ASManager::BeginDynamicGeometry( VkCommandBuffer cmd,
                                                                    uint32_t        frameIndex )
{
    scratchBuffer->Reset();

    // store data of current frame to use it in the next one
    CopyDynamicDataToPrevBuffers( cmd,
                                  Utils::GetPreviousByModulo( frameIndex, MAX_FRAMES_IN_FLIGHT ) );

    // dynamic AS must be recreated
    collectorDynamic[ frameIndex ]->Reset();

    return DynamicGeometryToken( InitAsExisting );
}

bool RTGL1::ASManager::AddMeshPrimitive( uint32_t                   frameIndex,
                                         const RgMeshInfo&          mesh,
                                         const RgMeshPrimitiveInfo& primitive,
                                         uint64_t                   uniqueID,
                                         bool                       isStatic,
                                         const TextureManager&      textureManager,
                                         GeomInfoManager&           geomInfoManager )
{
    auto textures = textureManager.GetTexturesForLayers( primitive );
    auto colors   = textureManager.GetColorForLayers( primitive );

    auto& collector = isStatic ? collectorStatic : collectorDynamic[ frameIndex ];

    return collector->AddPrimitive(
        frameIndex, isStatic, mesh, primitive, uniqueID, textures, colors, geomInfoManager );
}

void RTGL1::ASManager::SubmitDynamicGeometry( DynamicGeometryToken& token,
                                              VkCommandBuffer       cmd,
                                              uint32_t              frameIndex )
{
    assert( token );
    token = {};

    CmdLabel label( cmd, "Building dynamic BLAS" );
    using FT = VertexCollectorFilterTypeFlagBits;

    auto& colDyn = *collectorDynamic[ frameIndex ];

    colDyn.CopyFromStaging( cmd );

    assert( asBuilder->IsEmpty() );

    bool toBuild = false;

    // recreate dynamic blas
    for( auto& dynamicBlas : allDynamicBlas[ frameIndex ] )
    {
        // must be dynamic
        assert( dynamicBlas->GetFilter() & FT::CF_DYNAMIC );

        toBuild |= SetupBLAS( *dynamicBlas, colDyn );
    }

    if( !toBuild )
    {
        return;
    }

    // build BLAS
    asBuilder->BuildBottomLevel( cmd );

    // sync AS access
    Utils::ASBuildMemoryBarrier( cmd );
}

bool RTGL1::ASManager::SetupTLASInstanceFromBLAS( const BLASComponent& blas,
                                                  uint32_t             rayCullMaskWorld,
                                                  bool                 allowGeometryWithSkyFlag,
                                                  VkAccelerationStructureInstanceKHR& instance )
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    if( blas.GetAS() == VK_NULL_HANDLE || blas.IsEmpty() )
    {
        return false;
    }

    auto filter = blas.GetFilter();

    instance.accelerationStructureReference = blas.GetASAddress();

    instance.transform = RG_TRANSFORM_IDENTITY;

    instance.instanceCustomIndex = 0;


    if( filter & FT::CF_DYNAMIC )
    {
        // for choosing buffers with dynamic data
        instance.instanceCustomIndex = INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC;
    }


    if( filter & FT::PV_FIRST_PERSON )
    {
        instance.mask = INSTANCE_MASK_FIRST_PERSON;
        instance.instanceCustomIndex |= INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON;
    }
    else if( filter & FT::PV_FIRST_PERSON_VIEWER )
    {
        instance.mask = INSTANCE_MASK_FIRST_PERSON_VIEWER;
        instance.instanceCustomIndex |= INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON_VIEWER;
    }
    else
    {
        // also check rayCullMaskWorld, if world part is not included in the cull mask,
        // then don't add it to BLAS at all, it helps culling PT_REFLECT if it was a world part

        if( filter & FT::PV_WORLD_0 )
        {
            instance.mask = INSTANCE_MASK_WORLD_0;

            if( !( rayCullMaskWorld & INSTANCE_MASK_WORLD_0 ) )
            {
                instance = {};
                return false;
            }
        }
        else if( filter & FT::PV_WORLD_1 )
        {
            instance.mask = INSTANCE_MASK_WORLD_1;

            if( !( rayCullMaskWorld & INSTANCE_MASK_WORLD_1 ) )
            {
                instance = {};
                return false;
            }
        }
        else if( filter & FT::PV_WORLD_2 )
        {
            instance.mask = INSTANCE_MASK_WORLD_2;

            if( !( rayCullMaskWorld & INSTANCE_MASK_WORLD_2 ) )
            {
                instance = {};
                return false;
            }

#if RAYCULLMASK_SKY_IS_WORLD2
            if( allowGeometryWithSkyFlag )
            {
                instance.instanceCustomIndex |= INSTANCE_CUSTOM_INDEX_FLAG_SKY;
            }
#else
    #error Handle sky, if there is no WORLD_2
#endif
        }
        else
        {
            assert( 0 );
        }
    }


    if( filter & FT::PT_REFRACT )
    {
        // don't touch first-person
        bool isworld =
            !( filter & FT::PV_FIRST_PERSON ) && !( filter & FT::PV_FIRST_PERSON_VIEWER );

        if( isworld )
        {
            // completely rewrite mask, ignoring INSTANCE_MASK_WORLD_*,
            // if mask contains those world bits, then (mask & (~INSTANCE_MASK_REFRACT))
            // won't actually cull INSTANCE_MASK_REFRACT
            instance.mask = INSTANCE_MASK_REFRACT;
        }
    }


    if( filter & FT::PT_ALPHA_TESTED )
    {
        instance.instanceShaderBindingTableRecordOffset = SBT_INDEX_HITGROUP_ALPHA_TESTED;
        instance.flags = VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR |
                         VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR /*|
                         VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR*/
            ;
    }
    else
    {
        assert( ( filter & FT::PT_OPAQUE ) || ( filter & FT::PT_REFRACT ) );

        instance.instanceShaderBindingTableRecordOffset = SBT_INDEX_HITGROUP_FULLY_OPAQUE;
        instance.flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR |
                         VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR /*|
                         VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR*/
            ;
    }


    return true;
}

static void WriteInstanceGeomInfo( int32_t*                    instanceGeomInfoOffset,
                                   int32_t*                    instanceGeomCount,
                                   uint32_t                    index,
                                   const RTGL1::BLASComponent& blas )
{
    assert( index < MAX_TOP_LEVEL_INSTANCE_COUNT );

    uint32_t arrayOffset =
        RTGL1::VertexCollectorFilterTypeFlags_GetOffsetInGlobalArray( blas.GetFilter() );
    uint32_t geomCount = blas.GetGeomCount();

    // BLAS must not be empty, if it's added to TLAS
    assert( geomCount < MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT );

    instanceGeomInfoOffset[ index ] = static_cast< int32_t >( arrayOffset );
    instanceGeomCount[ index ]      = static_cast< int32_t >( geomCount );
}

std::pair< RTGL1::ASManager::TLASPrepareResult, RTGL1::ShVertPreprocessing > RTGL1::ASManager::
    PrepareForBuildingTLAS( uint32_t         frameIndex,
                            ShGlobalUniform& uniformData,
                            uint32_t         uniformData_rayCullMaskWorld,
                            bool             allowGeometryWithSkyFlag,
                            bool             disableRTGeometry ) const
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    static_assert( std::size( TLASPrepareResult{}.instances ) == MAX_TOP_LEVEL_INSTANCE_COUNT,
                   "Change TLASPrepareResult sizes" );


    TLASPrepareResult   r    = {};
    ShVertPreprocessing push = {};


    if( disableRTGeometry )
    {
        return std::make_pair( r, push );
    }


    // write geometry offsets to uniform to access geomInfos
    // with instance ID and local (in terms of BLAS) geometry index in shaders;
    // Note: std140 requires elements to be aligned by sizeof(vec4)
    int32_t* instanceGeomInfoOffset = uniformData.instanceGeomInfoOffset;

    // write geometry counts of each BLAS for iterating in vertex preprocessing
    int32_t* instanceGeomCount = uniformData.instanceGeomCount;

    const std::vector< std::unique_ptr< BLASComponent > >* blasArrays[] = {
        &allStaticBlas,
        &allDynamicBlas[ frameIndex ],
    };

    for( const auto* blasArr : blasArrays )
    {
        for( const auto& blas : *blasArr )
        {
            bool isDynamic = blas->GetFilter() & FT::CF_DYNAMIC;

            // add to TLAS instances array
            bool isAdded = SetupTLASInstanceFromBLAS( *blas,
                                                      uniformData_rayCullMaskWorld,
                                                      allowGeometryWithSkyFlag,
                                                      r.instances[ r.instanceCount ] );

            if( isAdded )
            {
                // mark bit if dynamic
                if( isDynamic )
                {
                    push.tlasInstanceIsDynamicBits[ r.instanceCount /
                                                    MAX_TOP_LEVEL_INSTANCE_COUNT ] |=
                        1 << ( r.instanceCount % MAX_TOP_LEVEL_INSTANCE_COUNT );
                }

                WriteInstanceGeomInfo(
                    instanceGeomInfoOffset, instanceGeomCount, r.instanceCount, *blas );
                r.instanceCount++;
            }
        }
    }

    push.tlasInstanceCount = r.instanceCount;

    return std::make_pair( r, push );
}

void RTGL1::ASManager::BuildTLAS( VkCommandBuffer          cmd,
                                  uint32_t                 frameIndex,
                                  const TLASPrepareResult& r )
{
    CmdLabel label( cmd, "Building TLAS" );


    if( r.instanceCount > 0 )
    {
        // fill buffer
        auto* mapped = static_cast< VkAccelerationStructureInstanceKHR* >(
            instanceBuffer->GetMapped( frameIndex ) );

        memcpy(
            mapped, r.instances, r.instanceCount * sizeof( VkAccelerationStructureInstanceKHR ) );

        instanceBuffer->CopyFromStaging( cmd, frameIndex );
    }


    TLASComponent* pCurrentTLAS = tlas[ frameIndex ].get();


    VkAccelerationStructureGeometryKHR instGeom = {
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .arrayOfPointers = VK_FALSE,
                .data = {
                    .deviceAddress = r.instanceCount > 0 ? instanceBuffer->GetDeviceAddress() : 0,
                },
            },
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    // get AS size and create buffer for AS
    VkAccelerationStructureBuildSizesInfoKHR buildSizes =
        asBuilder->GetTopBuildSizes( &instGeom, r.instanceCount, false );

    // if previous buffer's size is not enough
    pCurrentTLAS->RecreateIfNotValid( buildSizes, allocator );

    VkAccelerationStructureBuildRangeInfoKHR range = {};
    range.primitiveCount                           = r.instanceCount;


    // build
    assert( asBuilder->IsEmpty() );

    assert( pCurrentTLAS->GetAS() != VK_NULL_HANDLE );
    asBuilder->AddTLAS( pCurrentTLAS->GetAS(), &instGeom, &range, buildSizes, true, false );

    asBuilder->BuildTopLevel( cmd );


    // sync AS access
    Utils::ASBuildMemoryBarrier( cmd );


    // shader desc access
    UpdateASDescriptors( frameIndex );
}

void RTGL1::ASManager::CopyDynamicDataToPrevBuffers( VkCommandBuffer cmd, uint32_t frameIndex )
{
    uint32_t vertCount  = collectorDynamic[ frameIndex ]->GetCurrentVertexCount();
    uint32_t indexCount = collectorDynamic[ frameIndex ]->GetCurrentIndexCount();

    if( vertCount > 0 )
    {
        VkBufferCopy vertRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = vertCount * sizeof( ShVertex ),
        };

        vkCmdCopyBuffer( cmd,
                         collectorDynamic[ frameIndex ]->GetVertexBuffer(),
                         previousDynamicPositions.GetBuffer(),
                         1,
                         &vertRegion );
    }

    if( indexCount > 0 )
    {
        VkBufferCopy indexRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = indexCount * sizeof( uint32_t ),
        };

        vkCmdCopyBuffer( cmd,
                         collectorDynamic[ frameIndex ]->GetIndexBuffer(),
                         previousDynamicIndices.GetBuffer(),
                         1,
                         &indexRegion );
    }
}

void RTGL1::ASManager::OnVertexPreprocessingBegin( VkCommandBuffer cmd,
                                                   uint32_t        frameIndex,
                                                   bool            onlyDynamic )
{
    if( !onlyDynamic )
    {
        collectorStatic->InsertVertexPreprocessBeginBarrier( cmd );
    }

    collectorDynamic[ frameIndex ]->InsertVertexPreprocessBeginBarrier( cmd );
}

void RTGL1::ASManager::OnVertexPreprocessingFinish( VkCommandBuffer cmd,
                                                    uint32_t        frameIndex,
                                                    bool            onlyDynamic )
{
    if( !onlyDynamic )
    {
        collectorStatic->InsertVertexPreprocessFinishBarrier( cmd );
    }

    collectorDynamic[ frameIndex ]->InsertVertexPreprocessFinishBarrier( cmd );
}

bool RTGL1::ASManager::IsFastBuild( VertexCollectorFilterTypeFlags filter )
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    // fast trace for static non-movable,
    // fast build for dynamic and movable
    // (TODO: fix: device lost occurs on heavy scenes if with movable)
    return ( filter & FT::CF_DYNAMIC ) /* || (filter & FT::CF_STATIC_MOVABLE)*/;
}

VkDescriptorSet RTGL1::ASManager::GetBuffersDescSet( uint32_t frameIndex ) const
{
    return buffersDescSets[ frameIndex ];
}

VkDescriptorSet RTGL1::ASManager::GetTLASDescSet( uint32_t frameIndex ) const
{
    // if TLAS wasn't built, return null
    if( tlas[ frameIndex ]->GetAS() == VK_NULL_HANDLE )
    {
        return VK_NULL_HANDLE;
    }

    return asDescSets[ frameIndex ];
}

VkDescriptorSetLayout RTGL1::ASManager::GetBuffersDescSetLayout() const
{
    return buffersDescSetLayout;
}

VkDescriptorSetLayout RTGL1::ASManager::GetTLASDescSetLayout() const
{
    return asDescSetLayout;
}
