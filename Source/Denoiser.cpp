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

#include "Denoiser.h"

#include <cmath>
#include "Generated/ShaderCommonC.h"
#include "CmdLabel.h"
#include "Utils.h"

RTGL1::Denoiser::Denoiser( VkDevice                        _device,
                           std::shared_ptr< Framebuffers > _framebuffers,
                           const ShaderManager&            _shaderManager,
                           const GlobalUniform&            _uniform )
    : device( _device )
    , framebuffers( std::move( _framebuffers ) )
    , pipelineLayout( VK_NULL_HANDLE )
    , gradientAtrous{}
    , antifirefly( VK_NULL_HANDLE )
    , temporalAccumulation( VK_NULL_HANDLE )
    , varianceEstimation( VK_NULL_HANDLE )
    , atrous{}
{
    static_assert( sizeof( atrous ) / sizeof( VkPipeline ) == COMPUTE_SVGF_ATROUS_ITERATION_COUNT,
                   "Wrong atrous pipeline count" );
    static_assert( sizeof( gradientAtrous ) / sizeof( VkPipeline ) ==
                       COMPUTE_ASVGF_GRADIENT_ATROUS_ITERATION_COUNT,
                   "Wrong gradient atrous pipeline count" );


    VkDescriptorSetLayout setLayouts[] = {
        framebuffers->GetDescSetLayout(),
        _uniform.GetDescSetLayout(),
    };

    CreatePipelineLayout( setLayouts, std::size( setLayouts ) );
    CreatePipelines( &_shaderManager );
}

RTGL1::Denoiser::~Denoiser()
{
    vkDestroyPipelineLayout( device, pipelineLayout, nullptr );

    DestroyPipelines();
}

void RTGL1::Denoiser::Denoise( VkCommandBuffer                               cmd,
                               uint32_t                                      frameIndex,
                               const std::shared_ptr< const GlobalUniform >& uniform )
{
    typedef FramebufferImageIndex FI;


    // bind desc sets
    VkDescriptorSet sets[] = {
        framebuffers->GetDescSet( frameIndex ),
        uniform->GetDescSet( frameIndex ),
    };

    vkCmdBindDescriptorSets( cmd,
                             VK_PIPELINE_BIND_POINT_COMPUTE,
                             pipelineLayout,
                             0,
                             std::size( sets ),
                             sets,
                             0,
                             nullptr );


#if GRADIENT_ESTIMATION_ENABLED
    // gradient atrous
    {
        CmdLabel label( cmd, "Gradient Atrous" );

        for( uint32_t i = 0; i < COMPUTE_ASVGF_GRADIENT_ATROUS_ITERATION_COUNT; i++ )
        {
            uint32_t wgGradCountX = Utils::GetWorkGroupCount(
                uniform->GetData()->renderWidth / COMPUTE_ASVGF_STRATA_SIZE,
                COMPUTE_GRADIENT_ATROUS_GROUP_SIZE_X );
            uint32_t wgGradCountY = Utils::GetWorkGroupCount(
                uniform->GetData()->renderHeight / COMPUTE_ASVGF_STRATA_SIZE,
                COMPUTE_GRADIENT_ATROUS_GROUP_SIZE_X );

            if( i % 2 == 0 )
            {
                FI fs[] = {
                    FI::FB_IMAGE_INDEX_D_I_S_PING_GRADIENT,
                };
                framebuffers->BarrierMultiple( cmd, frameIndex, fs );
            }
            else
            {
                FI fs[] = {
                    FI::FB_IMAGE_INDEX_D_I_S_PONG_GRADIENT,
                };
                framebuffers->BarrierMultiple( cmd, frameIndex, fs );
            }

            vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientAtrous[ i ] );
            vkCmdDispatch( cmd, wgGradCountX, wgGradCountY, 1 );
        }
    }
