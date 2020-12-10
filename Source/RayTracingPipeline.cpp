#include "RayTracingPipeline.h"
#include "Generated/ShaderCommonC.h"

RayTracingPipeline::RayTracingPipeline(VkDevice device, const PhysicalDevice &physDevice, const ShaderManager& sm)
{
    this->device = device;

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.push_back(sm.GetStageInfo("RGen"));
    stages.push_back(sm.GetStageInfo("RMiss"));
    stages.push_back(sm.GetStageInfo("RMissShadow"));
    stages.push_back(sm.GetStageInfo("RClsHit"));

    AddGeneralGroup(0);
    AddGeneralGroup(1);
    AddGeneralGroup(2);
    AddHitGroup(3);

    CreateDescriptors();
    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        rtDescSetLayout,
    };

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayouts.size();
    plLayoutInfo.pSetLayouts = setLayouts.data();

    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &rtPipelineLayout);
    VK_CHECKERROR(r);

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = stages.size();
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = shaderGroups.size();
    pipelineInfo.pGroups = shaderGroups.data();
    pipelineInfo.maxRecursionDepth = 2;
    pipelineInfo.layout = rtPipelineLayout;
    pipelineInfo.libraries.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;

    r = vksCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rtPipeline);
    VK_CHECKERROR(r);

    CreateSBT(physDevice);
}

RayTracingPipeline::~RayTracingPipeline()
{
    vkDestroyPipeline(device, rtPipeline, nullptr);
    vkDestroyPipelineLayout(device, rtPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, rtDescSetLayout, nullptr);
    vkDestroyDescriptorPool(device, rtDescPool, nullptr);
}

void RayTracingPipeline::CreateDescriptors()
{
    VkResult r;

    VkDescriptorSetLayoutBinding asBinding = {};
    asBinding.binding = BINDING_ACCELERATION_STRUCTURE;
    asBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    asBinding.descriptorCount = 1;
    asBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &asBinding;

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &rtDescSetLayout);
    VK_CHECKERROR(r);

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &rtDescPool);
    VK_CHECKERROR(r);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = rtDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &rtDescSetLayout;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkAllocateDescriptorSets(device, &allocInfo, &rtDescSets[i]);
        VK_CHECKERROR(r);
    }
}

void RayTracingPipeline::CreateSBT(const PhysicalDevice &physDevice)
{
    VkResult r;

    uint32_t groupCount = shaderGroups.size();
    sbtAlignment = physDevice.GetRayTracingProperties().shaderGroupBaseAlignment;
    sbtHandleSize = physDevice.GetRayTracingProperties().shaderGroupHandleSize;
    sbtSize = sbtAlignment * groupCount;

    shaderBindingTable = std::make_shared<Buffer>();
    shaderBindingTable->Init(device, physDevice, sbtSize,
                             VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);

    std::vector<uint8_t> shaderHandles(sbtSize);
    r = vksGetRayTracingShaderGroupHandlesKHR(device, rtPipeline, 0, groupCount, shaderHandles.size(), shaderHandles.data());
    VK_CHECKERROR(r);

    uint8_t *mapped = (uint8_t *) shaderBindingTable->Map();

    for (uint32_t i = 0; i < groupCount; i++)
    {
        memcpy(mapped, shaderHandles.data() + i * sbtHandleSize, sbtHandleSize);
        mapped += sbtAlignment;
    }

    shaderBindingTable->Unmap();
}

void RayTracingPipeline::GetEntries(VkStridedBufferRegionKHR &raygenEntry, VkStridedBufferRegionKHR &missEntry,
                                    VkStridedBufferRegionKHR &hitEntry, VkStridedBufferRegionKHR &callableEntry)
{
    // TODO: remove indices
    raygenEntry = {};
    raygenEntry.buffer = shaderBindingTable->GetBuffer();
    raygenEntry.offset = sbtAlignment * 0;
    raygenEntry.stride = sbtAlignment;
    raygenEntry.size = sbtSize;

    missEntry = {};
    missEntry.buffer = shaderBindingTable->GetBuffer();
    missEntry.offset = sbtAlignment * 1;
    missEntry.stride = sbtAlignment;
    missEntry.size = sbtSize;

    hitEntry = {};
    hitEntry.buffer = shaderBindingTable->GetBuffer();
    hitEntry.offset = sbtAlignment * 2;
    hitEntry.stride = sbtAlignment;
    hitEntry.size = sbtSize;

    callableEntry = {};
}

void RayTracingPipeline::AddGeneralGroup(uint32_t generalIndex)
{
    VkRayTracingShaderGroupCreateInfoKHR group = {};
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    group.generalShader = generalIndex;
    group.closestHitShader = VK_SHADER_UNUSED_KHR;
    group.anyHitShader = VK_SHADER_UNUSED_KHR;
    group.intersectionShader = VK_SHADER_UNUSED_KHR;

    shaderGroups.push_back(group);
}

void RayTracingPipeline::AddHitGroup(uint32_t closestHitIndex)
{
    AddHitGroup(closestHitIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
}

void RayTracingPipeline::AddHitGroup(uint32_t closestHitIndex, uint32_t anyHitIndex)
{
    AddHitGroup(closestHitIndex, anyHitIndex, VK_SHADER_UNUSED_KHR);
}

void RayTracingPipeline::AddHitGroup(uint32_t closestHitIndex, uint32_t anyHitIndex, uint32_t intersectionIndex)
{
    VkRayTracingShaderGroupCreateInfoKHR group = {};
    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    group.closestHitShader = closestHitIndex;
    group.anyHitShader = anyHitIndex;
    group.intersectionShader = intersectionIndex;

    shaderGroups.push_back(group);
}