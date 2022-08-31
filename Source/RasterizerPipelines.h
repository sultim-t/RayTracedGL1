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
#include "RTGL1/RTGL1.h"

namespace RTGL1
{

class RasterizerPipelines
{
public:
    explicit RasterizerPipelines(
        VkDevice device,
        VkPipelineLayout pipelineLayout, 
        VkRenderPass renderPass,
        uint32_t additionalAttachmentsCount,
        bool applyVertexColorGamma);

    ~RasterizerPipelines();

    RasterizerPipelines(const RasterizerPipelines &other) = delete;
    RasterizerPipelines(RasterizerPipelines &&other) noexcept = delete;
    RasterizerPipelines &operator=(const RasterizerPipelines &other) = delete;
    RasterizerPipelines &operator=(RasterizerPipelines &&other) noexcept = delete;

    void Clear();
    void SetShaders(const ShaderManager *shaderManager, const char *vertexShaderName, const char *fragmentShaderName);
    void DisableDynamicState(const VkViewport &viewport, const VkRect2D &scissors);

    VkPipeline GetPipeline(RgRasterizedGeometryStateFlags pipelineState, RgBlendFactor blendFuncSrc, RgBlendFactor blendFuncDst);
    VkPipelineLayout GetPipelineLayout();

    void BindPipelineIfNew(VkCommandBuffer cmd, VkPipeline &curPipeline,
                           RgRasterizedGeometryStateFlags pipelineState, RgBlendFactor blendFuncSrc, RgBlendFactor blendFuncDst);


private:
    VkPipeline CreatePipeline(RgRasterizedGeometryStateFlags pipelineState, RgBlendFactor blendFuncSrc, RgBlendFactor blendFuncDst) const;

private:
    VkDevice device;

    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;
    VkPipelineShaderStageCreateInfo vertShaderStage;
    VkPipelineShaderStageCreateInfo fragShaderStage;

    rgl::unordered_map<uint32_t, VkPipeline> pipelines;
    VkPipelineCache pipelineCache;

    struct
    {
        VkViewport viewport = {};
        VkRect2D scissors = {};

        // if true, viewport/scissors must be set dynamically
        bool isEnabled = true;
    } dynamicState;

    uint32_t applyVertexColorGamma;
    uint32_t additionalAttachmentsCount;
};

}