#endif // GRADIENT_ESTIMATION_ENABLED


    // temporal accumulation
    {
        uint32_t wgCountX = Utils::GetWorkGroupCount( uniform->GetData()->renderWidth,
                                                      COMPUTE_SVGF_TEMPORAL_GROUP_SIZE_X );
        uint32_t wgCountY = Utils::GetWorkGroupCount( uniform->GetData()->renderHeight,
                                                      COMPUTE_SVGF_TEMPORAL_GROUP_SIZE_X );

        CmdLabel label( cmd, "Temporal accumulation" );

        FI fs[] = {
            FI::FB_IMAGE_INDEX_MOTION,
            FI::FB_IMAGE_INDEX_DEPTH_WORLD,
            FI::FB_IMAGE_INDEX_DEPTH_GRAD,
            FI::FB_IMAGE_INDEX_NORMAL,
            FI::FB_IMAGE_INDEX_METALLIC_ROUGHNESS,
            FI::FB_IMAGE_INDEX_SURFACE_POSITION,
            FI::FB_IMAGE_INDEX_VIEW_DIRECTION,
            FI::FB_IMAGE_INDEX_UNFILTERED_DIRECT,
            FI::FB_IMAGE_INDEX_UNFILTERED_SPECULAR,
            FI::FB_IMAGE_INDEX_UNFILTERED_INDIR,
            FI::FB_IMAGE_INDEX_DIFF_COLOR_HISTORY,
#if GRADIENT_ESTIMATION_ENABLED
            FI::FB_IMAGE_INDEX_D_I_S_PING_GRADIENT,
#endif
        };
        framebuffers->BarrierMultiple( cmd, frameIndex, fs );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, temporalAccumulation );
        vkCmdDispatch( cmd, wgCountX, wgCountY, 1 );
    }


    // antifirefly
    {
        uint32_t x = Utils::GetWorkGroupCount( uniform->GetData()->renderWidth,
                                               COMPUTE_ANTIFIREFLY_GROUP_SIZE_X );
        uint32_t y = Utils::GetWorkGroupCount( uniform->GetData()->renderHeight,
                                               COMPUTE_ANTIFIREFLY_GROUP_SIZE_X );

        CmdLabel label( cmd, "Antifirefly" );

        FI fs[] = {
            FI::FB_IMAGE_INDEX_DIFF_ACCUM_COLOR,
            FI::FB_IMAGE_INDEX_SPEC_ACCUM_COLOR,
            FI::FB_IMAGE_INDEX_INDIR_ACCUM,
        };
        framebuffers->BarrierMultiple( cmd, frameIndex, fs );


        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, antifirefly );
        vkCmdDispatch( cmd, x, y, 1 );
    }


    // variance estimation
    {
        uint32_t wgCountX = Utils::GetWorkGroupCount( uniform->GetData()->renderWidth,
                                                      COMPUTE_SVGF_VARIANCE_GROUP_SIZE_X );
        uint32_t wgCountY = Utils::GetWorkGroupCount( uniform->GetData()->renderHeight,
                                                      COMPUTE_SVGF_VARIANCE_GROUP_SIZE_X );

        CmdLabel label( cmd, "SVGF Variance estimation" );

        FI fs[] = { FI::FB_IMAGE_INDEX_DIFF_ACCUM_COLOR,
                    FI::FB_IMAGE_INDEX_DIFF_ACCUM_MOMENTS,
                    FI::FB_IMAGE_INDEX_ACCUM_HISTORY_LENGTH };
        framebuffers->BarrierMultiple( cmd, frameIndex, fs );

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, varianceEstimation );
        vkCmdDispatch( cmd, wgCountX, wgCountY, 1 );
    }


    // atrous

    for( uint32_t i = 0; i < COMPUTE_SVGF_ATROUS_ITERATION_COUNT; i++ )
    {
        uint32_t wgCountX = Utils::GetWorkGroupCount( uniform->GetData()->renderWidth,
                                                      COMPUTE_SVGF_ATROUS_GROUP_SIZE_X );
        uint32_t wgCountY = Utils::GetWorkGroupCount( uniform->GetData()->renderHeight,
                                                      COMPUTE_SVGF_ATROUS_GROUP_SIZE_X );

        CmdLabel label( cmd, "SVGF Atrous" );

        switch( i )
        {
            case 0: {
                FI fs[] = { FI::FB_IMAGE_INDEX_DIFF_PING_COLOR_AND_VARIANCE,
                            FI::FB_IMAGE_INDEX_SPEC_PING_COLOR,
                            FI::FB_IMAGE_INDEX_INDIR_PING,

                            FI::FB_IMAGE_INDEX_METALLIC_ROUGHNESS };

                framebuffers->BarrierMultiple( cmd, frameIndex, fs );
                break;
            }
            case 1: {
                FI fs[] = { FI::FB_IMAGE_INDEX_DIFF_COLOR_HISTORY,
                            FI::FB_IMAGE_INDEX_SPEC_PONG_COLOR,
                            FI::FB_IMAGE_INDEX_INDIR_PONG,
                            // on iteration 0 prefiltered variance was calculated
                            FI::FB_IMAGE_INDEX_ATROUS_FILTERED_VARIANCE };

                framebuffers->BarrierMultiple( cmd, frameIndex, fs );
                break;
            }
            case 2: {
                FI fs[] = { FI::FB_IMAGE_INDEX_DIFF_PING_COLOR_AND_VARIANCE,
                            FI::FB_IMAGE_INDEX_SPEC_PING_COLOR,
                            FI::FB_IMAGE_INDEX_INDIR_PING };

                framebuffers->BarrierMultiple( cmd, frameIndex, fs );
                break;
            }
            case 3: {
                FI fs[] = { FI::FB_IMAGE_INDEX_DIFF_PONG_COLOR_AND_VARIANCE,
                            FI::FB_IMAGE_INDEX_SPEC_PONG_COLOR,
                            FI::FB_IMAGE_INDEX_INDIR_PONG,
                            FI::FB_IMAGE_INDEX_THROUGHPUT };

                framebuffers->BarrierMultiple( cmd, frameIndex, fs );
                break;
            }
            default: {
                assert( 0 );
                return;
            }
        }

        vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, atrous[ i ] );
        vkCmdDispatch( cmd, wgCountX, wgCountY, 1 );
    }
}

