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

#include "RayTracingPipeline.h"

#include "Generated/ShaderCommonC.h"
#include "Utils.h"

#include <cstring>

namespace RTGL1
{
namespace
{

    template< uint32_t Count >
    VkPipelineLayout CreatePipelineLayout( VkDevice device,
                                           const VkDescriptorSetLayout ( &setLayouts )[ Count ] )
    {
        VkPipelineLayoutCreateInfo info = {
            .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = Count,
            .pSetLayouts    = setLayouts,
        };

        VkPipelineLayout layout;
        VkResult         r = vkCreatePipelineLayout( device, &info, nullptr, &layout );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME(
            device, layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Ray tracing pipeline Layout" );
        return layout;
    }

}
}


RTGL1::RayTracingPipeline::RayTracingPipeline( VkDevice                           _device,
                                               std::shared_ptr< PhysicalDevice >  _physDevice,
                                               std::shared_ptr< MemoryAllocator > _allocator,
                                               const ShaderManager&               _shaderManager,
                                               Scene&                             _scene,
                                               const GlobalUniform&               _uniform,
                                               const TextureManager&              _textureManager,
                                               const Framebuffers&                _framebuffers,
                                               const RestirBuffers&               _restirBuffers,
                                               const BlueNoise&                   _blueNoise,
                                               const CubemapManager&              _cubemapManager,
                                               const RenderCubemap&               _renderCubemap,
                                               const PortalList&                  _portalList,
                                               const Volumetric&                  _volumetric,
                                               const RgInstanceCreateInfo&        _rgInfo )
    : device( _device )
    , physDevice( std::move( _physDevice ) )
    , rtPipelineLayout( VK_NULL_HANDLE )
    , rtPipeline( VK_NULL_HANDLE )
    , copySBTFromStaging( false )
    , groupBaseAlignment( 0 )
    , handleSize( 0 )
    , alignedHandleSize( 0 )
    , raygenShaderCount( 0 )
    , hitGroupCount( 0 )
    , missShaderCount( 0 )
{
    shaderBindingTable = std::make_shared< AutoBuffer >( std::move( _allocator ) );

    // all set layouts to be used
    VkDescriptorSetLayout setLayouts[] = {
        // ray tracing acceleration structures
        _scene.GetASManager()->GetTLASDescSetLayout(),
        // storage images
        _framebuffers.GetDescSetLayout(),
        // uniform
        _uniform.GetDescSetLayout(),
        // vertex data
        _scene.GetASManager()->GetBuffersDescSetLayout(),
        // textures
        _textureManager.GetDescSetLayout(),
        // uniform random
        _blueNoise.GetDescSetLayout(),
        // light sources
        _scene.GetLightManager()->GetDescSetLayout(),
        // cubemaps, for a cubemap type of skyboxes
        _cubemapManager.GetDescSetLayout(),
        // dynamic cubemaps
        _renderCubemap.GetDescSetLayout(),
        // portals
        _portalList.GetDescSetLayout(),
        // device local buffers for restir
        _restirBuffers.GetDescSetLayout(),
        // device local buffers for volumetrics
        _volumetric.GetDescSetLayout(),
    };

    rtPipelineLayout = CreatePipelineLayout( device, setLayouts );

    assert( _rgInfo.primaryRaysMaxAlbedoLayers <= MATERIALS_MAX_LAYER_COUNT );
    assert( _rgInfo.indirectIlluminationMaxAlbedoLayers <= MATERIALS_MAX_LAYER_COUNT );

    // shader modules in the pipeline will have the exact order
    shaderStageInfos = {
        { "RGenPrimary",        std::make_shared< uint32_t >( _rgInfo.primaryRaysMaxAlbedoLayers ) },
        { "RGenReflRefr",       std::make_shared< uint32_t >( _rgInfo.primaryRaysMaxAlbedoLayers ) },
        { "RGenDirect",         nullptr },        
        { "RGenIndirectInit",   std::make_shared< uint32_t >( _rgInfo.indirectIlluminationMaxAlbedoLayers ) },
        { "RGenIndirectFinal",  std::make_shared< uint32_t >( _rgInfo.indirectIlluminationMaxAlbedoLayers ) },
        { "RGenGradients",      nullptr },
        { "RInitialReservoirs", nullptr },
        { "RVolumetric",        nullptr },
        { "RMiss",              nullptr },
        { "RMissShadow",        nullptr },
        { "RClsOpaque",         nullptr },
        { "RAlphaTest",         nullptr },
    };

#pragma region Utilities
    // simple lambda to get index in "stages" by name
    auto toIndex = [ this ]( const char* shaderName ) {
        for( uint32_t i = 0; i < shaderStageInfos.size(); i++ )
        {
            if( std::strcmp( shaderName, shaderStageInfos[ i ].pName ) == 0 )
            {
                return i;
            }
        }

        assert( 0 );
        return UINT32_MAX;
    };
#pragma endregion


    // set shader binding table structure the same as defined with SBT_INDEX_*

    AddRayGenGroup( toIndex( "RGenPrimary" ) );         assert( raygenShaderCount - 1 == SBT_INDEX_RAYGEN_PRIMARY );
    AddRayGenGroup( toIndex( "RGenReflRefr" ) );        assert( raygenShaderCount - 1 == SBT_INDEX_RAYGEN_REFL_REFR );
    AddRayGenGroup( toIndex( "RGenDirect" ) );          assert( raygenShaderCount - 1 == SBT_INDEX_RAYGEN_DIRECT );
    AddRayGenGroup( toIndex( "RGenIndirectInit" ) );    assert( raygenShaderCount - 1 == SBT_INDEX_RAYGEN_INDIRECT_INIT );
    AddRayGenGroup( toIndex( "RGenIndirectFinal" ) );   assert( raygenShaderCount - 1 == SBT_INDEX_RAYGEN_INDIRECT_FINAL );
    AddRayGenGroup( toIndex( "RGenGradients" ) );       assert( raygenShaderCount - 1 == SBT_INDEX_RAYGEN_GRADIENTS );
    AddRayGenGroup( toIndex( "RInitialReservoirs" ) );  assert( raygenShaderCount - 1 == SBT_INDEX_RAYGEN_INITIAL_RESERVOIRS );
    AddRayGenGroup( toIndex( "RVolumetric" ) );         assert( raygenShaderCount - 1 == SBT_INDEX_RAYGEN_VOLUMETRIC );

    AddMissGroup( toIndex( "RMiss" ) );                 assert( missShaderCount - 1 == SBT_INDEX_MISS_DEFAULT );
    AddMissGroup( toIndex( "RMissShadow" ) );           assert( missShaderCount - 1 == SBT_INDEX_MISS_SHADOW );

    // only opaque
    AddHitGroup( toIndex( "RClsOpaque" ) );             assert( hitGroupCount - 1 == SBT_INDEX_HITGROUP_FULLY_OPAQUE );
    // alpha tested and then opaque
    AddHitGroup( toIndex( "RClsOpaque" ), toIndex( "RAlphaTest" ) ); assert( hitGroupCount - 1 == SBT_INDEX_HITGROUP_ALPHA_TESTED );

    CreatePipeline( &_shaderManager );
    CreateSBT();
}

RTGL1::RayTracingPipeline::~RayTracingPipeline()
{
    DestroyPipeline();
    vkDestroyPipelineLayout( device, rtPipelineLayout, nullptr );
}

void RTGL1::RayTracingPipeline::CreatePipeline( const ShaderManager* shaderManager )
{
    std::vector< VkPipelineShaderStageCreateInfo > stages;
    for( const auto& s : shaderStageInfos )
    {
        stages.push_back( shaderManager->GetStageInfo( s.pName ) );
    }

    const VkSpecializationMapEntry specEntryCommonDef = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof( *shaderStageInfos[ 0 ].specConst ),
    };

