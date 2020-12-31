#include "ASManager.h"

#include <array>

#include "Generated/ShaderCommonC.h"

ASManager::ASManager(VkDevice device, std::shared_ptr<PhysicalDevice> physDevice, 
                     std::shared_ptr<CommandBufferManager> cmdManager,
                     const VBProperties &properties)
{
    this->device = device;
    this->physDevice = physDevice;
    this->cmdManager = cmdManager;
    this->properties = properties;

    scratchBuffer = std::make_shared<ScratchBuffer>(device, physDevice);
    asBuilder = std::make_shared<ASBuilder>(device, scratchBuffer);

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
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
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
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
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
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
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
        VkDescriptorBufferInfo &staticBufInfo = bufferInfos[i * 2];
        staticBufInfo.buffer = staticVertsBuffer->GetBuffer();
        staticBufInfo.offset = 0;
        staticBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo &dynamicBufInfo = bufferInfos[i * 2 + 1];
        dynamicBufInfo.buffer = dynamicVertsBuffer[i]->GetBuffer();
        dynamicBufInfo.offset = 0;
        dynamicBufInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet &staticWrt = writes[i * 2];
        staticWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        staticWrt.dstSet = buffersDescSets[i];
        staticWrt.dstBinding = BINDING_VERTEX_BUFFER_STATIC;
        staticWrt.dstArrayElement = 0;
        staticWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        staticWrt.descriptorCount = 1;
        staticWrt.pBufferInfo = &staticBufInfo;

        VkWriteDescriptorSet &dynamicWrt = writes[i * 2 + 1];
        dynamicWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        dynamicWrt.dstSet = buffersDescSets[i];
        dynamicWrt.dstBinding = BINDING_VERTEX_BUFFER_DYNAMIC;
        dynamicWrt.dstArrayElement = 0;
        dynamicWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        dynamicWrt.descriptorCount = 1;
        dynamicWrt.pBufferInfo = &dynamicBufInfo;
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
        wrt.dstSet = asDescSets[i];
        wrt.dstBinding = BINDING_ACCELERATION_STRUCTURE;
        wrt.dstArrayElement = 0;
        wrt.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        wrt.descriptorCount = 1;
    }

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

ASManager::~ASManager()
{
    DestroyAS(staticBlas);
    DestroyAS(staticMovableBlas);

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        DestroyAS(dynamicBlas[i]);
        DestroyAS(tlas[i]);
    }

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
    return UINT32_MAX;
}

uint32_t ASManager::AddDynamicGeometry(const RgGeometryUploadInfo &info, uint32_t frameIndex)
{
    if (info.geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        return collectorDynamic[frameIndex]->AddGeometry(info);
    }

    assert(0);
    return UINT32_MAX;
}

void ASManager::ResetStaticGeometry()
{
    collectorStaticMovable->Reset();
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

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    // copy from staging with barrier
    collectorStaticMovable->CopyFromStaging(cmd);

    const auto &staticGeoms = collectorStaticMovable->GetASGeometries();
    const auto &movableGeoms = collectorStaticMovable->GetASGeometriesFiltered();

    const auto &staticRanges = collectorStaticMovable->GetASBuildRangeInfos();
    const auto &movableRanges = collectorStaticMovable->GetASBuildRangeInfosFiltered();

    const auto &staticPrimCounts = collectorStaticMovable->GetPrimitiveCounts();
    const auto &movablePrimCounts = collectorStaticMovable->GetPrimitiveCountsFiltered();

    // destroy previous
    DestroyAS(staticBlas);
    DestroyAS(staticMovableBlas);

    // get AS size and create buffer for AS
    const auto staticBuildSizes = asBuilder->GetBottomBuildSizes(
        staticGeoms.size(), staticGeoms.data(), staticPrimCounts.data(), true);
    const auto movableBuildSizes = asBuilder->GetBottomBuildSizes(
        movableGeoms.size(), movableGeoms.data(), movablePrimCounts.data(), true);

    CreateASBuffer(staticBlas, staticBuildSizes.accelerationStructureSize);
    CreateASBuffer(staticMovableBlas, movableBuildSizes.accelerationStructureSize);

    VkResult r;

    // create AS
    VkAccelerationStructureCreateInfoKHR blasInfo = {};
    blasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasInfo.size = staticBuildSizes.accelerationStructureSize;
    blasInfo.buffer = staticBlas.buffer.GetBuffer();
    r = svkCreateAccelerationStructureKHR(device, &blasInfo, nullptr, &staticBlas.as);
    VK_CHECKERROR(r);

    VkAccelerationStructureCreateInfoKHR movableBlasInfo = {};
    movableBlasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    movableBlasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    movableBlasInfo.size = movableBuildSizes.accelerationStructureSize;
    movableBlasInfo.buffer = staticMovableBlas.buffer.GetBuffer();
    r = svkCreateAccelerationStructureKHR(device, &movableBlasInfo, nullptr, &staticMovableBlas.as);
    VK_CHECKERROR(r);

    // build BLAS
    assert(asBuilder->IsEmpty());

    asBuilder->AddBLAS(staticBlas.as, staticGeoms.size(),
                       staticGeoms.data(), staticRanges.data(),
                       staticBuildSizes,
                       true, false);
    asBuilder->AddBLAS(staticMovableBlas.as, movableGeoms.size(),
                       movableGeoms.data(), movableRanges.data(),
                       movableBuildSizes,
                       false, false);

    asBuilder->BuildBottomLevel(cmd);

    cmdManager->Submit(cmd, staticCopyFence);
    cmdManager->WaitForFence(staticCopyFence);
}

