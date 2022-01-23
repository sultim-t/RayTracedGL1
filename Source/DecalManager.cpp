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

constexpr uint32_t CUBE_VERTEX_COUNT = 14;
constexpr VkPrimitiveTopology CUBE_TOPOLOGY = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;


RTGL1::DecalManager::DecalManager(
    VkDevice _device,
    std::shared_ptr<MemoryAllocator> &_allocator,
    const std::shared_ptr<ShaderManager> &_shaderManager,
    const std::shared_ptr<GlobalUniform> &_uniform,
    std::shared_ptr<Framebuffers> _storageFramebuffers,
    const std::shared_ptr<TextureManager> &_textureManager)
:
    device(_device),
    storageFramebuffers(std::move(_storageFramebuffers)),
    decalCount(0),
    renderPass(VK_NULL_HANDLE),
    passFramebuffers{},
    pipelineLayout(VK_NULL_HANDLE),
    pipeline(VK_NULL_HANDLE),
    descPool(VK_NULL_HANDLE),
    descSetLayout(VK_NULL_HANDLE),
    descSet(VK_NULL_HANDLE)
{
    instanceBuffer = std::make_unique<AutoBuffer>(_allocator);
    instanceBuffer->Create(
        DECAL_MAX_COUNT * sizeof(ShDecalInstance),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Decal instance buffer");

    CreateDescriptors();
    CreateRenderPass();
    VkDescriptorSetLayout setLayouts[] =
    {
        _uniform->GetDescSetLayout(),
        storageFramebuffers->GetDescSetLayout(),
        _textureManager->GetDescSetLayout(),
        descSetLayout
    };
    CreatePipelineLayout(setLayouts, std::size(setLayouts));
    CreatePipelines(_shaderManager.get());
}

RTGL1::DecalManager::~DecalManager()
{
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    DestroyPipelines();

    vkDestroyRenderPass(device, renderPass, nullptr);
    DestroyFramebuffers();
}

void RTGL1::DecalManager::PrepareForFrame(uint32_t frameIndex)
{
    decalCount = 0;
}

void RTGL1::DecalManager::Upload(uint32_t frameIndex, const RgDecalUploadInfo &uploadInfo,
                                 const std::shared_ptr<TextureManager> &textureManager)
{
    if (decalCount >= DECAL_MAX_COUNT)
    {
        assert(0);
        return;
    }

    const uint32_t decalIndex = decalCount;
    decalCount++;

    const MaterialTextures mat = textureManager->GetMaterialTextures(uploadInfo.material);

    ShDecalInstance instance = {};
    instance.textureAlbedoAlpha      = mat.indices[MATERIAL_ALBEDO_ALPHA_INDEX];
    instance.textureRougnessMetallic = mat.indices[MATERIAL_ROUGHNESS_METALLIC_EMISSION_INDEX];
    instance.textureNormals          = mat.indices[MATERIAL_NORMAL_INDEX];
    Matrix::ToMat4Transposed(instance.transform, uploadInfo.transform);

    {
        ShDecalInstance *dst = (ShDecalInstance *)instanceBuffer->GetMapped(frameIndex);
        memcpy(&dst[decalIndex], &instance, sizeof(ShDecalInstance));
    }
}

void RTGL1::DecalManager::SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (decalCount == 0)
    {
        return;
    }

    CmdLabel label(cmd, "Copying decal data");

    instanceBuffer->CopyFromStaging(cmd, frameIndex, decalCount * sizeof(ShDecalInstance));
}

