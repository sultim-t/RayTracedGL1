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

#include "Rasterizer.h"
#include "Swapchain.h"
#include "Generated/ShaderCommonC.h"

Rasterizer::Rasterizer(
    VkDevice device, 
    const std::shared_ptr<PhysicalDevice> &physDevice, 
    const std::shared_ptr<ShaderManager> &shaderManager,
    VkDescriptorSetLayout texturesDescLayout,
    VkFormat surfaceFormat,
    uint32_t maxVertexCount, uint32_t maxIndexCount)
{
    this->device = device;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        collectors[i] = std::make_shared<RasterizedDataCollector>(
            device, physDevice, maxVertexCount, maxIndexCount);
    }

    CreateRenderPass(surfaceFormat);
    CreatePipelineLayout(texturesDescLayout);
    CreatePipelineCache();
    CreatePipeline(shaderManager);
}

Rasterizer::~Rasterizer()
{
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    DestroyFramebuffers();
}

void Rasterizer::Upload(const RgRasterizedGeometryUploadInfo &uploadInfo, uint32_t frameIndex)
{
    collectors[frameIndex]->AddGeometry(uploadInfo);
}

void Rasterizer::Draw(VkCommandBuffer cmd, uint32_t frameIndex, VkDescriptorSet texturesDescSet)
{
    VkDeviceSize offset = 0;
    VkBuffer vrtBuffer = collectors[frameIndex]->GetVertexBuffer();
    VkBuffer indBuffer = collectors[frameIndex]->GetIndexBuffer();

    const auto &drawInfos = collectors[frameIndex]->GetDrawInfos();

    VkClearValue clearValue = {};

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = framebuffers[frameIndex];
    beginInfo.renderArea = curRenderArea;
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetScissor(cmd, 0, 1, &curRenderArea);
    vkCmdSetViewport(cmd, 0, 1, &curViewport);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // TODO: global texture array desc set
#if 0
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
        1, &texturesDescSet,
        0, nullptr);
#endif
    vkCmdBindVertexBuffers(cmd, 0, 1, &vrtBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, indBuffer, offset, VK_INDEX_TYPE_UINT32);

    for (const auto &info : drawInfos)
    {
        if (info.indexCount > 0)
        {
            vkCmdDrawIndexed(cmd, info.indexCount, 1, info.firstIndex, info.firstVertex, 0);
        }
        else
        {
            vkCmdDraw(cmd, info.vertexCount, 1, info.firstVertex, 0);
        }
    }

    vkCmdEndRenderPass(cmd);
   
    collectors[frameIndex]->Clear();
}

void Rasterizer::OnSwapchainCreate(const Swapchain *pSwapchain)
{
    CreateFramebuffers(
        pSwapchain->GetWidth(), pSwapchain->GetHeight(),
        pSwapchain->GetImageViews(), pSwapchain->GetImageCount());
}

void Rasterizer::OnSwapchainDestroy()
{
    DestroyFramebuffers();
}

void Rasterizer::CreatePipelineCache()
{
    VkPipelineCacheCreateInfo cacheInfo = {};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    VkResult r = vkCreatePipelineCache(device, &cacheInfo, nullptr, &pipelineCache);
    VK_CHECKERROR(r);
}

void Rasterizer::CreatePipelineLayout(VkDescriptorSetLayout texturesDescLayout)
{
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    // TODO: global texture array desc set
#if 0
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &texturesDescLayout;
#endif
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Rasterizer draw pipeline layout");
}

void Rasterizer::CreatePipeline(const std::shared_ptr<ShaderManager> &shaderManager)
{
    VkDynamicState dynamicStates[2] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkVertexInputBindingDescription vertBinding = {};
    vertBinding.binding = 0;
    vertBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertBinding.stride = RasterizedDataCollector::GetVertexStride();

    std::array<VkVertexInputAttributeDescription, 4> attrs = {};
    RasterizedDataCollector::GetVertexLayout(attrs);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertBinding;
    vertexInputInfo.vertexAttributeDescriptionCount = attrs.size();
    vertexInputInfo.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr; // will be set dynamically
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr; // will be set dynamically

    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.lineWidth = 1.0f;
    raster.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttch = {};
    colorBlendAttch.blendEnable = true;
    colorBlendAttch.colorBlendOp = colorBlendAttch.alphaBlendOp
        = VK_BLEND_OP_ADD;
    colorBlendAttch.srcColorBlendFactor = colorBlendAttch.srcAlphaBlendFactor
        = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttch.dstColorBlendFactor = colorBlendAttch.dstAlphaBlendFactor
        = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttch.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttch;

    VkPipelineDynamicStateCreateInfo dynamicInfo = {};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
    dynamicInfo.pDynamicStates = dynamicStates;

    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0] = shaderManager->GetStageInfo("RasterizerVert");
    shaderStages[1] = shaderManager->GetStageInfo("RasterizerFrag");

    VkGraphicsPipelineCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    plInfo.stageCount = 2;
    plInfo.pStages = shaderStages;
    plInfo.pVertexInputState = &vertexInputInfo;
    plInfo.pInputAssemblyState = &inputAssembly;
    plInfo.pViewportState = &viewportState;
    plInfo.pRasterizationState = &raster;
    plInfo.pMultisampleState = &multisampling;
    plInfo.pDepthStencilState = &depthStencil;
    plInfo.pColorBlendState = &colorBlendState;
    plInfo.pDynamicState = &dynamicInfo;
    plInfo.layout = pipelineLayout;
    plInfo.renderPass = renderPass;
    plInfo.subpass = 0;
    plInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkResult r = vkCreateGraphicsPipelines(device, pipelineCache, 1, &plInfo, nullptr, &pipeline);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Rasterizer draw pipeline");
}

void Rasterizer::CreateFramebuffers(uint32_t width, uint32_t height, const VkImageView *pFrameAttchs, uint32_t count)
{
    assert(framebuffers.empty());
    framebuffers.resize(count);

    for (uint32_t i = 0; i < count; i++)
    {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &pFrameAttchs[i];
        fbInfo.width = width;
        fbInfo.height = height;
        fbInfo.layers = 1;

        VkResult r = vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, framebuffers[i], VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, "Rasterizer framebuffer");
    }

    curViewport.x = 0;
    curViewport.y = 0;
    curViewport.minDepth = 0;
    curViewport.maxDepth = 1;
    curViewport.width = width;
    curViewport.height = height;

    curRenderArea.offset = { 0, 0 };
    curRenderArea.extent = { width, height };
}

void Rasterizer::DestroyFramebuffers()
{
    for (VkFramebuffer fb : framebuffers)
    {
        vkDestroyFramebuffer(device, fb, nullptr);
    }

    framebuffers.clear();
}

void Rasterizer::CreateRenderPass(VkFormat surfaceFormat)
{
    VkAttachmentDescription colorAttch = {};
    colorAttch.format = surfaceFormat;
    colorAttch.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttch.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttch.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttch.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo passInfo = {};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    passInfo.attachmentCount = 1;
    passInfo.pAttachments = &colorAttch;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependencyCount = 1;
    passInfo.pDependencies = &dependency;

    VkResult r = vkCreateRenderPass(device, &passInfo, nullptr, &renderPass);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, renderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Rasterizer render pass");
}