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

#include "ASManager.h"

#include <array>

#include "Utils.h"
#include "Generated/ShaderCommonC.h"

constexpr bool ONLY_MAIN_TLAS = false;

using namespace RTGL1;

ASManager::ASManager(
    VkDevice _device,
    std::shared_ptr<MemoryAllocator> _allocator,
    std::shared_ptr<CommandBufferManager> _cmdManager,
    std::shared_ptr<TextureManager> _textureMgr,
    const VertexBufferProperties &_properties)
    :
    device(_device),
    allocator(std::move(_allocator)),
    cmdManager(std::move(_cmdManager)),
    textureMgr(std::move(_textureMgr)),
    properties(_properties)
{
    typedef VertexCollectorFilterTypeFlags FL;
    typedef VertexCollectorFilterTypeFlagBits FT;


    // init AS structs for each dimension
    VertexCollectorFilterTypeFlags_IterateOverFlags([this] (FL filter)
    {
        if (filter & FT::CF_DYNAMIC)
        {
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                allDynamicBlas[i].emplace_back(std::make_unique<BLASComponent>(device, filter));
            }
        }
        else
        {
            allStaticBlas.emplace_back(std::make_unique<BLASComponent>(device, filter));
        }
    });

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        tlas[i] = std::make_unique<TLASComponent>(device, "TLAS main");
        skyboxTlas[i] = std::make_unique<TLASComponent>(device, "TLAS skybox");
    }


    scratchBuffer = std::make_shared<ScratchBuffer>(allocator);
    asBuilder = std::make_shared<ASBuilder>(device, scratchBuffer);


    // static and movable static vertices share the same buffer as their data won't be changing
    collectorStatic = std::make_shared<VertexCollector>(
        device, allocator,
        sizeof(ShVertexBufferStatic), properties,
        FT::CF_STATIC_NON_MOVABLE | FT::CF_STATIC_MOVABLE | 
        FT::MASK_PASS_THROUGH_GROUP | 
        FT::MASK_PRIMARY_VISIBILITY_GROUP);

    // subscribe to texture manager only static collector,
    // as static geometries aren't updating its material info (in ShGeometryInstance)
    // every frame unlike dynamic ones
    textureMgr->Subscribe(collectorStatic);


    // dynamic vertices
    collectorDynamic[0] = std::make_shared<VertexCollector>(
        device, allocator,
        sizeof(ShVertexBufferDynamic), properties,
        FT::CF_DYNAMIC | 
        FT::MASK_PASS_THROUGH_GROUP | 
        FT::MASK_PRIMARY_VISIBILITY_GROUP);

    // other dynamic vertex collectors should share the same device local buffers as the first one
    for (uint32_t i = 1; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        collectorDynamic[i] = std::make_shared<VertexCollector>(collectorDynamic[0], allocator);
    }


    // instance buffer for TLAS
    instanceBuffer = std::make_unique<AutoBuffer>(device, allocator, "TLAS instance buffer staging", "TLAS instance buffer");

    // multiplying by 2 for main/skybox
    VkDeviceSize instanceBufferSize = 2 * MAX_TOP_LEVEL_INSTANCE_COUNT * sizeof(VkAccelerationStructureInstanceKHR);
    instanceBuffer->Create(instanceBufferSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);


    CreateDescriptors();

    // buffers won't be changing, update once
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateBufferDescriptors(i);
    }


    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    VkResult r = vkCreateFence(device, &fenceInfo, nullptr, &staticCopyFence);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, staticCopyFence, VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT, "Static BLAS fence");
}

#pragma region AS descriptors

