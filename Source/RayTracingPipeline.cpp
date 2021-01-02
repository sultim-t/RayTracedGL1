#include "RayTracingPipeline.h"

#include "Utils.h"

RayTracingPipeline::RayTracingPipeline(
    VkDevice device,
    const std::shared_ptr<PhysicalDevice> &physDevice,
    const std::shared_ptr<ShaderManager> &sm,
    const std::shared_ptr<ASManager> &asManager,
    const std::shared_ptr<GlobalUniform> &uniform,
    VkDescriptorSetLayout imagesSetLayout)
{
    this->device = device;

    std::vector<VkPipelineShaderStageCreateInfo> stages =
    {
        sm->GetStageInfo("RGen"),
        sm->GetStageInfo("RMiss"),
        sm->GetStageInfo("RMissShadow"),
        sm->GetStageInfo("RClsHit"),
    };

    AddGeneralGroup(0);
    AddGeneralGroup(1);
    AddGeneralGroup(2);
    AddHitGroup(3);

    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        // ray tracing acceleration structures
        asManager->GetTLASDescSetLayout(),
        // images
        imagesSetLayout,
        // uniform
        uniform->GetDescSetLayout(),
        // vertex data
        asManager->GetBuffersDescSetLayout()
    };

    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayouts.size();
    plLayoutInfo.pSetLayouts = setLayouts.data();

    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &rtPipelineLayout);
    VK_CHECKERROR(r);

    VkPipelineLibraryCreateInfoKHR libInfo = {};
    libInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = stages.size();
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = shaderGroups.size();
    pipelineInfo.pGroups = shaderGroups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 2;
    pipelineInfo.layout = rtPipelineLayout;
    pipelineInfo.pLibraryInfo = &libInfo;

    r = svkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rtPipeline);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, rtPipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Ray tracing pipeline Layout");
    SET_DEBUG_NAME(device, rtPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Ray tracing pipeline");

    CreateSBT(physDevice);
}

RayTracingPipeline::~RayTracingPipeline()
{
    vkDestroyPipeline(device, rtPipeline, nullptr);
    vkDestroyPipelineLayout(device, rtPipelineLayout, nullptr);
}

void RayTracingPipeline::CreateSBT(const std::shared_ptr<PhysicalDevice> &physDevice)
{
    VkResult r;

    uint32_t groupCount = shaderGroups.size();
    groupBaseAlignment = physDevice->GetRTPipelineProperties().shaderGroupBaseAlignment;

    handleSize = physDevice->GetRTPipelineProperties().shaderGroupHandleSize;
    alignedHandleSize = Utils::Align(handleSize, groupBaseAlignment);

    uint32_t sbtSize = alignedHandleSize * groupCount;

    shaderBindingTable = std::make_shared<Buffer>();
    shaderBindingTable->Init(
        device, *physDevice, sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "Shader binding table buffer");

    std::vector<uint8_t> shaderHandles(sbtSize);
    r = svkGetRayTracingShaderGroupHandlesKHR(device, rtPipeline, 0, groupCount, shaderHandles.size(), shaderHandles.data());
    VK_CHECKERROR(r);

    uint8_t *mapped = (uint8_t *) shaderBindingTable->Map();

    for (uint32_t i = 0; i < groupCount; i++)
    {
        memcpy(mapped, shaderHandles.data() + i * alignedHandleSize, handleSize);
        mapped += alignedHandleSize;
    }

    shaderBindingTable->Unmap();
}

void RayTracingPipeline::Bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);
}

void RayTracingPipeline::GetEntries(
    VkStridedDeviceAddressRegionKHR &raygenEntry,
    VkStridedDeviceAddressRegionKHR &missEntry,
    VkStridedDeviceAddressRegionKHR &hitEntry,
    VkStridedDeviceAddressRegionKHR &callableEntry) const
{
    VkDeviceAddress bufferAddress = shaderBindingTable->GetAddress();

    // TODO: remove indices
    raygenEntry = {};
    raygenEntry.deviceAddress = bufferAddress;
    raygenEntry.stride = alignedHandleSize;
    raygenEntry.size = alignedHandleSize;
    // vk spec
    assert(raygenEntry.size == raygenEntry.stride);

    missEntry = {};
    missEntry.deviceAddress = bufferAddress + alignedHandleSize;
    missEntry.stride = alignedHandleSize;
    missEntry.size = alignedHandleSize * 2;

    hitEntry = {};
    hitEntry.deviceAddress = bufferAddress + alignedHandleSize * 2;
    hitEntry.stride = alignedHandleSize;
    hitEntry.size = alignedHandleSize;

    callableEntry = {};
}

VkPipelineLayout RayTracingPipeline::GetLayout() const
{
    return rtPipelineLayout;
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