    std::vector< VkSpecializationInfo > specInfos;
    for( const auto& s : shaderStageInfos )
    {
        VkSpecializationInfo specInfo;

        if( s.specConst )
        {
            specInfo = {
                .mapEntryCount = 1,
                .pMapEntries   = &specEntryCommonDef,
                .dataSize      = sizeof( *s.specConst ),
                // need to be careful with addresses
                .pData = s.specConst.get(),
            };
        }
        else
        {
            specInfo = {};
        }

        specInfos.push_back( specInfo );
    }
    assert( stages.size() == specInfos.size() );


    for( uint32_t i = 0; i < shaderStageInfos.size(); i++ )
    {
        if( shaderStageInfos[ i ].specConst )
        {
            // need to be careful with addresses
            stages[ i ].pSpecializationInfo = &specInfos[ i ];
        }
    }

    VkPipelineLibraryCreateInfoKHR libInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR,
    };

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {
        .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount                   = static_cast< uint32_t >( stages.size() ),
        .pStages                      = stages.data(),
        .groupCount                   = static_cast< uint32_t >( shaderGroups.size() ),
        .pGroups                      = shaderGroups.data(),
        .maxPipelineRayRecursionDepth = 2,
        .pLibraryInfo                 = &libInfo,
        .layout                       = rtPipelineLayout,
    };

    VkResult r = svkCreateRayTracingPipelinesKHR(
        device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rtPipeline );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, rtPipeline, VK_OBJECT_TYPE_PIPELINE, "Ray tracing pipeline" );
}