void ASManager::CreateDescriptors()
{
    VkResult r;

    {
        std::array<VkDescriptorSetLayoutBinding, 6> bindings{};

        // static vertex data
        bindings[0].binding = BINDING_VERTEX_BUFFER_STATIC;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

        // dynamic vertex data
        bindings[1].binding = BINDING_VERTEX_BUFFER_DYNAMIC;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_ALL;

        bindings[2].binding = BINDING_INDEX_BUFFER_STATIC;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_ALL;

        bindings[3].binding = BINDING_INDEX_BUFFER_DYNAMIC;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_ALL;

        bindings[4].binding = BINDING_GEOMETRY_INSTANCES_STATIC;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_ALL;

        bindings[5].binding = BINDING_GEOMETRY_INSTANCES_DYNAMIC;
        bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[5].descriptorCount = 1;
        bindings[5].stageFlags = VK_SHADER_STAGE_ALL;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings = bindings.data();

        r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &buffersDescSetLayout);
        VK_CHECKERROR(r);
    }

    {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

        bindings[0].binding = BINDING_ACCELERATION_STRUCTURE_MAIN;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        bindings[1].binding = BINDING_ACCELERATION_STRUCTURE_SKYBOX;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = ONLY_MAIN_TLAS ? 1 : bindings.size();
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

    SET_DEBUG_NAME(device, descPool, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT, "AS manager Desc pool");

    VkDescriptorSetAllocateInfo descSetInfo = {};
    descSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetInfo.descriptorPool = descPool;
    descSetInfo.descriptorSetCount = 1;

    SET_DEBUG_NAME(device, buffersDescSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, "Vertex data Desc set layout");
    SET_DEBUG_NAME(device, asDescSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, "TLAS Desc set layout");

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        descSetInfo.pSetLayouts = &buffersDescSetLayout;
        r = vkAllocateDescriptorSets(device, &descSetInfo, &buffersDescSets[i]);
        VK_CHECKERROR(r);

        descSetInfo.pSetLayouts = &asDescSetLayout;
        r = vkAllocateDescriptorSets(device, &descSetInfo, &asDescSets[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, buffersDescSets[i], VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, "Vertex data Desc set");
        SET_DEBUG_NAME(device, asDescSets[i], VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, "TLAS Desc set");
    }
}

