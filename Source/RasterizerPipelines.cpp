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

#include "RasterizerPipelines.h"

#include <array>

#include "RasterizedDataCollector.h"
#include "RgException.h"

RTGL1::RasterizerPipelines::RasterizerPipelines( VkDevice             _device,
                                                 VkPipelineLayout     _pipelineLayout,
                                                 VkRenderPass         _renderPass,
                                                 const ShaderManager& _shaderManager,
                                                 std::string_view     _shaderNameVert,
                                                 std::string_view     _shaderNameFrag,
                                                 uint32_t             _additionalAttachmentsCount,
                                                 bool                 _applyVertexColorGamma,
                                                 const VkViewport*    _pViewport,
                                                 const VkRect2D*      _pScissors )
    : device( _device )
    , shaderNameVert( _shaderNameVert )
    , shaderNameFrag( _shaderNameFrag )
    , pipelineLayout( _pipelineLayout )
    , renderPass( _renderPass )
    , vertShaderStage{}
    , fragShaderStage{}
    , pipelineCache( VK_NULL_HANDLE )
    , nonDynamicViewport( _pViewport ? std::optional( *_pViewport ) : std::nullopt )
    , nonDynamicScissors( _pScissors ? std::optional( *_pScissors ) : std::nullopt )
    , applyVertexColorGamma( _applyVertexColorGamma )
    , additionalAttachmentsCount( _additionalAttachmentsCount )
{
    VkPipelineCacheCreateInfo info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };

    VkResult                  r = vkCreatePipelineCache( device, &info, nullptr, &pipelineCache );
    VK_CHECKERROR( r );

    OnShaderReload( &_shaderManager );
}

RTGL1::RasterizerPipelines::~RasterizerPipelines()
{
    DestroyAllPipelines();
    vkDestroyPipelineCache( device, pipelineCache, nullptr );
}

void RTGL1::RasterizerPipelines::DestroyAllPipelines()
{
    for( auto& p : pipelines )
    {
        vkDestroyPipeline( device, p.second, nullptr );
    }

    pipelines.clear();
}

void RTGL1::RasterizerPipelines::OnShaderReload( const ShaderManager* shaderManager )
{
    DestroyAllPipelines();

    vertShaderStage = shaderManager->GetStageInfo( shaderNameVert.c_str() );
    fragShaderStage = shaderManager->GetStageInfo( shaderNameFrag.c_str() );
}

VkPipeline RTGL1::RasterizerPipelines::GetPipeline( PipelineStateFlags pipelineState )
{
    auto f = pipelines.find( pipelineState );

    if( f == pipelines.end() )
    {
        VkPipeline p = CreatePipeline( pipelineState );

        pipelines[ pipelineState ] = p;
        return p;
    }

    return f->second;
}

VkPipelineLayout RTGL1::RasterizerPipelines::GetPipelineLayout()
{
    return pipelineLayout;
}

