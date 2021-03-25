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
#include "Generated/ShaderCommonCFramebuf.h"

using namespace RTGL1;

Rasterizer::Rasterizer(
    VkDevice _device,
    const std::shared_ptr<MemoryAllocator> &_allocator,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    std::shared_ptr<TextureManager> _textureMgr,
    std::shared_ptr<Framebuffers> _storageFramebuffers,
    VkFormat _surfaceFormat,
    uint32_t _maxVertexCount, uint32_t _maxIndexCount)
:
    device(_device),
    textureMgr(_textureMgr),
    storageFramebuffers(std::move(_storageFramebuffers)),
    rasterRenderPass(VK_NULL_HANDLE),
    swapchainRenderPass(VK_NULL_HANDLE),
    pipelineLayout(VK_NULL_HANDLE),
    rasterPipeline(VK_NULL_HANDLE),
    swapchainPipeline(VK_NULL_HANDLE),
    rasterFramebuffers{}
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        collectors[i] = std::make_shared<RasterizedDataCollector>(
            device, _allocator, _textureMgr, _maxVertexCount, _maxIndexCount);
    }

    CreateRasterRenderPass(ShFramebuffers_Formats[FB_IMAGE_INDEX_FINAL], ShFramebuffers_Formats[FB_IMAGE_INDEX_DEPTH]);
    CreateSwapchainRenderPass(_surfaceFormat);

    std::vector<VkDescriptorSetLayout> setLayouts = 
    {
        _textureMgr->GetDescSetLayout(),
        storageFramebuffers->GetDescSetLayout()
    };

    CreatePipelineLayout(setLayouts.data(), setLayouts.size());
    CreatePipelineCache();
    CreatePipelines(_shaderManager.get());
}

Rasterizer::~Rasterizer()
{
    vkDestroyRenderPass(device, rasterRenderPass, nullptr);
    vkDestroyRenderPass(device, swapchainRenderPass, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();
    DestroyRenderFramebuffers();
    DestroySwapchainFramebuffers();
}

void Rasterizer::PrepareForFrame(uint32_t frameIndex)
{  
    collectors[frameIndex]->Clear();
}

void Rasterizer::Upload(uint32_t frameIndex, 
                        const RgRasterizedGeometryUploadInfo &uploadInfo, 
                        const float *viewProjection, const RgViewport *viewport)
{
    collectors[frameIndex]->AddGeometry(uploadInfo, viewProjection, viewport);
}

void Rasterizer::DrawToFinalImage(VkCommandBuffer cmd, uint32_t frameIndex, float *view, float *proj)
{
    VkFramebuffer framebuffer = rasterFramebuffers[frameIndex];

    if (framebuffer == VK_NULL_HANDLE)
    {
        return;
    }

    storageFramebuffers->Barrier(cmd, frameIndex, FB_IMAGE_INDEX_DEPTH);
    storageFramebuffers->Barrier(cmd, frameIndex, FB_IMAGE_INDEX_FINAL);
    // TODO: clear depth attachment if no rays were traced?

    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, view, proj);

    Draw(cmd, frameIndex,
         collectors[frameIndex]->GetRasterDrawInfos(),
         rasterRenderPass, rasterPipeline, 
         framebuffer, rasterFramebufferState, defaultViewProj);
}

void Rasterizer::DrawToSwapchain(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t swapchainIndex, float *view, float *proj)
{
    if (swapchainIndex >= swapchainFramebuffers.size())
    {
        return;
    }

    VkFramebuffer framebuffer = swapchainFramebuffers[swapchainIndex];

    if (framebuffer == VK_NULL_HANDLE)
    {
        return;
    }

    float defaultViewProj[16];
    Matrix::Multiply(defaultViewProj, view, proj);

    Draw(cmd, frameIndex,
         collectors[frameIndex]->GetSwapchainDrawInfos(),
         swapchainRenderPass, swapchainPipeline, 
         framebuffer, swapchainFramebufferState, defaultViewProj);
}