void RTGL1::RayTracingPipeline::DestroyPipeline()
{
    vkDestroyPipeline( device, rtPipeline, nullptr );
    rtPipeline = VK_NULL_HANDLE;
}

void RTGL1::RayTracingPipeline::CreateSBT()
{
    uint32_t groupCount = uint32_t( shaderGroups.size() );
    groupBaseAlignment  = physDevice->GetRTPipelineProperties().shaderGroupBaseAlignment;

    handleSize        = physDevice->GetRTPipelineProperties().shaderGroupHandleSize;
    alignedHandleSize = Utils::Align( handleSize, groupBaseAlignment );

    uint32_t sbtSize = alignedHandleSize * groupCount;

    shaderBindingTable->Create( sbtSize,
                                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                "SBT",
                                1 );

    std::vector< uint8_t > shaderHandles( uint64_t( handleSize * groupCount ) );
    VkResult               r = svkGetRayTracingShaderGroupHandlesKHR(
        device, rtPipeline, 0, groupCount, shaderHandles.size(), shaderHandles.data() );
    VK_CHECKERROR( r );

    auto* mapped = shaderBindingTable->GetMappedAs< uint8_t* >( 0 );

    for( uint32_t i = 0; i < groupCount; i++ )
    {
        memcpy( mapped + uint64_t( i * alignedHandleSize ),
                shaderHandles.data() + uint64_t( i * handleSize ),
                handleSize );
    }

    copySBTFromStaging = true;
}

void RTGL1::RayTracingPipeline::DestroySBT()
{
    shaderBindingTable->Destroy();
}

void RTGL1::RayTracingPipeline::Bind( VkCommandBuffer cmd )
{
    if( copySBTFromStaging )
    {
        shaderBindingTable->CopyFromStaging( cmd, 0 );
        copySBTFromStaging = false;
    }

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline );
}