VkPipeline RTGL1::RasterizerPipelines::CreatePipeline( PipelineStateFlags pipelineState ) const
{
    assert( vertShaderStage.sType != 0 && fragShaderStage.sType != 0 );


    uint32_t alphaTest   = pipelineState & PipelineStateFlagBits::ALPHA_TEST ? 1 : 0;
    bool     translucent = pipelineState & PipelineStateFlagBits::TRANSLUCENT;
    bool     additive    = pipelineState & PipelineStateFlagBits::ADDITIVE;
    bool     depthTest   = pipelineState & PipelineStateFlagBits::DEPTH_TEST;
    bool     depthWrite  = pipelineState & PipelineStateFlagBits::DEPTH_WRITE;
    bool     isLines     = pipelineState & PipelineStateFlagBits::DRAW_AS_LINES;


    VkSpecializationMapEntry vertMapEntry = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof( uint32_t ),
    };

    VkSpecializationInfo vertSpecInfo = {
        .mapEntryCount = 1,
        .pMapEntries   = &vertMapEntry,
        .dataSize      = sizeof( applyVertexColorGamma ),
        .pData         = &applyVertexColorGamma,
    };

    VkSpecializationMapEntry fragMapEntry = {
        .constantID = 0,
        .offset     = 0,
        .size       = sizeof( uint32_t ),
    };

    VkSpecializationInfo fragSpecInfo = {
        .mapEntryCount = 1,
        .pMapEntries   = &fragMapEntry,
        .dataSize      = sizeof( alphaTest ),
        .pData         = &alphaTest,
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStage,
        fragShaderStage,
    };
    shaderStages[ 0 ].pSpecializationInfo = &vertSpecInfo;
    shaderStages[ 1 ].pSpecializationInfo = &fragSpecInfo;


    VkVertexInputBindingDescription vertBinding = {
        .binding   = 0,
        .stride    = RasterizedDataCollector::GetVertexStride(),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    auto                                 attrs = RasterizedDataCollector::GetVertexLayout();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType                         = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions    = &vertBinding,
        .vertexAttributeDescriptionCount = attrs.size(),
        .pVertexAttributeDescriptions    = attrs.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = isLines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = nonDynamicViewport ? &nonDynamicViewport.value() : nullptr,
        .scissorCount  = 1,
        .pScissors     = nonDynamicScissors ? &nonDynamicScissors.value() : nullptr,
    };

    VkPipelineRasterizationStateCreateInfo raster = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .lineWidth               = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable  = VK_FALSE,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        // must be true, if depthWrite is true
        .depthTestEnable       = depthTest || depthWrite,
        .depthWriteEnable      = depthWrite,
        .depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
    };

    VkBlendFactor blendSrc = VK_BLEND_FACTOR_ONE;
    VkBlendFactor blendDst = VK_BLEND_FACTOR_ONE;
    if( translucent )
    {
        if( additive )
        {
            blendSrc = VK_BLEND_FACTOR_ONE;
            blendDst = VK_BLEND_FACTOR_ONE;
        }
        else
        {
            blendSrc = VK_BLEND_FACTOR_SRC_ALPHA;
            blendDst = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        }
    }
    VkPipelineColorBlendAttachmentState blendAttch = {
        .blendEnable         = additive || translucent,
        .srcColorBlendFactor = blendSrc,
        .dstColorBlendFactor = blendDst,
        .colorBlendOp        = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = blendSrc,
        .dstAlphaBlendFactor = blendDst,
        .alphaBlendOp        = VK_BLEND_OP_ADD,
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendAttachmentState colorBlendAttchs[] = { blendAttch, blendAttch };

    VkPipelineColorBlendStateCreateInfo colorBlendState = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .attachmentCount = 1 + additionalAttachmentsCount,
        .pAttachments    = colorBlendAttchs,
    };

    if( colorBlendState.attachmentCount > std::size( colorBlendAttchs ) )
    {
        assert( 0 && "Add more entries to colorBlendAttchs" );
        throw RgException( RG_RESULT_GRAPHICS_API_ERROR, "Internal attachment count error" );
    }

    VkDynamicState dynamics[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicInfo = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = nonDynamicViewport && nonDynamicScissors ? 0u : 2u,
        .pDynamicStates    = dynamics,
    };

    VkGraphicsPipelineCreateInfo plInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = std::size( shaderStages ),
        .pStages             = shaderStages,
        .pVertexInputState   = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
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

    VkPipeline pipeline;
    VkResult r = vkCreateGraphicsPipelines( device, pipelineCache, 1, &plInfo, nullptr, &pipeline );

    VK_CHECKERROR( r );
    SET_DEBUG_NAME( device, pipeline, VK_OBJECT_TYPE_PIPELINE, "Rasterizer raster draw pipeline" );

    return pipeline;
}

VkPipeline RTGL1::RasterizerPipelines::BindPipelineIfNew( VkCommandBuffer    cmd,
                                                          VkPipeline         oldPipeline,
                                                          PipelineStateFlags pipelineState )
{
    VkPipeline p = GetPipeline( pipelineState );

    if( p == oldPipeline )
    {
        return oldPipeline;
    }

    vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p );
    return p;
}