void ASManager::UpdateBufferDescriptors(uint32_t frameIndex)
{
    const uint32_t bindingCount = 6;

    std::array<VkDescriptorBufferInfo, bindingCount> bufferInfos{};
    std::array<VkWriteDescriptorSet, bindingCount> writes{};

    // buffer infos
    VkDescriptorBufferInfo &stVertsBufInfo = bufferInfos[BINDING_VERTEX_BUFFER_STATIC];
    stVertsBufInfo.buffer = collectorStatic->GetVertexBuffer();
    stVertsBufInfo.offset = 0;
    stVertsBufInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo &dnVertsBufInfo = bufferInfos[BINDING_VERTEX_BUFFER_DYNAMIC];
    dnVertsBufInfo.buffer = collectorDynamic[frameIndex]->GetVertexBuffer();
    dnVertsBufInfo.offset = 0;
    dnVertsBufInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo &stIndexBufInfo = bufferInfos[BINDING_INDEX_BUFFER_STATIC];
    stIndexBufInfo.buffer = collectorStatic->GetIndexBuffer();
    stIndexBufInfo.offset = 0;
    stIndexBufInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo &dnIndexBufInfo = bufferInfos[BINDING_INDEX_BUFFER_DYNAMIC];
    dnIndexBufInfo.buffer = collectorDynamic[frameIndex]->GetIndexBuffer();
    dnIndexBufInfo.offset = 0;
    dnIndexBufInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo &gsBufInfo = bufferInfos[BINDING_GEOMETRY_INSTANCES_STATIC];
    gsBufInfo.buffer = collectorStatic->GetGeometryInfosBuffer();
    gsBufInfo.offset = 0;
    gsBufInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo &gdBufInfo = bufferInfos[BINDING_GEOMETRY_INSTANCES_DYNAMIC];
    gdBufInfo.buffer = collectorDynamic[frameIndex]->GetGeometryInfosBuffer();
    gdBufInfo.offset = 0;
    gdBufInfo.range = VK_WHOLE_SIZE;


    // writes
    VkWriteDescriptorSet &stVertWrt = writes[BINDING_VERTEX_BUFFER_STATIC];
    stVertWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    stVertWrt.dstSet = buffersDescSets[frameIndex];
    stVertWrt.dstBinding = BINDING_VERTEX_BUFFER_STATIC;
    stVertWrt.dstArrayElement = 0;
    stVertWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    stVertWrt.descriptorCount = 1;
    stVertWrt.pBufferInfo = &stVertsBufInfo;

    VkWriteDescriptorSet &dnVertWrt = writes[BINDING_VERTEX_BUFFER_DYNAMIC];
    dnVertWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    dnVertWrt.dstSet = buffersDescSets[frameIndex];
    dnVertWrt.dstBinding = BINDING_VERTEX_BUFFER_DYNAMIC;
    dnVertWrt.dstArrayElement = 0;
    dnVertWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dnVertWrt.descriptorCount = 1;
    dnVertWrt.pBufferInfo = &dnVertsBufInfo;

    VkWriteDescriptorSet &stIndexWrt = writes[BINDING_INDEX_BUFFER_STATIC];
    stIndexWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    stIndexWrt.dstSet = buffersDescSets[frameIndex];
    stIndexWrt.dstBinding = BINDING_INDEX_BUFFER_STATIC;
    stIndexWrt.dstArrayElement = 0;
    stIndexWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    stIndexWrt.descriptorCount = 1;
    stIndexWrt.pBufferInfo = &stIndexBufInfo;

    VkWriteDescriptorSet &dnIndexWrt = writes[BINDING_INDEX_BUFFER_DYNAMIC];
    dnIndexWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    dnIndexWrt.dstSet = buffersDescSets[frameIndex];
    dnIndexWrt.dstBinding = BINDING_INDEX_BUFFER_DYNAMIC;
    dnIndexWrt.dstArrayElement = 0;
    dnIndexWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dnIndexWrt.descriptorCount = 1;
    dnIndexWrt.pBufferInfo = &dnIndexBufInfo;

    VkWriteDescriptorSet &gmStWrt = writes[BINDING_GEOMETRY_INSTANCES_STATIC];
    gmStWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    gmStWrt.dstSet = buffersDescSets[frameIndex];
    gmStWrt.dstBinding = BINDING_GEOMETRY_INSTANCES_STATIC;
    gmStWrt.dstArrayElement = 0;
    gmStWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    gmStWrt.descriptorCount = 1;
    gmStWrt.pBufferInfo = &gsBufInfo;

    VkWriteDescriptorSet &gmDnWrt = writes[BINDING_GEOMETRY_INSTANCES_DYNAMIC];
    gmDnWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    gmDnWrt.dstSet = buffersDescSets[frameIndex];
    gmDnWrt.dstBinding = BINDING_GEOMETRY_INSTANCES_DYNAMIC;
    gmDnWrt.dstArrayElement = 0;
    gmDnWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    gmDnWrt.descriptorCount = 1;
    gmDnWrt.pBufferInfo = &gdBufInfo;

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

void ASManager::UpdateASDescriptors(uint32_t frameIndex)
{
    uint32_t bindings[] =
    {
        BINDING_ACCELERATION_STRUCTURE_MAIN,
        BINDING_ACCELERATION_STRUCTURE_SKYBOX,
    };

    TLASComponent *allTLAS[] =
    {
        tlas[frameIndex].get(),
        skyboxTlas[frameIndex].get(),
    };
    constexpr uint32_t allTLASCount = ONLY_MAIN_TLAS ? 1 : sizeof(allTLAS) / sizeof(allTLAS[0]);

    VkAccelerationStructureKHR asHandles[allTLASCount] = {};
    VkWriteDescriptorSetAccelerationStructureKHR asInfos[allTLASCount] = {};
    VkWriteDescriptorSet writes[allTLASCount] = {};

    for (uint32_t i = 0; i < allTLASCount; i++)
    {
        asHandles[i] = allTLAS[i]->GetAS();
        assert(asHandles[i] != VK_NULL_HANDLE);

        VkWriteDescriptorSetAccelerationStructureKHR &asInfo = asInfos[i];
        asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &asHandles[i];

        VkWriteDescriptorSet &wrt = writes[i];
        wrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wrt.pNext = &asInfo;
        wrt.dstSet = asDescSets[frameIndex];
        wrt.dstBinding = bindings[i];
        wrt.dstArrayElement = 0;
        wrt.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        wrt.descriptorCount = 1;
    }

    vkUpdateDescriptorSets(device, allTLASCount, writes, 0, nullptr);
}

#pragma endregion 

ASManager::~ASManager()
{
    for (auto &as : allStaticBlas)
    {
        as->Destroy();
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        for (auto &as : allDynamicBlas[i])
        {
            as->Destroy();
        }

        tlas[i]->Destroy();
        skyboxTlas[i]->Destroy();
    }

    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, buffersDescSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, asDescSetLayout, nullptr);
    vkDestroyFence(device, staticCopyFence, nullptr);
}

