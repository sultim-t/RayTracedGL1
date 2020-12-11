#include "VertexBufferManager.h"

#include <array>
#include "Generated/ShaderCommonC.h"

VertexBufferManager::VertexBufferManager(VkDevice device, std::shared_ptr<PhysicalDevice> physDevice, 
                                         std::shared_ptr<CommandBufferManager> cmdManager,
                                         const RgInstanceCreateInfo &info)
{
    this->device = device;
    this->physDevice = physDevice;
    this->cmdManager = cmdManager;

    properties.vertexArrayOfStructs = info.vertexArrayOfStructs == RG_TRUE;
    properties.positionStride = info.vertexPositionStride;
    properties.normalStride = info.vertexNormalStride;
    properties.texCoordStride = info.vertexTexCoordStride;
    properties.colorStride = info.vertexColorStride;

    scratchBuffer = std::make_shared<ScratchBuffer>(device, physDevice);
    asBuilder = std::make_shared<ASBuilder>(scratchBuffer);

    // static vertices
    staticVertsStaging = std::make_shared<Buffer>();
    staticVertsBuffer = std::make_shared<Buffer>();

    staticVertsStaging->Init(device, *physDevice,
                            sizeof(ShVertexBufferStatic),
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staticVertsBuffer->Init(device, *physDevice,
                           sizeof(ShVertexBufferStatic),
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // static and movable static share the same buffer as their data won't be changing
    collectorStaticMovable = std::make_shared<VertexCollectorFiltered>(staticVertsStaging, staticVertsBuffer, properties, RG_GEOMETRY_TYPE_STATIC_MOVABLE);

    // dynamic vertices
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        dynamicVertsStaging[i] = std::make_shared<Buffer>();
        dynamicVertsBuffer[i] = std::make_shared<Buffer>();

        dynamicVertsStaging[i]->Init(device, *physDevice,
                                    sizeof(ShVertexBufferDynamic),
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        dynamicVertsBuffer[i]->Init(device, *physDevice,
                                   sizeof(ShVertexBufferDynamic),
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        collectorDynamic[i] = std::make_shared<VertexCollector>(dynamicVertsStaging[i], dynamicVertsBuffer[i], properties);

    }

    CreateDescriptors();

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    VkResult r = vkCreateFence(device, &fenceInfo, nullptr, &staticCopyFence);
    VK_CHECKERROR(r);
}

void VertexBufferManager::CreateDescriptors()
{
    VkResult r;
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    // static
    bindings[0].binding = BINDING_VERTEX_BUFFER_STATIC;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

    // dynamic
    bindings[1].binding = BINDING_VERTEX_BUFFER_DYNAMIC;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 2;
    bindings[1].stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout);
    VK_CHECKERROR(r);

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

    VkDescriptorSetAllocateInfo descSetInfo = {};
    descSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetInfo.descriptorPool = descPool;
    descSetInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    descSetInfo.pSetLayouts = &descSetLayout;

    r = vkAllocateDescriptorSets(device, &descSetInfo, descSets);
    VK_CHECKERROR(r);

    // bind buffers to descriptors
    std::array<VkDescriptorBufferInfo, 2 * MAX_FRAMES_IN_FLIGHT> bufferInfos{};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        bufferInfos[i * 2].buffer = staticVertsBuffer->GetBuffer();
        bufferInfos[i * 2].offset = 0;
        bufferInfos[i * 2].range = VK_WHOLE_SIZE;

        bufferInfos[i * 2 + 1].buffer = dynamicVertsBuffer[i]->GetBuffer();
        bufferInfos[i * 2 + 1].offset = 0;
        bufferInfos[i * 2 + 1].range = VK_WHOLE_SIZE;
    }


    std::array<VkWriteDescriptorSet, bufferInfos.size()> writes{};

    for (uint32_t i = 0; i < bufferInfos.size(); i++)
    {
        VkWriteDescriptorSet &w = writes[i];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descSets[i];
        w.dstBinding = BINDING_VERTEX_BUFFER_STATIC;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

VertexBufferManager::~VertexBufferManager()
{
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyFence(device, staticCopyFence, nullptr);
}

uint32_t VertexBufferManager::AddGeometry(const RgGeometryCreateInfo& info)
{
    if (info.geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        return collectorDynamic[currentFrameIndex]->AddGeometry(info);
    }
    else
    {
        return collectorStaticMovable->AddGeometry(info);
    }
}

// separate functions to make adding between Begin..Geometry and Submit..Geometry a bit clearer

void VertexBufferManager::AddStaticGeometry(const RgGeometryCreateInfo &info)
{
    assert(info.geomType == RG_GEOMETRY_TYPE_STATIC || info.geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE);
    AddGeometry(info);
}

void VertexBufferManager::AddDynamicGeometry(const RgGeometryCreateInfo &info)
{
    assert(info.geomType == RG_GEOMETRY_TYPE_DYNAMIC);
    AddGeometry(info);
}

void VertexBufferManager::BeginStaticGeometry()
{
    // the whole static vertex data must be recreated, clear previous data
    collectorStaticMovable->Reset();
    collectorStaticMovable->BeginCollecting();
}

void VertexBufferManager::SubmitStaticGeometry()
{
    collectorStaticMovable->EndCollecting();

    VkResult r;

    DestroyAS(staticBlas);
    DestroyAS(staticMovableBlas);

    const auto &blasGT = collectorStaticMovable->GetASGeometryTypes();
    VkAccelerationStructureCreateInfoKHR blasInfo = {};
    blasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    blasInfo.maxGeometryCount = blasGT.size();
    blasInfo.pGeometryInfos = blasGT.data();
    r = vksCreateAccelerationStructureKHR(device, &blasInfo, nullptr, &staticBlas.as);
    VK_CHECKERROR(r);

    const auto &movableBlasGT = collectorStaticMovable->GetASGeometryTypesFiltered();
    VkAccelerationStructureCreateInfoKHR movableBlasInfo = {};
    movableBlasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    movableBlasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    movableBlasInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    movableBlasInfo.maxGeometryCount = movableBlasGT.size();
    movableBlasInfo.pGeometryInfos = movableBlasGT.data();
    r = vksCreateAccelerationStructureKHR(device, &movableBlasInfo, nullptr, &staticMovableBlas.as);
    VK_CHECKERROR(r);

    AllocBindASMemory(staticBlas);
    AllocBindASMemory(staticMovableBlas);

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    // copy from staging with barrier
    collectorStaticMovable->CopyFromStaging(cmd);

    const auto &geoms = collectorStaticMovable->GetASGeometries();
    const auto &movableGeoms = collectorStaticMovable->GetASGeometriesFiltered();

    const auto &offsets = collectorStaticMovable->GetASBuildOffsetInfos();
    const auto &movableOffsets = collectorStaticMovable->GetASBuildOffsetInfosFiltered();

    const VkAccelerationStructureGeometryKHR *pGeoms = geoms.data();
    const VkAccelerationStructureGeometryKHR *pMovableGeoms = movableGeoms.data();

    asBuilder->AddBLAS(staticBlas.as, geoms.size(), &pGeoms, offsets.data(), true, false);
    asBuilder->AddBLAS(staticMovableBlas.as, movableGeoms.size(), &pMovableGeoms, movableOffsets.data(), false, false);

    asBuilder->BuildBottomLevel(cmd);

    cmdManager->Submit(cmd, staticCopyFence);
    cmdManager->WaitForFence(staticCopyFence);
}

void VertexBufferManager::BeginDynamicGeometry(uint32_t frameIndex)
{
    currentFrameIndex = frameIndex;

    // dynamic AS must be recreated
    collectorDynamic[currentFrameIndex]->Reset();
    collectorDynamic[currentFrameIndex]->BeginCollecting();
}

void VertexBufferManager::SubmitDynamicGeometry(VkCommandBuffer cmd)
{
    const auto &colDyn = collectorDynamic[currentFrameIndex];
    auto &dynBlas = dynamicBlas[currentFrameIndex];

    colDyn->EndCollecting();

    VkResult r;

    DestroyAS(dynBlas);

    const auto &blasGT = colDyn->GetASGeometryTypes();
    VkAccelerationStructureCreateInfoKHR blasInfo = {};
    blasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    blasInfo.maxGeometryCount = blasGT.size();
    blasInfo.pGeometryInfos = blasGT.data();
    r = vksCreateAccelerationStructureKHR(device, &blasInfo, nullptr, &dynamicBlas[currentFrameIndex].as);
    VK_CHECKERROR(r);

    AllocBindASMemory(dynBlas);

    colDyn->CopyFromStaging(cmd);

    const auto &geoms = colDyn->GetASGeometries();
    const auto &offsets = colDyn->GetASBuildOffsetInfos();

    const VkAccelerationStructureGeometryKHR *pGeoms = geoms.data();
    asBuilder->AddBLAS(dynBlas.as, geoms.size(), &pGeoms, offsets.data(), false, false);
    asBuilder->BuildBottomLevel(cmd);
}

void VertexBufferManager::UpdateStaticMovableTransform(uint32_t geomIndex, const RgTransform &transform)
{
    collectorStaticMovable->UpdateTransform(geomIndex, transform);
}

void VertexBufferManager::ResubmitStaticMovable()
{

}

void VertexBufferManager::AllocBindASMemory(AccelerationStructure &as)
{
    VkAccelerationStructureMemoryRequirementsInfoKHR memReqInfo = {};
    memReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_KHR;
    memReqInfo.accelerationStructure = as.as;
    memReqInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_KHR;
    memReqInfo.buildType = VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR;

    VkMemoryRequirements2 memReq2 = {};
    memReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vksGetAccelerationStructureMemoryRequirementsKHR(device, &memReqInfo, &memReq2);

    as.memory = physDevice->AllocDeviceMemory(memReq2, true);

    VkBindAccelerationStructureMemoryInfoKHR bindInfo = {};
    bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_KHR;
    bindInfo.accelerationStructure = as.as;
    bindInfo.memory = as.memory;

    VkResult r = vksBindAccelerationStructureMemoryKHR(device, 1, &bindInfo);
    VK_CHECKERROR(r);
}

void VertexBufferManager::DestroyAS(AccelerationStructure &as)
{
    physDevice->FreeDeviceMemory(as.memory);
    vkDestroyAccelerationStructureKHR(device, as.as, nullptr);
}