void RTGL1::DecalManager::Draw(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, const std::shared_ptr<Framebuffers> &framebuffers, const std::shared_ptr<TextureManager> &textureManager)
{
    if (decalCount == 0)
    {
        return;
    }

    CmdLabel label(cmd, "Decal draw");

    {
        VkBufferMemoryBarrier2KHR b = {};
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
        b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
        b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
        b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
        b.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR;
        b.buffer = instanceBuffer->GetDeviceLocal();
        b.offset = 0;
        b.size = decalCount * sizeof(ShDecalInstance);

        VkDependencyInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        info.bufferMemoryBarrierCount = 1;
        info.pBufferMemoryBarriers = &b;

        svkCmdPipelineBarrier2KHR(cmd, &info);
    }

    {
        FramebufferImageIndex fs[] =
        {
            FB_IMAGE_INDEX_ALBEDO,
            FB_IMAGE_INDEX_SURFACE_POSITION,
            FB_IMAGE_INDEX_NORMAL_GEOMETRY,
            FB_IMAGE_INDEX_NORMAL,
            FB_IMAGE_INDEX_METALLIC_ROUGHNESS,
        };

        framebuffers->BarrierMultiple(cmd, frameIndex, fs);
    }

    assert(passFramebuffers[frameIndex] != VK_NULL_HANDLE);

    const VkViewport viewport = { 0, 0, uniform->GetData()->renderWidth, uniform->GetData()->renderHeight, 0.0f, 1.0f };
    const VkRect2D renderArea = { { 0, 0 }, { (uint32_t)uniform->GetData()->renderWidth, (uint32_t)uniform->GetData()->renderHeight} };

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = passFramebuffers[frameIndex];
    beginInfo.renderArea = renderArea;
    beginInfo.clearValueCount = 0;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDescriptorSet sets[] =
    {
        uniform->GetDescSet(frameIndex),
        framebuffers->GetDescSet(frameIndex),
        textureManager->GetDescSet(frameIndex),
        descSet
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
    DestroyFramebuffers();
    CreateFramebuffers(width, height);
}

void RTGL1::DecalManager::CreateRenderPass()
{
    VkAttachmentDescription colorAttch = {};
    colorAttch.format = ShFramebuffers_Formats[FB_IMAGE_INDEX_ALBEDO];
    colorAttch.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttch.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttch.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttch.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttch.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    colorAttch.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_GENERAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // imageStore
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.flags = 0;
    info.attachmentCount = 1;
    info.pAttachments = &colorAttch;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    VkResult r = vkCreateRenderPass(device, &info, nullptr, &renderPass);
    VK_CHECKERROR(r);
}

void RTGL1::DecalManager::CreateFramebuffers(uint32_t width, uint32_t height)
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        assert(passFramebuffers[i] == VK_NULL_HANDLE);

        VkImageView v = storageFramebuffers->GetImageView(FB_IMAGE_INDEX_ALBEDO, i);

        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPass;
        info.attachmentCount = 1;
        info.pAttachments = &v;
        info.width = width;
        info.height = height;
        info.layers = 1;

        VkResult r = vkCreateFramebuffer(device, &info, nullptr, &passFramebuffers[i]);
        VK_CHECKERROR(r);
    }
}

void RTGL1::DecalManager::DestroyFramebuffers()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (passFramebuffers[i] != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, passFramebuffers[i], nullptr);
            passFramebuffers[i] = VK_NULL_HANDLE;
        }
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
    raster.rasterizerDiscardEnable = VK_FALSE;
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

    VkPipelineColorBlendAttachmentState colorBlendAttch = {};
    colorBlendAttch.blendEnable = VK_TRUE;
    colorBlendAttch.colorBlendOp = colorBlendAttch.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttch.srcColorBlendFactor = colorBlendAttch.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttch.dstColorBlendFactor = colorBlendAttch.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttch.colorWriteMask = 
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        0;

    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttch;

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

void RTGL1::DecalManager::CreateDescriptors()
{
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        VkResult r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Decal desc pool");
    }
    {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = BINDING_DECAL_INSTANCES;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;

        VkResult r = vkCreateDescriptorSetLayout(device, &info, nullptr, &descSetLayout);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Decal desc set layout");
    }

    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descSetLayout;

        VkResult r = vkAllocateDescriptorSets(device, &allocInfo, &descSet);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Decal desc set");
    }
    {
        VkDescriptorBufferInfo b = {};
        b.buffer = instanceBuffer->GetDeviceLocal();
        b.offset = 0;
        b.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet w = {};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descSet;
        w.dstBinding = BINDING_DECAL_INSTANCES;
        w.dstArrayElement = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &b;

        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }
}
