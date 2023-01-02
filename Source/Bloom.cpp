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

#include "Bloom.h"

#include "CmdLabel.h"
#include "RenderResolutionHelper.h"
#include "TextureManager.h"
#include "Utils.h"

#include "Generated/ShaderCommonC.h"

namespace
{
VkPipelineLayout CreatePipelineLayout( VkDevice                           device,
                                       std::span< VkDescriptorSetLayout > setLayouts,
                                       std::string_view                   name )
{
    VkPipelineLayout layout = VK_NULL_HANDLE;

    VkPipelineLayoutCreateInfo info = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast< uint32_t >( setLayouts.size() ),
        .pSetLayouts    = setLayouts.data(),
    };

    VkResult r = vkCreatePipelineLayout( device, &info, nullptr, &layout );
    RTGL1::VK_CHECKERROR( r );

    SET_DEBUG_NAME( device, layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, name.data() );

    return layout;
}
}

RTGL1::Bloom::Bloom( VkDevice                        _device,
                     std::shared_ptr< Framebuffers > _framebuffers,
                     const ShaderManager&            _shaderManager,
                     const GlobalUniform&            _uniform,
                     const TextureManager&           _textureManager,
                     const Tonemapping&              _tonemapping )
    : device( _device )
    , framebuffers( std::move( _framebuffers ) )
    , pipelineLayout( VK_NULL_HANDLE )
    , downsamplePipelines{}
    , upsamplePipelines{}
    , applyPipelines{}
{
    {
        VkDescriptorSetLayout setLayouts[] = {
            framebuffers->GetDescSetLayout(),
            _uniform.GetDescSetLayout(),
            _tonemapping.GetDescSetLayout(),
        };
        pipelineLayout = CreatePipelineLayout( device, setLayouts, "Bloom layout" );
    }
    {
        VkDescriptorSetLayout setLayouts[] = {
            framebuffers->GetDescSetLayout(),
            _uniform.GetDescSetLayout(),
            _textureManager.GetDescSetLayout(),
        };
        applyPipelineLayout = CreatePipelineLayout( device, setLayouts, "Bloom apply layout" );
    }
    CreatePipelines( &_shaderManager );

    static_assert( StepCount == COMPUTE_BLOOM_STEP_COUNT, "Recheck COMPUTE_BLOOM_STEP_COUNT" );
}

RTGL1::Bloom::~Bloom()
{
    vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
    vkDestroyPipelineLayout( device, applyPipelineLayout, nullptr );
    DestroyPipelines();
}

void RTGL1::Bloom::Prepare( VkCommandBuffer      cmd,
                            uint32_t             frameIndex,
                            const GlobalUniform& uniform,
                            const Tonemapping&   tonemapping )
{
    VkMemoryBarrier2KHR memoryBarrier = {
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR,
        .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
        .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR | VK_ACCESS_2_SHADER_READ_BIT_KHR,
    };

    VkDependencyInfoKHR dependencyInfo = {
        .sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
        .memoryBarrierCount = 1,
        .pMemoryBarriers    = &memoryBarrier,
    };

    // bind desc sets
    VkDescriptorSet sets[] = {
        framebuffers->GetDescSet( frameIndex ),
        uniform.GetDescSet( frameIndex ),
        tonemapping.GetDescSet(),
    };

    vkCmdBindDescriptorSets( cmd,
                             VK_PIPELINE_BIND_POINT_COMPUTE,
                             pipelineLayout,
                             0,
                             std::size( sets ),
                             sets,
                             0,
                             nullptr );

    for( int i = 0; i < COMPUTE_BLOOM_STEP_COUNT; i++ )
    {
        CmdLabel label( cmd, "Bloom downsample iteration" );

        const float w = uniform.GetData()->renderWidth / float( 1 << ( i + 1 ) );
        const float h = uniform.GetData()->renderHeight / float( 1 << ( i + 1 ) );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, downsamplePipelines[ i ] );

        switch( i )
        {
            case 0: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_INPUT ); break;
            case 1: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP1 ); break;
            case 2: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP2 ); break;
            case 3: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP3 ); break;
            case 4: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP4 ); break;
            case 5: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP5 ); break;
            case 6: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP6 ); break;
            case 7: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP7 ); break;
            default: assert( 0 );
        }

        vkCmdDispatch( cmd,
                       Utils::GetWorkGroupCount( w, COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_X ),
                       Utils::GetWorkGroupCount( h, COMPUTE_BLOOM_DOWNSAMPLE_GROUP_SIZE_Y ),
                       1 );
    }


    svkCmdPipelineBarrier2KHR( cmd, &dependencyInfo );


    // start from the other side
    for( int i = COMPUTE_BLOOM_STEP_COUNT - 1; i >= 0; i-- )
    {
        CmdLabel label( cmd, "Bloom upsample iteration" );

        const float w = uniform.GetData()->renderWidth / float( 1 << i );
        const float h = uniform.GetData()->renderHeight / float( 1 << i );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, upsamplePipelines[ i ] );

        switch( i )
        {
            case 7: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP8 ); break;
            case 6: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP7 ); break;
            case 5: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP6 ); break;
            case 4: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP5 ); break;
            case 3: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP4 ); break;
            case 2: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP3 ); break;
            case 1: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP2 ); break;
            case 0: framebuffers->BarrierOne( cmd, frameIndex, FB_IMAGE_INDEX_BLOOM_MIP1 ); break;
            default: assert( 0 );
        }

        vkCmdDispatch( cmd,
                       Utils::GetWorkGroupCount( w, COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_X ),
                       Utils::GetWorkGroupCount( h, COMPUTE_BLOOM_UPSAMPLE_GROUP_SIZE_Y ),
                       1 );
    }


    svkCmdPipelineBarrier2KHR( cmd, &dependencyInfo );
}

