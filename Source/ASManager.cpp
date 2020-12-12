#include "ASManager.h"

#include <array>
#include "Generated/ShaderCommonC.h"

ASManager::ASManager(VkDevice device, std::shared_ptr<PhysicalDevice> physDevice, 
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

    staticVertsStaging->Init(
        device, *physDevice,
        sizeof(ShVertexBufferStatic),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staticVertsBuffer->Init(
        device, *physDevice,
        sizeof(ShVertexBufferStatic),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // static and movable static share the same buffer as their data won't be changing
    collectorStaticMovable = std::make_shared<VertexCollectorFiltered>(
        device, *physDevice,
        staticVertsStaging, staticVertsBuffer, 
        properties, 
        RG_GEOMETRY_TYPE_STATIC_MOVABLE);

    // dynamic vertices
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        dynamicVertsStaging[i] = std::make_shared<Buffer>();
        dynamicVertsBuffer[i] = std::make_shared<Buffer>();

        dynamicVertsStaging[i]->Init(
            device, *physDevice,
            sizeof(ShVertexBufferDynamic),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        dynamicVertsBuffer[i]->Init(
            device, *physDevice,
            sizeof(ShVertexBufferDynamic),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        collectorDynamic[i] = std::make_shared<VertexCollector>(
            device, *physDevice,
            dynamicVertsStaging[i], dynamicVertsBuffer[i], properties);

    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        instanceBuffers[i].Init(
            device, *physDevice,
            MAX_TOP_LEVEL_INSTANCE_COUNT * sizeof(VkTransformMatrixKHR),
            VK_BUFFER_USAGE_RAY_TRACING_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    CreateDescriptors();

    // buffers won't be changing, update once
    UpdateBufferDescriptors();

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    VkResult r = vkCreateFence(device, &fenceInfo, nullptr, &staticCopyFence);
    VK_CHECKERROR(r);
}

void ASManager::CreateDescriptors()
{
    VkResult r;

    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

        // static
        bindings[0].binding = BINDING_VERTEX_BUFFER_STATIC;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

        // dynamic
        bindings[1].binding = BINDING_VERTEX_BUFFER_DYNAMIC;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings = bindings.data();

        r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &buffersDescSetLayout);
        VK_CHECKERROR(r);
    }

    {
        std::array<VkDescriptorSetLayoutBinding, 1> bindings{};

        bindings[0].binding = BINDING_ACCELERATION_STRUCTURE;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings = bindings.data();

        r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &asDescSetLayout);
        VK_CHECKERROR(r);
    }

    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT * 2;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    VkDescriptorSetAllocateInfo descSetInfo = {};
    descSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetInfo.descriptorPool = descPool;
    descSetInfo.descriptorSetCount = 1;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        descSetInfo.pSetLayouts = &buffersDescSetLayout;
        r = vkAllocateDescriptorSets(device, &descSetInfo, &buffersDescSets[i]);
        VK_CHECKERROR(r);

        descSetInfo.pSetLayouts = &asDescSetLayout;
        r = vkAllocateDescriptorSets(device, &descSetInfo, &asDescSets[i]);
        VK_CHECKERROR(r);
    }
}

void ASManager::UpdateBufferDescriptors()
{
    std::array<VkDescriptorBufferInfo, 2 * MAX_FRAMES_IN_FLIGHT> bufferInfos{};
    std::array<VkWriteDescriptorSet, 2 * MAX_FRAMES_IN_FLIGHT> writes{};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        bufferInfos[i * 2].buffer = staticVertsBuffer->GetBuffer();
        bufferInfos[i * 2].offset = 0;
        bufferInfos[i * 2].range = VK_WHOLE_SIZE;

        bufferInfos[i * 2 + 1].buffer = dynamicVertsBuffer[i]->GetBuffer();
        bufferInfos[i * 2 + 1].offset = 0;
        bufferInfos[i * 2 + 1].range = VK_WHOLE_SIZE;
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkWriteDescriptorSet &staticWrt = writes[i * 2];
        staticWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        staticWrt.dstSet = buffersDescSets[i];
        staticWrt.dstBinding = BINDING_VERTEX_BUFFER_STATIC;
        staticWrt.dstArrayElement = 0;
        staticWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        staticWrt.descriptorCount = 1;
        staticWrt.pBufferInfo = &bufferInfos[i * 2];

        VkWriteDescriptorSet &dynamicWrt = writes[i * 2 + 1];
        dynamicWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        dynamicWrt.dstSet = buffersDescSets[i];
        dynamicWrt.dstBinding = BINDING_VERTEX_BUFFER_DYNAMIC;
        dynamicWrt.dstArrayElement = 0;
        dynamicWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        dynamicWrt.descriptorCount = 1;
        dynamicWrt.pBufferInfo = &bufferInfos[i * 2 + 1];
    }

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

void ASManager::UpdateASDescriptors(uint32_t frameIndex)
{
    std::array<VkWriteDescriptorSet, MAX_FRAMES_IN_FLIGHT> writes{};
    std::array<VkWriteDescriptorSetAccelerationStructureKHR, MAX_FRAMES_IN_FLIGHT> asWrites{};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkWriteDescriptorSetAccelerationStructureKHR &asWrt = asWrites[i];
        asWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asWrt.accelerationStructureCount = 1;
        asWrt.pAccelerationStructures = &tlas[frameIndex].as;

        VkWriteDescriptorSet &wrt = writes[i];
        wrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wrt.pNext = &asWrites[i];
        wrt.dstSet = buffersDescSets[i];
        wrt.dstBinding = BINDING_ACCELERATION_STRUCTURE;
        wrt.dstArrayElement = 0;
        wrt.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        wrt.descriptorCount = 1;
    }

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);

}