void ASManager::SetupBLAS(BLASComponent &blas, const std::shared_ptr<VertexCollector> &vertCollector)
{
    auto filter = blas.GetFilter();
    const std::vector<VkAccelerationStructureGeometryKHR> &geoms = vertCollector->GetASGeometries(filter);

    blas.RegisterGeometries(geoms);

    if (blas.IsEmpty())
    {
        return;
    }

    const std::vector<VkAccelerationStructureBuildRangeInfoKHR> &ranges = vertCollector->GetASBuildRangeInfos(filter);
    const std::vector<uint32_t> &primCounts = vertCollector->GetPrimitiveCounts(filter);

    const bool fastTrace = !IsFastBuild(filter);
    const bool update = false;

    // get AS size and create buffer for AS
    const auto buildSizes = asBuilder->GetBottomBuildSizes(geoms.size(), geoms.data(), primCounts.data(), fastTrace);

    // if no buffer, or it was created, but its size is too small for current AS
    blas.RecreateIfNotValid(buildSizes, allocator);

    assert(blas.GetAS() != VK_NULL_HANDLE);

    // add BLAS, all passed arrays must be alive until BuildBottomLevel() call
    asBuilder->AddBLAS(blas.GetAS(), geoms.size(),
                       geoms.data(), ranges.data(),
                       buildSizes,
                       fastTrace, update);
}

void ASManager::UpdateBLAS(BLASComponent &blas, const std::shared_ptr<VertexCollector> &vertCollector)
{
    auto filter = blas.GetFilter();
    const std::vector<VkAccelerationStructureGeometryKHR> &geoms = vertCollector->GetASGeometries(filter);

    blas.RegisterGeometries(geoms);

    if (blas.IsEmpty())
    {
        return;
    }

    const std::vector<VkAccelerationStructureBuildRangeInfoKHR> &ranges = vertCollector->GetASBuildRangeInfos(filter);
    const std::vector<uint32_t> &primCounts = vertCollector->GetPrimitiveCounts(filter);

    const bool fastTrace = !IsFastBuild(filter);

    // must be just updated
    const bool update = true;

    const auto buildSizes = asBuilder->GetBottomBuildSizes(
        geoms.size(), geoms.data(), primCounts.data(), fastTrace);

    assert(blas.IsValid(buildSizes));
    assert(blas.GetAS() != VK_NULL_HANDLE);

    // add BLAS, all passed arrays must be alive until BuildBottomLevel() call
    asBuilder->AddBLAS(blas.GetAS(), geoms.size(),
                       geoms.data(), ranges.data(),
                       buildSizes,
                       fastTrace, update);
}

// separate functions to make adding between Begin..Geometry() and Submit..Geometry() a bit clearer

uint32_t ASManager::AddStaticGeometry(const RgGeometryUploadInfo &info)
{
    if (info.geomType == RG_GEOMETRY_TYPE_STATIC || info.geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE)
    {
        MaterialTextures materials[3] =
        {
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[0]),
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[1]),
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[2])
        };

        return collectorStatic->AddGeometry(info, materials);
    }

    assert(0);
    return UINT32_MAX;
}

uint32_t ASManager::AddDynamicGeometry(const RgGeometryUploadInfo &info, uint32_t frameIndex)
{
    if (info.geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        MaterialTextures materials[3] =
        {
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[0]),
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[1]),
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[2])
        };

        return collectorDynamic[frameIndex]->AddGeometry(info, materials);
    }

    assert(0);
    return UINT32_MAX;
}

void ASManager::ResetStaticGeometry()
{
    collectorStatic->Reset();
}

void ASManager::BeginStaticGeometry()
{
    // the whole static vertex data must be recreated, clear previous data
    collectorStatic->Reset();
    collectorStatic->BeginCollecting();
}