void Rasterizer::Draw(VkCommandBuffer cmd, uint32_t frameIndex,
                      const std::vector<RasterizedDataCollector::DrawInfo> &drawInfos,
                      VkRenderPass renderPass, VkPipeline pipeline,
                      VkFramebuffer framebuffer, const RasterAreaState &raState, float *defaultViewProj)
{
    assert(framebuffer != VK_NULL_HANDLE);

    VkDescriptorSet descSets[2] = {};
    const int descSetCount = sizeof(descSets) / sizeof(VkDescriptorSet);

    descSets[1] = storageFramebuffers->GetDescSet(frameIndex);

    if (auto tm = textureMgr.lock())
    {
        descSets[0] = tm->GetDescSet(frameIndex);
    }
    else
    {
        return;
    }


    const VkViewport &defaultViewport = raState.viewport;
    const VkRect2D &defaultRenderArea = raState.renderArea;

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = framebuffer;
    beginInfo.renderArea = defaultRenderArea;
    beginInfo.clearValueCount = 0;
    beginInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);


    VkDeviceSize offset = 0;
    VkBuffer vrtBuffer = collectors[frameIndex]->GetVertexBuffer();
    VkBuffer indBuffer = collectors[frameIndex]->GetIndexBuffer();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
        descSetCount, descSets,
        0, nullptr);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vrtBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, indBuffer, offset, VK_INDEX_TYPE_UINT32);


    vkCmdSetScissor(cmd, 0, 1, &defaultRenderArea);
    vkCmdSetViewport(cmd, 0, 1, &defaultViewport);
    VkViewport curViewport = defaultViewport;

    for (const auto &info : drawInfos)
    {
        SetViewportIfNew(cmd, info, defaultViewport, curViewport);
        
        float model[16];
        Matrix::ToMat4Transposed(model, info.transform);

        // TODO: less memory usage
        struct
        {
            float vp[16];
            uint32_t t;
        } push;
        static_assert(sizeof(push) == 16 * sizeof(float) + sizeof(uint32_t), "");

        if (!info.isDefaultViewProjMatrix)
        {
            Matrix::Multiply(push.vp, model, info.viewProj);
        }
        else
        {
            Matrix::Multiply(push.vp, model, defaultViewProj);
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
}

void Rasterizer::SetViewportIfNew(VkCommandBuffer cmd, const RasterizedDataCollector::DrawInfo &info, 
                                  const VkViewport &defaultViewport, VkViewport &curViewport) const
{
    const VkViewport &newViewport = info.isDefaultViewport ? defaultViewport : info.viewport;

    if (!Utils::AreViewportsSame(curViewport, newViewport))
    {
        vkCmdSetViewport(cmd, 0, 1, &newViewport);
        curViewport = newViewport;
    }
}

void Rasterizer::OnSwapchainCreate(const Swapchain *pSwapchain)
{
    CreateSwapchainFramebuffers(
        pSwapchain->GetWidth(), pSwapchain->GetHeight(),
        pSwapchain->GetImageViews(), pSwapchain->GetImageCount());
}

void Rasterizer::OnSwapchainDestroy()
{
    DestroySwapchainFramebuffers();
}

void Rasterizer::OnShaderReload(const ShaderManager *shaderManager)
{
    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void Rasterizer::OnFramebuffersSizeChange(uint32_t width, uint32_t height)
{
    DestroyRenderFramebuffers();
    CreateRenderFramebuffers(width, height);
}

void Rasterizer::CreatePipelineCache()
{
}

void Rasterizer::CreatePipelineLayout(VkDescriptorSetLayout *pSetLayouts, uint32_t count)
{
    VkPushConstantRange pushConst = {};
    pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConst.offset = 0;
    pushConst.size = 16 * sizeof(float) + sizeof(uint32_t);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = count;
    layoutInfo.pSetLayouts = pSetLayouts;
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

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};

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
    plInfo.subpass = 0;
    plInfo.basePipelineHandle = VK_NULL_HANDLE;

    {
        shaderStages[0] = shaderManager->GetStageInfo("VertRasterizer");
        shaderStages[1] = shaderManager->GetStageInfo("FragRasterizerDepth");
        plInfo.renderPass = rasterRenderPass;

        VkResult r = vkCreateGraphicsPipelines(device, nullptr, 1, &plInfo, nullptr, &rasterPipeline);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, rasterPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Rasterizer raster draw pipeline");
    }

    {
        shaderStages[0] = shaderManager->GetStageInfo("VertRasterizer");
        shaderStages[1] = shaderManager->GetStageInfo("FragRasterizer");
        plInfo.renderPass = swapchainRenderPass;

        VkResult r = vkCreateGraphicsPipelines(device, nullptr, 1, &plInfo, nullptr, &swapchainPipeline);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, swapchainPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Rasterizer swapchain draw pipeline");
    }
}

void Rasterizer::DestroyPipelines()
{
    vkDestroyPipeline(device, rasterPipeline, nullptr);
    rasterPipeline = VK_NULL_HANDLE;

    vkDestroyPipeline(device, swapchainPipeline, nullptr);
    swapchainPipeline = VK_NULL_HANDLE;
}

void Rasterizer::CreateRenderFramebuffers(uint32_t renderWidth, uint32_t renderHeight)
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(rasterFramebuffers[i] == VK_NULL_HANDLE);

        const int attchCount = 1;
        VkImageView attchs[attchCount] = {
            storageFramebuffers->GetImageView(FB_IMAGE_INDEX_FINAL, i),
            //storageFramebuffers->GetImageView(FB_IMAGE_INDEX_DEPTH, i)
        };

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = rasterRenderPass;
        fbInfo.attachmentCount = attchCount;
        fbInfo.pAttachments = attchs;
        fbInfo.width = renderWidth;
        fbInfo.height = renderHeight;
        fbInfo.layers = 1;

        VkResult r = vkCreateFramebuffer(device, &fbInfo, nullptr, &rasterFramebuffers[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, rasterFramebuffers[i], VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, "Rasterizer raster framebuffer");
    }

    auto &rnvp = rasterFramebufferState.viewport;
    auto &rnra = rasterFramebufferState.renderArea;

    rnvp.x = 0.0f;
    rnvp.y = 0.0f;
    rnvp.minDepth = 0.0f;
    rnvp.maxDepth = 1.0f;
    rnvp.width = (float)renderWidth;
    rnvp.height = (float)renderHeight;

    rnra.offset = { 0, 0 };
    rnra.extent = { renderWidth, renderHeight };
}

