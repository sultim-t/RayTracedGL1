// Copyright (c) 2022 Sultim Tsyrendashiev
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

#include "DecalManager.h"

#include "CmdLabel.h"
#include "Matrix.h"
#include "Generated/ShaderCommonC.h"


constexpr uint32_t DECAL_MAX_COUNT = 4096;

constexpr uint32_t            CUBE_VERTEX_COUNT = 14;
constexpr VkPrimitiveTopology CUBE_TOPOLOGY     = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;


RTGL1::DecalManager::DecalManager( VkDevice                           _device,
                                   std::shared_ptr< MemoryAllocator > _allocator,
                                   std::shared_ptr< Framebuffers >    _storageFramebuffers,
                                   const ShaderManager&               _shaderManager,
                                   const GlobalUniform&               _uniform,
                                   const TextureManager&              _textureManager )
    : device( _device )
    , storageFramebuffers( std::move( _storageFramebuffers ) )
    , decalCount( 0 )
    , renderPass( VK_NULL_HANDLE )
    , passFramebuffers{}
    , pipelineLayout( VK_NULL_HANDLE )
    , pipeline( VK_NULL_HANDLE )
    , descPool( VK_NULL_HANDLE )
    , descSetLayout( VK_NULL_HANDLE )
    , descSet( VK_NULL_HANDLE )
{
    instanceBuffer = std::make_unique< AutoBuffer >( std::move( _allocator ) );
    instanceBuffer->Create( DECAL_MAX_COUNT * sizeof( ShDecalInstance ),
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            "Decal instance buffer" );

    CreateDescriptors();
    CreateRenderPass();
    VkDescriptorSetLayout setLayouts[] = {
        _uniform.GetDescSetLayout(),
        storageFramebuffers->GetDescSetLayout(),
        _textureManager.GetDescSetLayout(),
        descSetLayout,
    };
    CreatePipelineLayout( setLayouts, std::size( setLayouts ) );
    CreatePipelines( &_shaderManager );
}

RTGL1::DecalManager::~DecalManager()
{
    vkDestroyDescriptorSetLayout( device, descSetLayout, nullptr );
    vkDestroyDescriptorPool( device, descPool, nullptr );

    vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
    DestroyPipelines();

    vkDestroyRenderPass( device, renderPass, nullptr );
    DestroyFramebuffers();
}

void RTGL1::DecalManager::PrepareForFrame( uint32_t frameIndex )
{
    decalCount = 0;
}

void RTGL1::DecalManager::Upload( uint32_t                                 frameIndex,
                                  const RgDecalUploadInfo&                 uploadInfo,
                                  const std::shared_ptr< TextureManager >& textureManager )
{
    if( decalCount >= DECAL_MAX_COUNT )
    {
        assert( 0 );
        return;
    }

    const uint32_t decalIndex = decalCount;
    decalCount++;

    const MaterialTextures mat = textureManager->GetMaterialTextures( uploadInfo.pTextureName );

    ShDecalInstance instance = {
        .textureAlbedoAlpha = mat.indices[ TEXTURE_ALBEDO_ALPHA_INDEX ],
        .textureOcclusionRoughnessMetallic =
            mat.indices[ TEXTURE_OCCLUSION_ROUGHNESS_METALLIC_INDEX ],
        .textureNormal   = mat.indices[ TEXTURE_NORMAL_INDEX ],
        .textureEmissive = mat.indices[ TEXTURE_EMISSIVE_INDEX ],
    };
    Matrix::ToMat4Transposed( instance.transform, uploadInfo.transform );

    {
        auto* dst = instanceBuffer->GetMappedAs< ShDecalInstance* >( frameIndex );
        memcpy( &dst[ decalIndex ], &instance, sizeof( ShDecalInstance ) );
    }
}

void RTGL1::DecalManager::SubmitForFrame( VkCommandBuffer cmd, uint32_t frameIndex )
{
    if( decalCount == 0 )
    {
        return;
    }

    CmdLabel label( cmd, "Copying decal data" );

    instanceBuffer->CopyFromStaging( cmd, frameIndex, decalCount * sizeof( ShDecalInstance ) );
}

