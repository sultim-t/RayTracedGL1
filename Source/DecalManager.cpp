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

#include "Generated/ShaderCommonC.h"


constexpr uint32_t DECAL_MAX_COUNT = 4096;

constexpr uint32_t CUBE_VERTEX_COUNT = 14;
constexpr VkPrimitiveTopology CUBE_TOPOLOGY = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;


RTGL1::DecalManager::DecalManager(
    VkDevice _device,
    std::shared_ptr<MemoryAllocator> &_allocator,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    const std::shared_ptr<GlobalUniform> &_uniform,
    const std::shared_ptr<Framebuffers> &_framebuffers,
    const std::shared_ptr<TextureManager> &_textureManager)
:
    device(_device),
    decalCount(0),
    renderPass(VK_NULL_HANDLE),
    framebuffer(VK_NULL_HANDLE),
    pipelineLayout(VK_NULL_HANDLE),
    pipeline(VK_NULL_HANDLE)
{
    CreateRenderPass();

    VkDescriptorSetLayout setLayouts[] =
    {
        _uniform->GetDescSetLayout(),
        _framebuffers->GetDescSetLayout(),
        _textureManager->GetDescSetLayout()
    };
    CreatePipelineLayout(setLayouts, std::size(setLayouts));

    CreatePipelines(_shaderManager.get());
}

RTGL1::DecalManager::~DecalManager()
{
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();

    vkDestroyRenderPass(device, renderPass, nullptr);
    DestroyFramebuffer();
}

void RTGL1::DecalManager::PrepareForFrame(uint32_t frameIndex)
{
    decalCount = 0;
}

void RTGL1::DecalManager::Upload(uint32_t frameIndex, const RgDecalUploadInfo &uploadInfo)
{
    if (decalCount >= DECAL_MAX_COUNT)
    {
        assert(0);
        return;
    }

    decalCount++;
}

void RTGL1::DecalManager::SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{}

void RTGL1::DecalManager::Draw(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, const std::shared_ptr<Framebuffers> &framebuffers, const std::shared_ptr<TextureManager> &textureManager)
{
    if (decalCount == 0)
    {
        return;
    }

    assert(framebuffer != VK_NULL_HANDLE);

    const VkViewport viewport = { 0, 0, uniform->GetData()->renderWidth, uniform->GetData()->renderHeight, 0.0f, 1.0f };
    const VkRect2D renderArea = { { 0, 0 }, { (uint32_t)uniform->GetData()->renderWidth, (uint32_t)uniform->GetData()->renderHeight} };

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = framebuffer;
    beginInfo.renderArea = renderArea;
    beginInfo.clearValueCount = 0;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDescriptorSet sets[] =
    {
        uniform->GetDescSet(frameIndex),
        framebuffers->GetDescSet(frameIndex),
        textureManager->GetDescSet(frameIndex),
    };

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
        0, std::size(sets), sets,
        0, nullptr);

    vkCmdSetScissor(cmd, 0, 1, &renderArea);
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    vkCmdDraw(cmd, CUBE_VERTEX_COUNT, decalCount, 0, 0);

    vkCmdEndRenderPass(cmd);
}

void RTGL1::DecalManager::OnShaderReload(const ShaderManager *shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void RTGL1::DecalManager::OnFramebuffersSizeChange(uint32_t width, uint32_t height)
{
    DestroyFramebuffer();
    CreateFramebuffer(width, height);
}

void RTGL1::DecalManager::CreateRenderPass()
{
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 0;
    subpass.pColorAttachments = nullptr;
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.flags = 0;
    info.attachmentCount = 0;
    info.pAttachments = nullptr;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 0;
    info.pDependencies = nullptr;

    VkResult r = vkCreateRenderPass(device, &info, nullptr, &renderPass);
    VK_CHECKERROR(r);
}

void RTGL1::DecalManager::CreateFramebuffer(uint32_t width, uint32_t height)
{
    assert(framebuffer == VK_NULL_HANDLE);

    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = renderPass;
    info.attachmentCount = 0;
    info.pAttachments = nullptr;
    info.width = width;
    info.height = height;
    info.layers = 1;

    VkResult r = vkCreateFramebuffer(device, &info, nullptr, &framebuffer);
    VK_CHECKERROR(r);
}

void RTGL1::DecalManager::DestroyFramebuffer()
{
    if (framebuffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }
}

void RTGL1::DecalManager::CreatePipelineLayout(const VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount)
{
    VkPipelineLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = setLayoutCount;
    info.pSetLayouts = pSetLayouts;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;

    VkResult r = vkCreatePipelineLayout(device, &info, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);
}

void RTGL1::DecalManager::CreatePipelines(const ShaderManager *shaderManager)
{
    assert(pipeline == VK_NULL_HANDLE);
    assert(renderPass != VK_NULL_HANDLE);
    assert(pipelineLayout != VK_NULL_HANDLE);

    VkPipelineShaderStageCreateInfo stages[] =
    {
        shaderManager->GetStageInfo("VertDecal"),
        shaderManager->GetStageInfo("FragDecal"),
    };

    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 0;
    vertexInput.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = CUBE_TOPOLOGY;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr; // dynamic state
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr; // dynamic state

    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_TRUE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.depthBiasEnable = VK_FALSE;
    raster.depthBiasConstantFactor = 0;
    raster.depthBiasClamp = 0;
    raster.depthBiasSlopeFactor = 0;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthTestEnable = VK_FALSE; // must be true, if depthWrite is true
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.depthBoundsTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.attachmentCount = 0;

    VkDynamicState dynamicStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo = {};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = std::size(dynamicStates);
    dynamicInfo.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = std::size(stages);
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pTessellationState = nullptr;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisampling;
    info.pDepthStencilState = &depthStencil;
    info.pColorBlendState = &colorBlendState;
    info.pDynamicState = &dynamicInfo;
    info.layout = pipelineLayout;
    info.renderPass = renderPass;
    info.subpass = 0;
    info.basePipelineHandle = VK_NULL_HANDLE;

    VkResult r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    VK_CHECKERROR(r);
}

void RTGL1::DecalManager::DestroyPipelines()
{
    assert(pipeline != VK_NULL_HANDLE);

    vkDestroyPipeline(device, pipeline, nullptr);
    pipeline = VK_NULL_HANDLE;
}