void Rasterizer::CreateSwapchainFramebuffers(uint32_t swapchainWidth, uint32_t swapchainHeight,
    const VkImageView *pSwapchainAttchs, uint32_t swapchainAttchCount)
{
    // prepare framebuffers for drawing right into swapchain images
    swapchainFramebuffers.resize(swapchainAttchCount, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < swapchainAttchCount; i++)
    {
        assert(swapchainFramebuffers[i] == VK_NULL_HANDLE);

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = swapchainRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &pSwapchainAttchs[i];
        fbInfo.width = swapchainWidth;
        fbInfo.height = swapchainHeight;
        fbInfo.layers = 1;

        VkResult r = vkCreateFramebuffer(device, &fbInfo, nullptr, &swapchainFramebuffers[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, swapchainFramebuffers[i], VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, "Rasterizer swapchain framebuffer");
    }

    auto &swvp = swapchainFramebufferState.viewport;
    auto &swra = swapchainFramebufferState.renderArea;

    swvp.x = 0.0f;
    swvp.y = 0.0f;
    swvp.minDepth = 0.0f;
    swvp.maxDepth = 1.0f;
    swvp.width = (float)swapchainWidth;
    swvp.height = (float)swapchainHeight;

    swra.offset = { 0, 0 };
    swra.extent = { swapchainWidth, swapchainHeight };
}

void Rasterizer::DestroyRenderFramebuffers()
{
    for (VkFramebuffer &fb : rasterFramebuffers)
    {
        if (fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
}

void Rasterizer::DestroySwapchainFramebuffers()
{
    for (VkFramebuffer &fb : swapchainFramebuffers)
    {
        if (fb != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, fb, nullptr);
            fb = VK_NULL_HANDLE;
        }
    }
}

void Rasterizer::CreateRasterRenderPass(VkFormat finalImageFormat, VkFormat depthImageFormat)
{
    const int attchCount = 1;
    VkAttachmentDescription attchs[attchCount] = {};

    auto &colorAttch = attchs[0];
    colorAttch.format = finalImageFormat;
    colorAttch.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttch.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttch.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttch.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    /*auto &depthAttch = attchs[1];
    depthAttch.format = depthImageFormat; 
    depthAttch.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttch.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttch.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    depthAttch.finalLayout = VK_IMAGE_LAYOUT_GENERAL;*/


    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    //subpass.pDepthStencilAttachment = &depthRef;


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
    passInfo.attachmentCount = attchCount;
    passInfo.pAttachments = attchs;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependencyCount = 1;
    passInfo.pDependencies = &dependency;

    VkResult r = vkCreateRenderPass(device, &passInfo, nullptr, &rasterRenderPass);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, rasterRenderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Rasterizer raster render pass");
}

void Rasterizer::CreateSwapchainRenderPass(VkFormat surfaceFormat)
{
    VkAttachmentDescription colorAttch = {};
    colorAttch.format = surfaceFormat;
    colorAttch.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttch.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttch.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttch.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = nullptr;


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

    VkResult r = vkCreateRenderPass(device, &passInfo, nullptr, &swapchainRenderPass);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, swapchainRenderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Rasterizer swapchain render pass");
}