void RTGL1::DecalManager::Draw( VkCommandBuffer                          cmd,
                                uint32_t                                 frameIndex,
                                const std::shared_ptr< GlobalUniform >&  uniform,
                                const std::shared_ptr< Framebuffers >&   framebuffers,
                                const std::shared_ptr< TextureManager >& textureManager )
{
    if( decalCount == 0 )
    {
        return;
    }

    CmdLabel label( cmd, "Decal draw" );

    {
        VkBufferMemoryBarrier2KHR b = {
            .sType         = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT_KHR,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
            .dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR,
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR,
            .buffer        = instanceBuffer->GetDeviceLocal(),
            .offset        = 0,
            .size          = decalCount * sizeof( ShDecalInstance ),
        };

        VkDependencyInfoKHR info = {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers    = &b,
        };

        svkCmdPipelineBarrier2KHR( cmd, &info );
    }

    {
        FramebufferImageIndex fs[] = {
            FB_IMAGE_INDEX_ALBEDO,
            FB_IMAGE_INDEX_SURFACE_POSITION,
            FB_IMAGE_INDEX_NORMAL,
            FB_IMAGE_INDEX_METALLIC_ROUGHNESS,
        };

        framebuffers->BarrierMultiple( cmd, frameIndex, fs );
    }

    assert( passFramebuffers[ frameIndex ] != VK_NULL_HANDLE );

    const VkViewport viewport = {
        .x        = 0,
        .y        = 0,
        .width    = uniform->GetData()->renderWidth,
        .height   = uniform->GetData()->renderHeight,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const VkRect2D renderArea = {
        .offset = { .x = 0, .y = 0 },
        .extent = { .width  = uint32_t( uniform->GetData()->renderWidth ),
                    .height = uint32_t( uniform->GetData()->renderHeight ) },
    };

    VkRenderPassBeginInfo beginInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass      = renderPass,
        .framebuffer     = passFramebuffers[ frameIndex ],
        .renderArea      = renderArea,
        .clearValueCount = 0,
    };

    vkCmdBeginRenderPass( cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE );
    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );

    VkDescriptorSet sets[] = {
        uniform->GetDescSet( frameIndex ),
        framebuffers->GetDescSet( frameIndex ),
        textureManager->GetDescSet( frameIndex ),
        descSet,
    };

    vkCmdBindDescriptorSets( cmd,
                             VK_PIPELINE_BIND_POINT_GRAPHICS,
                             pipelineLayout,
                             0,
                             std::size( sets ),
                             sets,
                             0,
                             nullptr );

    vkCmdSetScissor( cmd, 0, 1, &renderArea );
    vkCmdSetViewport( cmd, 0, 1, &viewport );

    vkCmdDraw( cmd, CUBE_VERTEX_COUNT, decalCount, 0, 0 );

    vkCmdEndRenderPass( cmd );
}

void RTGL1::DecalManager::OnShaderReload( const ShaderManager* shaderManager )
{
    DestroyPipelines();
    CreatePipelines( shaderManager );
}

void RTGL1::DecalManager::OnFramebuffersSizeChange( const ResolutionState& resolutionState )
{
    DestroyFramebuffers();
    CreateFramebuffers( resolutionState.renderWidth, resolutionState.renderHeight );
}

void RTGL1::DecalManager::CreateRenderPass()
{
    VkAttachmentDescription colorAttch = {
        .format         = ShFramebuffers_Formats[ FB_IMAGE_INDEX_ALBEDO ],
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout    = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkAttachmentReference colorRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount    = 0,
        .pInputAttachments       = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colorRef,
        .pResolveAttachments     = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments    = nullptr,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask =
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT, // imageStore
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo info = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .flags           = 0,
        .attachmentCount = 1,
        .pAttachments    = &colorAttch,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };

    VkResult r = vkCreateRenderPass( device, &info, nullptr, &renderPass );
    VK_CHECKERROR( r );
}

void RTGL1::DecalManager::CreateFramebuffers( uint32_t width, uint32_t height )
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        assert( passFramebuffers[ i ] == VK_NULL_HANDLE );

        VkImageView v = storageFramebuffers->GetImageView( FB_IMAGE_INDEX_ALBEDO, i );

        VkFramebufferCreateInfo info = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = renderPass,
            .attachmentCount = 1,
            .pAttachments    = &v,
            .width           = width,
            .height          = height,
            .layers          = 1,
        };

        VkResult r = vkCreateFramebuffer( device, &info, nullptr, &passFramebuffers[ i ] );
        VK_CHECKERROR( r );
    }
}

