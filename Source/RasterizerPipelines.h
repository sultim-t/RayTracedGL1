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

#pragma once

#include "Common.h"
#include "Containers.h"
#include "ShaderManager.h"
#include "RasterizedDataCollector.h"

namespace RTGL1
{

class RasterizerPipelines
{
public:
    explicit RasterizerPipelines( VkDevice             device,
                                  VkPipelineLayout     pipelineLayout,
                                  VkRenderPass         renderPass,
                                  const ShaderManager* shaderManager,
                                  std::string_view     shaderNameVert,
                                  std::string_view     shaderNameFrag,
                                  uint32_t             additionalAttachmentsCount,
                                  bool                 applyVertexColorGamma,
                                  const VkViewport*    pViewport = nullptr,
                                  const VkRect2D*      pScissors = nullptr );
    ~RasterizerPipelines();

    RasterizerPipelines( const RasterizerPipelines& other )     = delete;
    RasterizerPipelines( RasterizerPipelines&& other ) noexcept = delete;
    RasterizerPipelines& operator=( const RasterizerPipelines& other ) = delete;
    RasterizerPipelines& operator=( RasterizerPipelines&& other ) noexcept = delete;

    void                 OnShaderReload( const ShaderManager* shaderManager );

    VkPipelineLayout     GetPipelineLayout();

    VkPipeline           BindPipelineIfNew( VkCommandBuffer    cmd,
                                            VkPipeline         oldPipeline,
                                            PipelineStateFlags pipelineState );


private:
    [[nodiscard]] VkPipeline CreatePipeline( PipelineStateFlags pipelineState ) const;
    VkPipeline               GetPipeline( PipelineStateFlags pipelineState );
    void                     DestroyAllPipelines();

private:
    VkDevice                                             device;

    std::string                                          shaderNameVert;
    std::string                                          shaderNameFrag;

    VkPipelineLayout                                     pipelineLayout;
    VkRenderPass                                         renderPass;
    VkPipelineShaderStageCreateInfo                      vertShaderStage;
    VkPipelineShaderStageCreateInfo                      fragShaderStage;

    rgl::unordered_map< PipelineStateFlags, VkPipeline > pipelines;
    VkPipelineCache                                      pipelineCache;

    std::optional< VkViewport >                          nonDynamicViewport;
    std::optional< VkRect2D >                            nonDynamicScissors;

    uint32_t                                             applyVertexColorGamma;
    uint32_t                                             additionalAttachmentsCount;
};

}
