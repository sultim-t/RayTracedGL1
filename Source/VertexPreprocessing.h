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

#pragma once

#include "Common.h"
#include "ASManager.h"
#include "GlobalUniform.h"
#include "ShaderManager.h"

namespace RTGL1
{

struct ShVertPreprocessing;

class VertexPreprocessing : public IShaderDependency
{
public:
    VertexPreprocessing( VkDevice             device,
                         const GlobalUniform& uniform,
                         const ASManager&     asManager,
                         const ShaderManager& shaderManager );

    ~VertexPreprocessing() override;

    VertexPreprocessing( const VertexPreprocessing& other )     = delete;
    VertexPreprocessing( VertexPreprocessing&& other ) noexcept = delete;
    VertexPreprocessing& operator=( const VertexPreprocessing& other ) = delete;
    VertexPreprocessing& operator=( VertexPreprocessing&& other ) noexcept = delete;

    void                 Preprocess( VkCommandBuffer            cmd,
                                     uint32_t                   frameIndex,
                                     uint32_t                   preprocMode,
                                     const GlobalUniform&       uniform,
                                     ASManager&                 asManager,
                                     const ShVertPreprocessing& push );

    void                 OnShaderReload( const ShaderManager* shaderManager ) override;

private:
    void CreatePipelineLayout( const VkDescriptorSetLayout* pSetLayouts, uint32_t setLayoutCount );
    void CreatePipelines( const ShaderManager* shaderManager );
    void DestroyPipelines();

private:
    VkDevice         device;
    VkPipelineLayout pipelineLayout;
    VkPipeline       pipelineOnlyDynamic;
    VkPipeline       pipelineDynamicAndMovable;
    VkPipeline       pipelineAll;
};

}