void ASManager::SubmitStaticGeometry()
{
    collectorStatic->EndCollecting();

    typedef VertexCollectorFilterTypeFlagBits FT;

    auto staticFlags = FT::CF_STATIC_NON_MOVABLE | FT::CF_STATIC_MOVABLE;

    // destroy previous static
    for (auto &staticBlas : allStaticBlas)
    {
        assert(!(staticBlas->GetFilter() & FT::CF_DYNAMIC));

        // if flags have any of static bits
        if (staticBlas->GetFilter() & staticFlags)
        {
            staticBlas->Destroy();
        }
    }

    assert(asBuilder->IsEmpty());

    // skip if all static geometries are empty
    if (collectorStatic->AreGeometriesEmpty(staticFlags))
    {
        return;
    }

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    // copy from staging with barrier
    collectorStatic->CopyFromStaging(cmd, true);

    // setup static blas
    for (auto &staticBlas : allStaticBlas)
    {
        // if flags have any of static bits
        if (staticBlas->GetFilter() & staticFlags)
        {
            SetupBLAS(*staticBlas, collectorStatic);
        }
    }
    
    // build AS
    asBuilder->BuildBottomLevel(cmd);

    // submit and wait
    cmdManager->Submit(cmd, staticCopyFence);
    Utils::WaitAndResetFence(device, staticCopyFence);
}

void ASManager::BeginDynamicGeometry(uint32_t frameIndex)
{
    // dynamic AS must be recreated
    collectorDynamic[frameIndex]->Reset();
    collectorDynamic[frameIndex]->BeginCollecting();
}

void ASManager::SubmitDynamicGeometry(VkCommandBuffer cmd, uint32_t frameIndex)
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    const auto &colDyn = collectorDynamic[frameIndex];

    colDyn->EndCollecting();
    colDyn->CopyFromStaging(cmd, false);

    assert(asBuilder->IsEmpty());

    if (colDyn->AreGeometriesEmpty(FT::CF_DYNAMIC))
    {
        return;
    }

    // recreate dynamic blas
    for (auto &dynamicBlas : allDynamicBlas[frameIndex])
    {
        // must be dynamic
        assert(dynamicBlas->GetFilter() & FT::CF_DYNAMIC);

        SetupBLAS(*dynamicBlas, colDyn);
    }

    // build BLAS
    asBuilder->BuildBottomLevel(cmd);
}

void ASManager::UpdateStaticMovableTransform(uint32_t geomIndex, const RgTransform &transform)
{
    collectorStatic->UpdateTransform(geomIndex, transform);
}

void ASManager::ResubmitStaticMovable(VkCommandBuffer cmd)
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    if (collectorStatic->AreGeometriesEmpty(FT::CF_STATIC_MOVABLE))
    {
        return;
    }

    assert(asBuilder->IsEmpty());

    // update movable blas
    for (auto &blas : allStaticBlas)
    {
        assert(!(blas->GetFilter() & FT::CF_DYNAMIC));

        // if flags have any of static bits
        if (blas->GetFilter() & FT::CF_STATIC_MOVABLE)
        {
            auto &movableBlas = blas;

            UpdateBLAS(*blas, collectorStatic);
        }
    }

    // copy transforms to device-local memory
    collectorStatic->CopyTransformsFromStaging(cmd);

    asBuilder->BuildBottomLevel(cmd);
}