void RTGL1::DecalManager::DestroyFramebuffers()
{
    for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
    {
        if( passFramebuffers[ i ] != VK_NULL_HANDLE )
        {
            vkDestroyFramebuffer( device, passFramebuffers[ i ], nullptr );
            passFramebuffers[ i ] = VK_NULL_HANDLE;
        }
    }
}

void RTGL1::DecalManager::CreatePipelineLayout( const VkDescriptorSetLayout* pSetLayouts,
                                                uint32_t                     setLayoutCount )
{
    VkPipelineLayoutCreateInfo info = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = setLayoutCount,
        .pSetLayouts            = pSetLayouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = nullptr,
    };

    VkResult r = vkCreatePipelineLayout( device, &info, nullptr, &pipelineLayout );
    VK_CHECKERROR( r );
}

void RTGL1::DecalManager::CreatePipelines( const ShaderManager* shaderManager )
{
    assert( pipeline == VK_NULL_HANDLE );
    assert( renderPass != VK_NULL_HANDLE );
    assert( pipelineLayout != VK_NULL_HANDLE );

    VkPipelineShaderStageCreateInfo stages[] = {
        shaderManager->GetStageInfo( "VertDecal" ),
        shaderManager->GetStageInfo( "FragDecal" ),
    };

    VkPipelineVertexInputStateCreateInfo vertexInput = {
        .sType                         = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = CUBE_TOPOLOGY,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = nullptr, // dynamic state
        .scissorCount  = 1,
        .pScissors     = nullptr, // dynamic state
    };

    VkPipelineRasterizationStateCreateInfo raster = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0,
        .depthBiasClamp          = 0,
        .depthBiasSlopeFactor    = 0,
        .lineWidth               = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable  = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = VK_FALSE, // must be true, if depthWrite is true
        .depthWriteEnable      = VK_FALSE,
        .depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttch = {
        .blendEnable         = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | 0,
    };

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttch,
    };

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = std::size( dynamicStates ),
        .pDynamicStates    = dynamicStates,
    };

    VkGraphicsPipelineCreateInfo info = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = std::size( stages ),
        .pStages             = stages,
        .pVertexInputState   = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState  = nullptr,
        .pViewportState      = &viewportState,
        .pRasterizationState = &raster,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depthStencil,
        .pColorBlendState    = &colorBlendState,
        .pDynamicState       = &dynamicInfo,
        .layout              = pipelineLayout,
        .renderPass          = renderPass,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
    };

    VkResult r = vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline );
    VK_CHECKERROR( r );
}

void RTGL1::DecalManager::DestroyPipelines()
{
    assert( pipeline != VK_NULL_HANDLE );

    vkDestroyPipeline( device, pipeline, nullptr );
    pipeline = VK_NULL_HANDLE;
}

void RTGL1::DecalManager::CreateDescriptors()
{
    {
        VkDescriptorPoolSize poolSize = {
            .type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
        };

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets       = 1,
            .poolSizeCount = 1,
            .pPoolSizes    = &poolSize,
        };

        VkResult r = vkCreateDescriptorPool( device, &poolInfo, nullptr, &descPool );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Decal desc pool" );
    }
    {
        VkDescriptorSetLayoutBinding binding = {
            .binding         = BINDING_DECAL_INSTANCES,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        VkDescriptorSetLayoutCreateInfo info = {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings    = &binding,
        };

        VkResult r = vkCreateDescriptorSetLayout( device, &info, nullptr, &descSetLayout );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME(
            device, descSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Decal desc set layout" );
    }

    {
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = descPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &descSetLayout,
        };

        VkResult r = vkAllocateDescriptorSets( device, &allocInfo, &descSet );
        VK_CHECKERROR( r );

        SET_DEBUG_NAME( device, descSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Decal desc set" );
    }
    {
        VkDescriptorBufferInfo b = {
            .buffer = instanceBuffer->GetDeviceLocal(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet w = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = descSet,
            .dstBinding      = BINDING_DECAL_INSTANCES,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo     = &b,
        };

        vkUpdateDescriptorSets( device, 1, &w, 0, nullptr );
    }
}
