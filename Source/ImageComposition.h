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
#include "ShaderManager.h"
#include "Framebuffers.h"
#include "GlobalUniform.h"
#include "Tonemapping.h"
#include "Volumetric.h"

namespace RTGL1
{

class ImageComposition : public IShaderDependency
{
public:
    ImageComposition( VkDevice                           device,
                      std::shared_ptr< MemoryAllocator > allocator,
                      std::shared_ptr< Framebuffers >    framebuffers,
                      const ShaderManager&               shaderManager,
                      const GlobalUniform&               uniform,
                      const Tonemapping&                 tonemapping );
    ~ImageComposition() override;

    ImageComposition( const ImageComposition& other )                = delete;
    ImageComposition( ImageComposition&& other ) noexcept            = delete;
    ImageComposition& operator=( const ImageComposition& other )     = delete;
    ImageComposition& operator=( ImageComposition&& other ) noexcept = delete;

    void PrepareForRaster( VkCommandBuffer cmd, uint32_t frameIndex, const GlobalUniform* uniform );

    void Finalize( VkCommandBuffer                     cmd,
                   uint32_t                            frameIndex,
                   const GlobalUniform&                uniform,
                   const Tonemapping&                  tonemapping,
                   const RgDrawFrameTonemappingParams& params );

    void OnShaderReload( const ShaderManager* shaderManager ) override;

private:
    void ProcessCheckerboard( VkCommandBuffer      cmd,
                              uint32_t             frameIndex,
                              const GlobalUniform* uniform );
    void ApplyTonemapping( VkCommandBuffer      cmd,
                           uint32_t             frameIndex,
                           const GlobalUniform& uniform,
                           const Tonemapping&   tonemapping );

    static VkPipelineLayout CreatePipelineLayout( VkDevice               device,
                                                  VkDescriptorSetLayout* pSetLayouts,
                                                  uint32_t               setLayoutCount,
                                                  const char*            pDebugName );

    void CreateDescriptors();

    void CreatePipelines( const ShaderManager* shaderManager );
    void DestroyPipelines();

    void SetupLpmParams( VkCommandBuffer                     cmd,
                         uint32_t                            frameIndex,
                         const RgDrawFrameTonemappingParams& params );

private:
    VkDevice device;

    std::shared_ptr< Framebuffers > framebuffers;

    std::unique_ptr< AutoBuffer > lpmParams;
    struct
    {
        RgFloat3D saturation{ FLT_MAX, FLT_MAX, FLT_MAX };
        RgFloat3D crosstalk{ FLT_MAX, FLT_MAX, FLT_MAX };
    } lpmPrev;

    VkPipelineLayout composePipelineLayout;
    VkPipelineLayout checkerboardPipelineLayout;

    VkPipeline composePipeline;
    VkPipeline checkerboardPipeline;

    VkDescriptorSetLayout descLayout;
    VkDescriptorPool      descPool;
    VkDescriptorSet       descSet;
};

}