bool ASManager::SetupTLASInstanceFromBLAS(const BLASComponent &blas, VkAccelerationStructureInstanceKHR &instance)
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    if (blas.GetAS() == VK_NULL_HANDLE || blas.IsEmpty())
    {
        return false;
    }

    auto filter = blas.GetFilter();

    instance.accelerationStructureReference = blas.GetASAddress();

    instance.transform = 
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    instance.instanceCustomIndex = 0;

    if (filter & FT::CF_DYNAMIC)
    {
        // for choosing buffers with dynamic data
        instance.instanceCustomIndex = INSTANCE_CUSTOM_INDEX_FLAG_DYNAMIC;
    }
    // blended geometry doesn't have indirect illumination

    if (filter & (/*FT::PT_BLEND_ADDITIVE |*/ FT::PT_BLEND_UNDER))
    {
        instance.mask = INSTANCE_MASK_BLENDED;
    }
    else if (filter & FT::PV_FIRST_PERSON)
    {
        instance.mask = INSTANCE_MASK_FIRST_PERSON;
        instance.instanceCustomIndex |= INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON;
    }
    else if (filter & FT::PV_FIRST_PERSON_VIEWER)
    {
        instance.mask = INSTANCE_MASK_FIRST_PERSON_VIEWER;
        instance.instanceCustomIndex |= INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON_VIEWER;
    }
    else if (filter & FT::PV_SKYBOX)
    {
        instance.mask = INSTANCE_MASK_SKYBOX;
        instance.instanceCustomIndex |= INSTANCE_CUSTOM_INDEX_FLAG_SKYBOX;
    }
    else
    {
        instance.mask = INSTANCE_MASK_WORLD;
    }

    if (filter & FT::PT_OPAQUE)
    {
        instance.instanceShaderBindingTableRecordOffset = SBT_INDEX_HITGROUP_FULLY_OPAQUE;
        instance.flags =
            VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR |
            VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    }
    else 
    {
        if (filter & FT::PT_ALPHA_TESTED)
        {
            instance.instanceShaderBindingTableRecordOffset = SBT_INDEX_HITGROUP_ALPHA_TESTED;
        }
        /*else if (filter &FT::PT_BLEND_ADDITIVE)
        {
            instance.instanceShaderBindingTableRecordOffset = SBT_INDEX_HITGROUP_BLEND_ADDITIVE;
        }*/
        else if (filter & FT::PT_BLEND_UNDER)
        {
            instance.instanceShaderBindingTableRecordOffset = SBT_INDEX_HITGROUP_BLEND_UNDER;
        }
        
        instance.flags =
            VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR |
            VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    }

    return true;
}

static void WriteInstanceGeomInfoOffset(int32_t *instanceGeomInfoOffset, uint32_t index, uint32_t skyboxIndex, VertexCollectorFilterTypeFlags flags)
{
    assert(index < MAX_TOP_LEVEL_INSTANCE_COUNT);
    assert(skyboxIndex < MAX_TOP_LEVEL_INSTANCE_COUNT);

    uint32_t arrayOffset = VertexCollectorFilterTypeFlags_ToOffset(flags) * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT;

    bool isSkybox = flags & VertexCollectorFilterTypeFlagBits::PV_SKYBOX;

    if (!isSkybox)
    {
        instanceGeomInfoOffset[index] = arrayOffset;
    }
    else
    {
        uint32_t skyboxStartIndex = MAX_TOP_LEVEL_INSTANCE_COUNT;
        instanceGeomInfoOffset[skyboxStartIndex + skyboxIndex] = arrayOffset;
    }
}