void RTGL1::Denoiser::OnShaderReload( const ShaderManager* shaderManager )
{
    DestroyPipelines();
    CreatePipelines( shaderManager );
}

void RTGL1::Denoiser::CreatePipelineLayout( VkDescriptorSetLayout* pSetLayouts,
                                            uint32_t               setLayoutCount )
{
    VkPipelineLayoutCreateInfo plLayoutInfo = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = setLayoutCount,
        .pSetLayouts    = pSetLayouts,
    };

    VkResult r = vkCreatePipelineLayout( device, &plLayoutInfo, nullptr, &pipelineLayout );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME(
        device, pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Denoiser pipeline layout" );
}

void RTGL1::Denoiser::DestroyPipelines()
{
    vkDestroyPipeline( device, antifirefly, nullptr );
    vkDestroyPipeline( device, temporalAccumulation, nullptr );
    vkDestroyPipeline( device, varianceEstimation, nullptr );

    for( VkPipeline& p : gradientAtrous )
    {
        vkDestroyPipeline( device, p, nullptr );
        p = VK_NULL_HANDLE;
    }

    for( VkPipeline& p : atrous )
    {
        vkDestroyPipeline( device, p, nullptr );
        p = VK_NULL_HANDLE;
    }

    antifirefly          = VK_NULL_HANDLE;
    temporalAccumulation = VK_NULL_HANDLE;
    varianceEstimation   = VK_NULL_HANDLE;
}