ASManager::~ASManager()
{
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, buffersDescSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, asDescSetLayout, nullptr);
    vkDestroyFence(device, staticCopyFence, nullptr);
}

// separate functions to make adding between Begin..Geometry and Submit..Geometry a bit clearer

uint32_t ASManager::AddStaticGeometry(const RgGeometryUploadInfo &info)
{
    if (info.geomType == RG_GEOMETRY_TYPE_STATIC || info.geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE)
    {
        return collectorStaticMovable->AddGeometry(info);
    }

    assert(0);
    return 0;
}

uint32_t ASManager::AddDynamicGeometry(const RgGeometryUploadInfo &info, uint32_t frameIndex)
{
    if (info.geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        return collectorDynamic[frameIndex]->AddGeometry(info);
    }

    assert(0);
    return 0;
}

void ASManager::BeginStaticGeometry()
{
    // the whole static vertex data must be recreated, clear previous data
    collectorStaticMovable->Reset();
    collectorStaticMovable->BeginCollecting();
}

void ASManager::SubmitStaticGeometry()
{
    collectorStaticMovable->EndCollecting();

    VkResult r;

    DestroyAS(staticBlas);
    DestroyAS(staticMovableBlas);

    const auto &blasGT = collectorStaticMovable->GetASGeometryTypes();
    VkAccelerationStructureCreateInfoKHR blasInfo = {};
    blasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasInfo.flags = 
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    blasInfo.maxGeometryCount = blasGT.size();
    blasInfo.pGeometryInfos = blasGT.data();
    r = vksCreateAccelerationStructureKHR(device, &blasInfo, nullptr, &staticBlas.as);
    VK_CHECKERROR(r);

    const auto &movableBlasGT = collectorStaticMovable->GetASGeometryTypesFiltered();
    VkAccelerationStructureCreateInfoKHR movableBlasInfo = {};
    movableBlasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    movableBlasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    movableBlasInfo.flags = 
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR |
        VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
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

    assert(asBuilder->IsEmpty());

    asBuilder->AddBLAS(staticBlas.as, 
                       geoms.size(), &pGeoms, offsets.data(), 
                       true, false);
    asBuilder->AddBLAS(staticMovableBlas.as, 
                       movableGeoms.size(), &pMovableGeoms, movableOffsets.data(), 
                       false, false);

    asBuilder->BuildBottomLevel(cmd);

    cmdManager->Submit(cmd, staticCopyFence);
    cmdManager->WaitForFence(staticCopyFence);
}

void ASManager::BeginDynamicGeometry(uint32_t frameIndex)
{
    currentFrameIndex = frameIndex;

    // dynamic AS must be recreated
    collectorDynamic[currentFrameIndex]->Reset();
    collectorDynamic[currentFrameIndex]->BeginCollecting();
}

void ASManager::SubmitDynamicGeometry(VkCommandBuffer cmd, uint32_t frameIndex)
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
    blasInfo.flags = 
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    blasInfo.maxGeometryCount = blasGT.size();
    blasInfo.pGeometryInfos = blasGT.data();
    r = vksCreateAccelerationStructureKHR(device, &blasInfo, nullptr, &dynamicBlas[currentFrameIndex].as);
    VK_CHECKERROR(r);

    AllocBindASMemory(dynBlas);

    colDyn->CopyFromStaging(cmd);

    const auto &geoms = colDyn->GetASGeometries();
    const auto &offsets = colDyn->GetASBuildOffsetInfos();
    const VkAccelerationStructureGeometryKHR *pGeoms = geoms.data();

    assert(asBuilder->IsEmpty());

    asBuilder->AddBLAS(dynBlas.as,
                       geoms.size(), &pGeoms, offsets.data(),
                       false, false);
    asBuilder->BuildBottomLevel(cmd);
}