RTGL1::FramebufferImageIndex RTGL1::Bloom::Apply( VkCommandBuffer       cmd,
                                                  uint32_t              frameIndex,
                                                  const GlobalUniform&  uniform,
                                                  const TextureManager& textureManager,
                                                  uint32_t              width,
                                                  uint32_t              height,
                                                  FramebufferImageIndex inputFramebuf )
{
    CmdLabel label( cmd, "Bloom apply" );


    assert( inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PING ||
            inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PONG );
    uint32_t isSourcePing = inputFramebuf == FB_IMAGE_INDEX_UPSCALED_PING;


    // bind desc sets
    VkDescriptorSet sets[] = {
        framebuffers->GetDescSet( frameIndex ),
        uniform.GetDescSet( frameIndex ),
        textureManager.GetDescSet( frameIndex ),
    };

    vkCmdBindDescriptorSets( cmd,
                             VK_PIPELINE_BIND_POINT_COMPUTE,
                             applyPipelineLayout,
                             0,
                             std::size( sets ),
                             sets,
                             0,
                             nullptr );

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, applyPipelines[ isSourcePing ] );

    FramebufferImageIndex fs[] = {
        inputFramebuf,
        FB_IMAGE_INDEX_BLOOM_RESULT,
    };
    framebuffers->BarrierMultiple( cmd, frameIndex, fs );

    vkCmdDispatch( cmd,
                   Utils::GetWorkGroupCount( width, COMPUTE_BLOOM_APPLY_GROUP_SIZE_X ),
                   Utils::GetWorkGroupCount( height, COMPUTE_BLOOM_APPLY_GROUP_SIZE_Y ),
                   1 );


    return isSourcePing ? FB_IMAGE_INDEX_UPSCALED_PONG : FB_IMAGE_INDEX_UPSCALED_PING;
}

void RTGL1::Bloom::OnShaderReload( const ShaderManager* shaderManager )
{
    DestroyPipelines();
    CreatePipelines( shaderManager );
}

void RTGL1::Bloom::CreatePipelines( const ShaderManager* shaderManager )
{
    CreateStepPipelines( shaderManager );
    CreateApplyPipelines( shaderManager );
}