void RTGL1::Denoiser::CreatePipelines( const ShaderManager* shaderManager )
{
    uint32_t gAtrousIteration = 0;

    VkSpecializationMapEntry specEntry = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof( uint32_t ),
    };

    VkSpecializationInfo specInfo = {
        .mapEntryCount = 1,
        .pMapEntries   = &specEntry,
        .dataSize      = sizeof( uint32_t ),
        .pData         = &gAtrousIteration,
    };

    {
        const char* debugNames[ COMPUTE_ASVGF_GRADIENT_ATROUS_ITERATION_COUNT ] = {
            "ASVGF Gradient atrous iteration #0 pipeline",
            "ASVGF Gradient atrous iteration #1 pipeline",
            "ASVGF Gradient atrous iteration #2 pipeline",
            "ASVGF Gradient atrous iteration #3 pipeline",
        };

        VkComputePipelineCreateInfo plInfo = {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage  = shaderManager->GetStageInfo( "CASVGFGradientAtrous" ),
            .layout = pipelineLayout,
        };
        plInfo.stage.pSpecializationInfo = &specInfo;

        for( uint32_t i = 0; i < COMPUTE_ASVGF_GRADIENT_ATROUS_ITERATION_COUNT; i++ )
        {
            gAtrousIteration = i;

            VkResult r = vkCreateComputePipelines(
                device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &gradientAtrous[ i ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME( device, gradientAtrous[ i ], VK_OBJECT_TYPE_PIPELINE, debugNames[ i ] );
        }
    }

    {
        VkComputePipelineCreateInfo plInfo = {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage  = shaderManager->GetStageInfo( "CSVGFTemporalAccum" ),
            .layout = pipelineLayout,
        };

        VkResult r = vkCreateComputePipelines(
            device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &temporalAccumulation );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device,
                        temporalAccumulation,
                        VK_OBJECT_TYPE_PIPELINE,
                        "SVGF Temporal accumulation pipeline" );
    }

    {
        VkComputePipelineCreateInfo plInfo = {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage  = shaderManager->GetStageInfo( "CAntiFirefly" ),
            .layout = pipelineLayout,
        };

        VkResult r =
            vkCreateComputePipelines( device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &antifirefly );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device, antifirefly, VK_OBJECT_TYPE_PIPELINE, "Antifirefly pipeline" );
    }

    {
        VkComputePipelineCreateInfo plInfo = {
            .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage  = shaderManager->GetStageInfo( "CSVGFVarianceEstim" ),
            .layout = pipelineLayout,
        };

        VkResult r = vkCreateComputePipelines(
            device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &varianceEstimation );

        VK_CHECKERROR( r );
        SET_DEBUG_NAME( device,
                        varianceEstimation,
                        VK_OBJECT_TYPE_PIPELINE,
                        "SVGF Variance estimation pipeline" );
    }

    {
        const char* debugNames[ COMPUTE_SVGF_ATROUS_ITERATION_COUNT ] = {
            "SVGF Atrous iteration #0 pipeline",
            "SVGF Atrous iteration #1 pipeline",
            "SVGF Atrous iteration #2 pipeline",
            "SVGF Atrous iteration #3 pipeline",
        };

        // special iteration 0
        {
            VkComputePipelineCreateInfo plInfo = {
                .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage  = shaderManager->GetStageInfo( "CSVGFAtrous_Iter0" ),
                .layout = pipelineLayout,
            };

            VkResult r = vkCreateComputePipelines(
                device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &atrous[ 0 ] );

            VK_CHECKERROR( r );
            SET_DEBUG_NAME( device, atrous[ 0 ], VK_OBJECT_TYPE_PIPELINE, debugNames[ 0 ] );
        }

        {
            VkComputePipelineCreateInfo plInfo = {
                .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .stage  = shaderManager->GetStageInfo( "CSVGFAtrous" ),
                .layout = pipelineLayout,
            };
            plInfo.stage.pSpecializationInfo = &specInfo;

            for( uint32_t i = 1; i < COMPUTE_SVGF_ATROUS_ITERATION_COUNT; i++ )
            {
                gAtrousIteration = i;

                VkResult r = vkCreateComputePipelines(
                    device, VK_NULL_HANDLE, 1, &plInfo, nullptr, &atrous[ i ] );

                VK_CHECKERROR( r );
                SET_DEBUG_NAME( device, atrous[ i ], VK_OBJECT_TYPE_PIPELINE, debugNames[ i ] );
            }
        }
    }
}