void ASManager::UpdateStaticMovableTransform(uint32_t geomIndex, const RgTransform &transform)
{
    collectorStaticMovable->UpdateTransform(geomIndex, transform);
}

void ASManager::ResubmitStaticMovable(VkCommandBuffer cmd)
{
    const auto &movableGeoms = collectorStaticMovable->GetASGeometriesFiltered();
    const auto &movableOffsets = collectorStaticMovable->GetASBuildOffsetInfosFiltered();
    const VkAccelerationStructureGeometryKHR *pMovableGeoms = movableGeoms.data();

    assert(asBuilder->IsEmpty());
    asBuilder->AddBLAS(staticMovableBlas.as, 
                       movableGeoms.size(), &pMovableGeoms, movableOffsets.data(), 
                       false, true);

    asBuilder->BuildBottomLevel(cmd);
}

void ASManager::BuildTLAS(VkCommandBuffer cmd, uint32_t frameIndex)
{
    VkResult r;

    VkTransformMatrixKHR identity =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    std::vector<VkAccelerationStructureInstanceKHR> instances(3);

    {
        VkAccelerationStructureInstanceKHR &staticInstance = instances[0];
        staticInstance.transform = identity;
        staticInstance.instanceCustomIndex = 0;
        staticInstance.mask = 0xFF;
        staticInstance.instanceShaderBindingTableRecordOffset = 0;
        staticInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        staticInstance.accelerationStructureReference = GetASAddress(staticBlas);
    }
    {
        VkAccelerationStructureInstanceKHR &movableInstance = instances[1];
        movableInstance.transform = identity;
        movableInstance.instanceCustomIndex = 0;
        movableInstance.mask = 0xFF;
        movableInstance.instanceShaderBindingTableRecordOffset = 0;
        movableInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        movableInstance.accelerationStructureReference = GetASAddress(staticMovableBlas);
    }
    {
        VkAccelerationStructureInstanceKHR &dynamicInstance = instances[2];
        dynamicInstance.transform = identity;
        dynamicInstance.instanceCustomIndex = 0;
        dynamicInstance.mask = 0xFF;
        dynamicInstance.instanceShaderBindingTableRecordOffset = 0;
        dynamicInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        dynamicInstance.accelerationStructureReference = GetASAddress(dynamicBlas[frameIndex]);
    }

    void *mapped = instanceBuffers[frameIndex].Map();
    memcpy(mapped, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

    instanceBuffers[frameIndex].Unmap();


    VkAccelerationStructureCreateGeometryTypeInfoKHR geomTypeInfo = {};
    geomTypeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
    geomTypeInfo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geomTypeInfo.maxPrimitiveCount = instances.size();

    VkAccelerationStructureCreateInfoKHR asInfo = {};
    asInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    asInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    asInfo.maxGeometryCount = 1;
    asInfo.pGeometryInfos = &geomTypeInfo;
    r = vksCreateAccelerationStructureKHR(device, &asInfo, nullptr, &tlas[frameIndex].as);
    VK_CHECKERROR(r);

    AllocBindASMemory(tlas[frameIndex]);

    assert(asBuilder->IsEmpty());

    VkAccelerationStructureGeometryKHR instGeom = {};
    instGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    instGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    instGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    auto &instData = instGeom.geometry.instances;
    instData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instData.arrayOfPointers = VK_FALSE;
    instData.data.deviceAddress = instanceBuffers[frameIndex].GetAddress();

    VkAccelerationStructureBuildOffsetInfoKHR offset = {};
    offset.primitiveCount = instances.size();

    const VkAccelerationStructureGeometryKHR *pGeometry = &instGeom;

    asBuilder->AddTLAS(tlas[frameIndex].as, &pGeometry, &offset, true, false);
    asBuilder->BuildTopLevel(cmd);

    UpdateASDescriptors(frameIndex);
}



void ASManager::AllocBindASMemory(AccelerationStructure &as)
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

void ASManager::DestroyAS(AccelerationStructure &as)
{
    physDevice->FreeDeviceMemory(as.memory);
    vkDestroyAccelerationStructureKHR(device, as.as, nullptr);
}

VkDeviceAddress ASManager::GetASAddress(const AccelerationStructure& as)
{
    return GetASAddress(as.as);
}

VkDeviceAddress ASManager::GetASAddress(VkAccelerationStructureKHR as)
{
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = as;

    return vksGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}
