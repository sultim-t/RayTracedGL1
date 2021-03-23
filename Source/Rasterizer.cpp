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

#include <array>

#include "Swapchain.h"
#include "Matrix.h"
#include "Utils.h"
#include "Generated/ShaderCommonC.h"

using namespace RTGL1;

Rasterizer::Rasterizer(
    VkDevice _device,
    const std::shared_ptr<MemoryAllocator> &_allocator,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    std::shared_ptr<TextureManager> _textureMgr,
    VkFormat _surfaceFormat,
    uint32_t _maxVertexCount, uint32_t _maxIndexCount)
:
    device(_device),
    textureMgr(_textureMgr),
    renderPass(VK_NULL_HANDLE),
    pipelineLayout(VK_NULL_HANDLE),
    pipelineCache(VK_NULL_HANDLE),
    pipeline(VK_NULL_HANDLE),
    fbRenderArea{},
    fbViewport{}
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        collectors[i] = std::make_shared<RasterizedDataCollector>(
            device, _allocator, _textureMgr, _maxVertexCount, _maxIndexCount);
    }

    CreateRenderPass(_surfaceFormat);
    CreatePipelineLayout(_textureMgr->GetDescSetLayout());
    CreatePipelineCache();
    CreatePipelines(_shaderManager.get());
}

Rasterizer::~Rasterizer()
{
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();
    DestroyFramebuffers();
}

void Rasterizer::Upload(uint32_t frameIndex, 
                        const RgRasterizedGeometryUploadInfo &uploadInfo, 
                        const float *viewProjection, const RgViewport *viewport)
{
    collectors[frameIndex]->AddGeometry(uploadInfo, viewProjection, viewport);
}

void Rasterizer::Draw(VkCommandBuffer cmd, uint32_t frameIndex, float *view, float *proj)
{
    VkDescriptorSet texturesDescSet;

    if (auto tm = textureMgr.lock())
    {
        texturesDescSet = tm->GetDescSet(frameIndex);
    }
    else
    {
        return;
    }

    VkDeviceSize offset = 0;
    VkBuffer vrtBuffer = collectors[frameIndex]->GetVertexBuffer();
    VkBuffer indBuffer = collectors[frameIndex]->GetIndexBuffer();

    const auto &drawInfos = collectors[frameIndex]->GetDrawInfos();

    VkClearValue clearValue = {};

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = framebuffers[frameIndex];
    beginInfo.renderArea = fbRenderArea;
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
        1, &texturesDescSet,
        0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vrtBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, indBuffer, offset, VK_INDEX_TYPE_UINT32);

    vkCmdSetScissor(cmd, 0, 1, &fbRenderArea);
    vkCmdSetViewport(cmd, 0, 1, &fbViewport);
    VkViewport curViewport = fbViewport;

    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, view, proj);

    for (const auto &info : drawInfos)
    {
        TrySetViewport(cmd, info, curViewport);

        struct
        {
            float vp[16];
            uint32_t t;
        } push;
        static_assert(sizeof(push) == 16 * sizeof(float) + sizeof(uint32_t), "");

        if (!info.isDefaultViewProjMatrix)
        {
            memcpy(push.vp, info.viewProj, sizeof(float) * 16);
        }
        else
        {
            memcpy(push.vp, defaultViewProj, sizeof(float) * 16);
        }

        push.t = info.textureIndex;

        vkCmdPushConstants(
            cmd, pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(push),
            &push);

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

void Rasterizer::TrySetViewport(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info, VkViewport &curViewport)
{
    const VkViewport &newViewport = info.isDefaultViewport ? fbViewport : info.viewport;

    if (!Utils::AreViewportsSame(curViewport, newViewport))
    {
        vkCmdSetViewport(cmd, 0, 1, &newViewport);
        curViewport = newViewport;
    }
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

void Rasterizer::OnShaderReload(const ShaderManager *shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
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
    VkPushConstantRange pushConst = {};
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConst.offset = 0;
    pushConst.size = 16 * sizeof(float) + sizeof(uint32_t);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &texturesDescLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConst;

    VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, pipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Rasterizer draw pipeline layout");
}

void Rasterizer::CreatePipelines(const ShaderManager *shaderManager)
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

    std::array<VkVertexInputAttributeDescription, 8> attrs = {};
    uint32_t attrsCount;

    RasterizedDataCollector::GetVertexLayout(attrs.data(), &attrsCount);
    assert(attrsCount <= attrs.size());

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertBinding;
    vertexInputInfo.vertexAttributeDescriptionCount = attrsCount;
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
    raster.cullMode = VK_CULL_MODE_NONE;
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
    shaderStages[0] = shaderManager->GetStageInfo("VertRasterizer");
    shaderStages[1] = shaderManager->GetStageInfo("FragRasterizer");

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

void Rasterizer::DestroyPipelines()
{
    vkDestroyPipeline(device, pipeline, nullptr);
    pipeline = VK_NULL_HANDLE;
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

    fbViewport.x = 0;
    fbViewport.y = 0;
    fbViewport.minDepth = 0;
    fbViewport.maxDepth = 1;
    fbViewport.width = width;
    fbViewport.height = height;

    fbRenderArea.offset = { 0, 0 };
    fbRenderArea.extent = { width, height };
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