bool ASManager::TryBuildTLAS(VkCommandBuffer cmd, uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, bool ignoreSkyboxTLAS)
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    // BLAS instances
    uint32_t instanceCount = 0;
    VkAccelerationStructureInstanceKHR instances[MAX_TOP_LEVEL_INSTANCE_COUNT] = {};

    uint32_t skyboxInstanceCount = 0;
    VkAccelerationStructureInstanceKHR skyboxInstances[MAX_TOP_LEVEL_INSTANCE_COUNT] = {};

    // for getting offsets in geomInfos buffer by instance ID in shaders,
    // this array will be copied to uniform buffer
    // note: std140 requires elements to be aligned by sizeof(vec4)
    int32_t instanceGeomInfoOffset[sizeof(ShGlobalUniform::instanceGeomInfoOffset) / sizeof(int32_t)];

    std::vector<std::unique_ptr<BLASComponent>> *blasArrays[] =
    {
        &allStaticBlas,
        &allDynamicBlas[frameIndex],
    };

    for (auto *blasArr : blasArrays)
    {
        for (auto &blas : *blasArr)
        {
            // add to appropriate TLAS instances array
            if (blas->GetFilter() & FT::PV_SKYBOX)
            {
                bool isAdded = ASManager::SetupTLASInstanceFromBLAS(*blas, skyboxInstances[skyboxInstanceCount]);

                if (isAdded)
                {
                    // if skybox TLAS is ignored, skybox geometry must not be previously added
                    assert(!ignoreSkyboxTLAS);

                    WriteInstanceGeomInfoOffset(instanceGeomInfoOffset, instanceCount, skyboxInstanceCount, blas->GetFilter());
                    skyboxInstanceCount++;
                }
            }
            else
            {
                bool isAdded = ASManager::SetupTLASInstanceFromBLAS(*blas, instances[instanceCount]);

                if (isAdded)
                {
                    WriteInstanceGeomInfoOffset(instanceGeomInfoOffset, instanceCount, skyboxInstanceCount, blas->GetFilter());
                    instanceCount++;
                }
            }
        }
    }

    if (instanceCount == 0 && skyboxInstanceCount == 0)
    {
        return false;
    }

    // copy geometry offsets to uniform to access geomInfos
    // with instance ID and geometry index in shaders;
    memcpy(uniform->GetData()->instanceGeomInfoOffset, instanceGeomInfoOffset, sizeof(instanceGeomInfoOffset));


    // fill buffer
    auto *mapped = (VkAccelerationStructureInstanceKHR*)instanceBuffer->GetMapped(frameIndex);

    memcpy(mapped, instances, instanceCount * sizeof(VkAccelerationStructureInstanceKHR));
    memcpy(mapped + MAX_TOP_LEVEL_INSTANCE_COUNT, skyboxInstances, skyboxInstanceCount * sizeof(VkAccelerationStructureInstanceKHR));

    instanceBuffer->CopyFromStaging(cmd, frameIndex);


    TLASComponent *allTLAS[] =
    {
        tlas[frameIndex].get(),
        skyboxTlas[frameIndex].get(),
    };
    constexpr uint32_t allTLASCount = ONLY_MAIN_TLAS ? 1 : sizeof(allTLAS) / sizeof(allTLAS[0]);

    VkAccelerationStructureGeometryKHR instGeoms[allTLASCount] = {};
    VkAccelerationStructureBuildSizesInfoKHR buildSizes[allTLASCount] = {};
    VkAccelerationStructureBuildRangeInfoKHR ranges[allTLASCount] = {};

    uint32_t instanceCounts[allTLASCount] =
    {
        instanceCount,
        skyboxInstanceCount
    };

    for (uint32_t i = 0; i < allTLASCount; i++)
    {
        VkAccelerationStructureGeometryKHR &instGeom = instGeoms[i];

        instGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        instGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        instGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        auto &instData = instGeom.geometry.instances;
        instData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instData.arrayOfPointers = VK_FALSE;
        instData.data.deviceAddress = 
            instanceBuffer->GetDeviceAddress()
            + sizeof(VkAccelerationStructureInstanceKHR) * MAX_TOP_LEVEL_INSTANCE_COUNT * i;

        // get AS size and create buffer for AS
        buildSizes[i] = asBuilder->GetTopBuildSizes(&instGeom, instanceCounts[i], false);

        // if previous buffer's size is not enough
        allTLAS[i]->RecreateIfNotValid(buildSizes[i], allocator);

        ranges[i].primitiveCount = instanceCounts[i];
    }

    uint32_t tlasToBuild = ignoreSkyboxTLAS ? 1 : allTLASCount;

    for (uint32_t i = 0; i < tlasToBuild; i++)
    {
        assert(asBuilder->IsEmpty());

        assert(allTLAS[i]->GetAS() != VK_NULL_HANDLE);
        asBuilder->AddTLAS(allTLAS[i]->GetAS(), &instGeoms[i], &ranges[i], buildSizes[i], true, false);

        asBuilder->BuildTopLevel(cmd);
    }


    UpdateASDescriptors(frameIndex);
    return true;
}

bool ASManager::IsFastBuild(VertexCollectorFilterTypeFlags filter)
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    // fast trace for static
    // fast build for dynamic
    return filter & FT::CF_DYNAMIC;
}

VkDescriptorSet ASManager::GetBuffersDescSet(uint32_t frameIndex) const
{
    return buffersDescSets[frameIndex];
}

VkDescriptorSet ASManager::GetTLASDescSet(uint32_t frameIndex) const
{
    // if TLAS wasn't built, return null
    if (tlas[frameIndex]->GetAS() == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }

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