void RTGL1::RayTracingPipeline::GetEntries( uint32_t                         sbtRayGenIndex,
                                            VkStridedDeviceAddressRegionKHR& raygenEntry,
                                            VkStridedDeviceAddressRegionKHR& missEntry,
                                            VkStridedDeviceAddressRegionKHR& hitEntry,
                                            VkStridedDeviceAddressRegionKHR& callableEntry ) const
{
    assert( sbtRayGenIndex == SBT_INDEX_RAYGEN_PRIMARY ||
            sbtRayGenIndex == SBT_INDEX_RAYGEN_REFL_REFR ||
            sbtRayGenIndex == SBT_INDEX_RAYGEN_DIRECT ||
            sbtRayGenIndex == SBT_INDEX_RAYGEN_INDIRECT_INIT ||
            sbtRayGenIndex == SBT_INDEX_RAYGEN_INDIRECT_FINAL ||
            sbtRayGenIndex == SBT_INDEX_RAYGEN_GRADIENTS ||
            sbtRayGenIndex == SBT_INDEX_RAYGEN_INITIAL_RESERVOIRS ||
            sbtRayGenIndex == SBT_INDEX_RAYGEN_VOLUMETRIC );

    VkDeviceAddress bufferAddress = shaderBindingTable->GetDeviceAddress();

    uint64_t        offset = 0;


    raygenEntry = {
        .deviceAddress = bufferAddress + offset + uint64_t( sbtRayGenIndex ) * alignedHandleSize,
        .stride        = alignedHandleSize,
        .size          = alignedHandleSize,
    };
    assert( raygenEntry.size == raygenEntry.stride );
    offset += uint64_t( raygenShaderCount ) * alignedHandleSize;


    missEntry = {
        .deviceAddress = bufferAddress + offset,
        .stride        = alignedHandleSize,
        .size          = uint64_t( missShaderCount ) * alignedHandleSize,
    };
    offset += uint64_t( missShaderCount ) * alignedHandleSize;


    hitEntry = {
        .deviceAddress = bufferAddress + offset,
        .stride        = alignedHandleSize,
        .size          = uint64_t( hitGroupCount ) * alignedHandleSize,
    };
    offset += uint64_t( hitGroupCount ) * alignedHandleSize;


    callableEntry = {};
}

VkPipelineLayout RTGL1::RayTracingPipeline::GetLayout() const
{
    return rtPipelineLayout;
}

void RTGL1::RayTracingPipeline::OnShaderReload( const ShaderManager* shaderManager )
{
    DestroySBT();
    DestroyPipeline();

    CreatePipeline( shaderManager );
    CreateSBT();
}

void RTGL1::RayTracingPipeline::AddGeneralGroup( uint32_t generalIndex )
{
    shaderGroups.push_back( VkRayTracingShaderGroupCreateInfoKHR{
        .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader      = generalIndex,
        .closestHitShader   = VK_SHADER_UNUSED_KHR,
        .anyHitShader       = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    } );
}

void RTGL1::RayTracingPipeline::AddRayGenGroup( uint32_t raygenIndex )
{
    AddGeneralGroup( raygenIndex );

    raygenShaderCount++;
}

void RTGL1::RayTracingPipeline::AddMissGroup( uint32_t missIndex )
{
    AddGeneralGroup( missIndex );

    missShaderCount++;
}

void RTGL1::RayTracingPipeline::AddHitGroup( uint32_t closestHitIndex )
{
    AddHitGroup( closestHitIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR );
}

void RTGL1::RayTracingPipeline::AddHitGroupOnlyAny( uint32_t anyHitIndex )
{
    AddHitGroup( VK_SHADER_UNUSED_KHR, anyHitIndex, VK_SHADER_UNUSED_KHR );
}

void RTGL1::RayTracingPipeline::AddHitGroup( uint32_t closestHitIndex, uint32_t anyHitIndex )
{
    AddHitGroup( closestHitIndex, anyHitIndex, VK_SHADER_UNUSED_KHR );
}

void RTGL1::RayTracingPipeline::AddHitGroup( uint32_t closestHitIndex,
                                             uint32_t anyHitIndex,
                                             uint32_t intersectionIndex )
{
    shaderGroups.push_back( VkRayTracingShaderGroupCreateInfoKHR{
        .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader      = VK_SHADER_UNUSED_KHR,
        .closestHitShader   = closestHitIndex,
        .anyHitShader       = anyHitIndex,
        .intersectionShader = intersectionIndex,
    } );

    hitGroupCount++;
}