void ASManager::BeginDynamicGeometry(uint32_t frameIndex)
{
    // dynamic AS must be recreated
    collectorDynamic[frameIndex]->Reset();
    collectorDynamic[frameIndex]->BeginCollecting();
}

void ASManager::SubmitDynamicGeometry(VkCommandBuffer cmd, uint32_t frameIndex)
{
    const auto &colDyn = collectorDynamic[frameIndex];
    auto &dynBlas = dynamicBlas[frameIndex];

    colDyn->EndCollecting();
    colDyn->CopyFromStaging(cmd);

    const auto &geoms = colDyn->GetASGeometries();
    const auto &ranges = colDyn->GetASBuildRangeInfos();
    const auto &counts = colDyn->GetPrimitiveCounts();

    DestroyAS(dynBlas);

    // get AS size and create buffer for AS
    const auto dynamicBuildSizes = asBuilder->GetBottomBuildSizes(
        geoms.size(), geoms.data(), counts.data(), false);

    CreateASBuffer(dynamicBlas[frameIndex], dynamicBuildSizes.accelerationStructureSize);

    // create AS
    VkAccelerationStructureCreateInfoKHR blasInfo = {};
    blasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasInfo.size = dynamicBuildSizes.accelerationStructureSize;
    blasInfo.buffer = dynamicBlas[frameIndex].buffer.GetBuffer();
    VkResult r = svkCreateAccelerationStructureKHR(device, &blasInfo, nullptr, &dynamicBlas[frameIndex].as);
    VK_CHECKERROR(r);

    // build BLAS
    assert(asBuilder->IsEmpty());

    asBuilder->AddBLAS(dynBlas.as,
                       geoms.size(), geoms.data(), ranges.data(),
                       dynamicBuildSizes,
                       false, false);
    asBuilder->BuildBottomLevel(cmd);
}

void ASManager::UpdateStaticMovableTransform(uint32_t geomIndex, const RgTransform &transform)
{
    collectorStaticMovable->UpdateTransform(geomIndex, transform);
}

void ASManager::ResubmitStaticMovable(VkCommandBuffer cmd)
{
    const auto &geoms = collectorStaticMovable->GetASGeometriesFiltered();
    const auto &ranges = collectorStaticMovable->GetASBuildRangeInfosFiltered();
    const auto &primCounts = collectorStaticMovable->GetPrimitiveCountsFiltered();

    const auto buildSizes = asBuilder->GetBottomBuildSizes(
        geoms.size(), geoms.data(), primCounts.data(), true);

    assert(asBuilder->IsEmpty());
    asBuilder->AddBLAS(staticMovableBlas.as,
                       geoms.size(), geoms.data(), ranges.data(),
                       buildSizes,
                       false, true);

    asBuilder->BuildBottomLevel(cmd);
}

void ASManager::BuildTLAS(VkCommandBuffer cmd, uint32_t frameIndex)
{
    VkTransformMatrixKHR identity =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    // BLAS instances
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

    // fill buffer
    void *mapped = instanceBuffers[frameIndex].Map();
    memcpy(mapped, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

    instanceBuffers[frameIndex].Unmap();

    DestroyAS(tlas[frameIndex]);

    VkAccelerationStructureGeometryKHR instGeom = {};
    instGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    instGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    instGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    auto &instData = instGeom.geometry.instances;
    instData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instData.arrayOfPointers = VK_FALSE;
    instData.data.deviceAddress = instanceBuffers[frameIndex].GetAddress();

    uint32_t primCount = instances.size();

    // get AS size and create buffer for AS
    const auto buildSizes = asBuilder->GetTopBuildSizes(&instGeom, &primCount, false);

    CreateASBuffer(tlas[frameIndex], buildSizes.accelerationStructureSize);

    // create AS
    VkAccelerationStructureCreateInfoKHR tlasInfo = {};
    tlasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasInfo.size = buildSizes.accelerationStructureSize;
    tlasInfo.buffer = tlas[frameIndex].buffer.GetBuffer();
    VkResult r = svkCreateAccelerationStructureKHR(device, &tlasInfo, nullptr, &tlas[frameIndex].as);
    VK_CHECKERROR(r);

    assert(asBuilder->IsEmpty());

    VkAccelerationStructureBuildRangeInfoKHR range = {};
    range.primitiveCount = primCount;

    asBuilder->AddTLAS(tlas[frameIndex].as, &instGeom, &range, buildSizes, true, false);
    asBuilder->BuildTopLevel(cmd);

    UpdateASDescriptors(frameIndex);
}

void ASManager::CreateASBuffer(AccelerationStructure &as, VkDeviceSize size)
{
    as.buffer.Init(
        device, *physDevice, size,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

void ASManager::DestroyAS(AccelerationStructure &as)
{
    as.buffer.Destroy();

    if (as.as != VK_NULL_HANDLE)
    {
        svkDestroyAccelerationStructureKHR(device, as.as, nullptr);
    }
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

    return svkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}

VkDescriptorSet ASManager::GetBuffersDescSet(uint32_t frameIndex) const
{
    return buffersDescSets[frameIndex];
}

VkDescriptorSet ASManager::GetTLASDescSet(uint32_t frameIndex) const
{
    return asDescSets[frameIndex];
}

VkDescriptorSetLayout ASManager::GetBuffersDescSetLayout() const
{
    return buffersDescSetLayout;
}

VkDescriptorSetLayout ASManager::GetTLASDescSetLayout() const
{
    return asDescSetLayout;
}
