#include "Rasterizer.h"
#include "Generated/ShaderCommonC.h"

struct RasterizerVertex
{
    float       position[3];
    uint32_t    color;
    uint32_t    materialIds[4];
    float       texCoord[2];
};

Rasterizer::Rasterizer(
    VkDevice device, 
    const std::shared_ptr<PhysicalDevice> &physDevice, 
    const std::shared_ptr<ShaderManager> &shaderManager,
    VkDescriptorSetLayout texturesDescLayout,
    uint32_t maxVertexCount, uint32_t maxIndexCount)
{
    /*this->device = device;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vertexBuffers[i].Init(
            device, *physDevice,
            maxVertexCount * sizeof(RasterizerVertex),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        indexBuffers[i].Init(
            device, *physDevice,
            maxIndexCount * sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }



    CreateRenderPass();
    CreatePipelineLayout(texturesDescLayout);
    CreatePipeline(shaderManager);

    vkCreateFramebuffer*/
}

Rasterizer::~Rasterizer()
{
    /*vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);*/
}

void Rasterizer::Upload(const RgRasterizedGeometryUploadInfo &uploadInfo)
{

}

void Rasterizer::Draw(VkCommandBuffer cmd, uint32_t frameIndex)
{
    /*vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, );

    vkCmdBindDescriptorSets();

    VkBuffer vertBuffer = vertexBuffers[frameIndex].GetBuffer();
    VkBuffer indBuffer = indexBuffers[frameIndex].GetBuffer();

    VkDeviceSize offset = 0;

    vkCmdBindVertexBuffers(cmd, 0, 1, &vertBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, indBuffer, offset, VK_INDEX_TYPE_UINT32);
   
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);*/
}
/*
void Rasterizer::CreatePipelineCache()
{
    VkPipelineCacheCreateInfo cacheInfo = {};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    VkResult r = vkCreatePipelineCache(device, &cacheInfo, nullptr, &pipelineCache);
    VK_CHECKERROR(r);
}

void Rasterizer::CreatePipelineLayout(VkDescriptorSetLayout texturesDescLayout)
{



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
    vertBinding.stride = sizeof(RasterizerVertex);

    VkVertexInputAttributeDescription attrs[4] = {};

    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = offsetof(RasterizerVertex, position);

    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32_UINT;
    attrs[1].offset = offsetof(RasterizerVertex, color);

    attrs[2].binding = 0;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32A32_UINT;
    attrs[2].offset = offsetof(RasterizerVertex, materialIds);

    attrs[3].binding = 0;
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = offsetof(RasterizerVertex, texCoord);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertBinding;
    vertexInputInfo.vertexAttributeDescriptionCount = sizeof(attrs) / sizeof(attrs[0]);
    vertexInputInfo.pVertexAttributeDescriptions = attrs;

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
}*/

/*
void Rasterizer::CreateDescriptors()
{
    VkResult r;

    VkDescriptorSetLayoutBinding binding = {};
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.binding = BINDING_RASTERIZER_STORAGE_BUFFER;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, "Rasterizer desc set layout");

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descPool, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT, "Rasterizer desc set layout");

    VkDescriptorSetAllocateInfo descSetInfo = {};
    descSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetInfo.descriptorPool = descPool;
    descSetInfo.descriptorSetCount = 1;
    descSetInfo.pSetLayouts = &descLayout;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkAllocateDescriptorSets(device, &descSetInfo, &descSets[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descSets[i], VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, "Rasterizer desc set");
    
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = ;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof();

        VkWriteDescriptorSet wrt = {};
        wrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wrt.dstSet = descSets[i];
        wrt.dstBinding = BINDING_RASTERIZER_STORAGE_BUFFER;
        wrt.dstArrayElement = 0;
        wrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wrt.descriptorCount = 1;
        wrt.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &wrt, 0, nullptr);
    }
}

*/