void RTGL1::Bloom::CreateStepPipelines( const ShaderManager* shaderManager )
{
    assert( pipelineLayout != VK_NULL_HANDLE );

    const char* dnsmplDebugNames[] = {
        "Bloom downsample 0 pipeline", "Bloom downsample 1 pipeline", "Bloom downsample 2 pipeline",
        "Bloom downsample 3 pipeline", "Bloom downsample 4 pipeline", "Bloom downsample 5 pipeline",
        "Bloom downsample 6 pipeline", "Bloom downsample 7 pipeline",
    };
    static_assert( COMPUTE_BLOOM_STEP_COUNT == std::size( dnsmplDebugNames ) );

    const char* upsmplDebugNames[] = {
        "Bloom upsample 0 pipeline", "Bloom upsample 1 pipeline", "Bloom upsample 2 pipeline",
        "Bloom upsample 3 pipeline", "Bloom upsample 4 pipeline", "Bloom upsample 5 pipeline",
        "Bloom upsample 6 pipeline", "Bloom upsample 7 pipeline",
    };
    static_assert( COMPUTE_BLOOM_STEP_COUNT == std::size( upsmplDebugNames ) );


    for( uint32_t i = 0; i < COMPUTE_BLOOM_STEP_COUNT; i++ )
    {
        assert( downsamplePipelines[ i ] == VK_NULL_HANDLE );
        assert( upsamplePipelines[ i ] == VK_NULL_HANDLE );

        VkSpecializationMapEntry specEntry = {
            .constantID = 0,
            .offset     = 0,
            .size       = sizeof( uint32_t ),
        };

        VkSpecializationInfo specInfo = {
            .mapEntryCount = 1,
            .pMapEntries   = &specEntry,
            .dataSize      = sizeof( uint32_t ),
            .pData         = &i,
        };

        {
            VkComputePipelineCreateInfo info = {
                .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage  = shaderManager->GetStageInfo( "CBloomDownsample" ),
                .layout = pipelineLayout,
            };
            info.stage.pSpecializationInfo = &specInfo;

            VkResult r = vkCreateComputePipelines(
                device, VK_NULL_HANDLE, 1, &info, nullptr, &downsamplePipelines[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME(
                device, downsamplePipelines[ i ], VK_OBJECT_TYPE_PIPELINE, dnsmplDebugNames[ i ] );
        }

        {
            VkComputePipelineCreateInfo info = {
                .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage  = shaderManager->GetStageInfo( "CBloomUpsample" ),
                .layout = pipelineLayout,
            };
            info.stage.pSpecializationInfo = &specInfo;

            VkResult r = vkCreateComputePipelines(
                device, VK_NULL_HANDLE, 1, &info, nullptr, &upsamplePipelines[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME(
                device, upsamplePipelines[ i ], VK_OBJECT_TYPE_PIPELINE, upsmplDebugNames[ i ] );
        }
    }
}

void RTGL1::Bloom::CreateApplyPipelines( const ShaderManager* shaderManager )
{
    assert( applyPipelineLayout != VK_NULL_HANDLE );

    for( VkPipeline t : applyPipelines )
    {
        assert( t == VK_NULL_HANDLE );
    }

    for( uint32_t isSourcePing = 0; isSourcePing <= 1; isSourcePing++ )
    {
        VkSpecializationMapEntry specEntry = {
            .constantID = 0,
            .offset     = 0,
            .size       = sizeof( isSourcePing ),
        };

        VkSpecializationInfo specInfo = {
            .mapEntryCount = 1,
            .pMapEntries   = &specEntry,
            .dataSize      = sizeof( isSourcePing ),
            .pData         = &isSourcePing,
        };

        VkComputePipelineCreateInfo info = {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage  = shaderManager->GetStageInfo( "CBloomApply" ),
            .layout = applyPipelineLayout,
        };
        info.stage.pSpecializationInfo = &specInfo;

        VkResult r = vkCreateComputePipelines(
            device, VK_NULL_HANDLE, 1, &info, nullptr, &applyPipelines[ isSourcePing ] );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME(
            device,
            applyPipelines[ isSourcePing ],
            VK_OBJECT_TYPE_PIPELINE,
            ( "Bloom apply from " + std::string( isSourcePing ? "Ping" : "Pong" ) ).c_str() );
    }
}

void RTGL1::Bloom::DestroyPipelines()
{
    for( VkPipeline& p : downsamplePipelines )
    {
        vkDestroyPipeline( device, p, nullptr );
        p = VK_NULL_HANDLE;
    }

    for( VkPipeline& p : upsamplePipelines )
    {
        vkDestroyPipeline( device, p, nullptr );
        p = VK_NULL_HANDLE;
    }

    for( VkPipeline& t : applyPipelines )
    {
        vkDestroyPipeline( device, t, nullptr );
        t = VK_NULL_HANDLE;
    }
}
