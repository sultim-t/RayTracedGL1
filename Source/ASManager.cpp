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
#include "CmdLabel.h"

using namespace RTGL1;

ASManager::ASManager(
    VkDevice _device,
    std::shared_ptr<MemoryAllocator> _allocator,
    std::shared_ptr<CommandBufferManager> _cmdManager,
    std::shared_ptr<TextureManager> _textureManager,
    std::shared_ptr<GeomInfoManager> _geomInfoManager,
    const VertexBufferProperties &_properties)
:
    device(_device),
    allocator(std::move(_allocator)),
    staticCopyFence(VK_NULL_HANDLE),
    cmdManager(std::move(_cmdManager)),
    textureMgr(std::move(_textureManager)),
    geomInfoMgr(std::move(_geomInfoManager)),
    descPool(VK_NULL_HANDLE),
    buffersDescSetLayout(VK_NULL_HANDLE),
    asDescSetLayout(VK_NULL_HANDLE),
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
    }


    scratchBuffer = std::make_shared<ScratchBuffer>(allocator);
    asBuilder = std::make_shared<ASBuilder>(device, scratchBuffer);


    // static and movable static vertices share the same buffer as their data won't be changing
    collectorStatic = std::make_shared<VertexCollector>(
        device, allocator, geomInfoMgr,
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
        device, allocator, geomInfoMgr,
        sizeof(ShVertexBufferDynamic), properties,
        FT::CF_DYNAMIC | 
        FT::MASK_PASS_THROUGH_GROUP | 
        FT::MASK_PRIMARY_VISIBILITY_GROUP);

    // other dynamic vertex collectors should share the same device local buffers as the first one
    for (uint32_t i = 1; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        collectorDynamic[i] = std::make_shared<VertexCollector>(collectorDynamic[0], allocator);
    }

    previousDynamicPositions.Init(
        allocator, sizeof(ShVertexBufferDynamic::positions),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Previous frame's vertex data");
    previousDynamicIndices.Init(
        allocator, sizeof(ShVertexBufferDynamic::positions),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Previous frame's index data");


    // instance buffer for TLAS
    instanceBuffer = std::make_unique<AutoBuffer>(device, allocator, "TLAS instance buffer staging", "TLAS instance buffer");

    VkDeviceSize instanceBufferSize = MAX_TOP_LEVEL_INSTANCE_COUNT * sizeof(VkAccelerationStructureInstanceKHR);
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

    SET_DEBUG_NAME(device, staticCopyFence, VK_OBJECT_TYPE_FENCE, "Static BLAS fence");
}

#pragma region AS descriptors

void ASManager::CreateDescriptors()
{
    VkResult r;

    {
        std::array<VkDescriptorSetLayoutBinding, 8> bindings{};

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

        bindings[4].binding = BINDING_GEOMETRY_INSTANCES;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_ALL;

        bindings[5].binding = BINDING_GEOMETRY_INSTANCES_MATCH_PREV;
        bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[5].descriptorCount = 1;
        bindings[5].stageFlags = VK_SHADER_STAGE_ALL;

        bindings[6].binding = BINDING_PREV_POSITIONS_BUFFER_DYNAMIC;
        bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[6].descriptorCount = 1;
        bindings[6].stageFlags = VK_SHADER_STAGE_ALL;

        bindings[7].binding = BINDING_PREV_INDEX_BUFFER_DYNAMIC;
        bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[7].descriptorCount = 1;
        bindings[7].stageFlags = VK_SHADER_STAGE_ALL;

        static_assert(sizeof(bindings) / sizeof(bindings[0]) == 8, "");

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings = bindings.data();

        r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &buffersDescSetLayout);
        VK_CHECKERROR(r);
    }

    {
        VkDescriptorSetLayoutBinding bnd = {};
        bnd.binding = BINDING_ACCELERATION_STRUCTURE_MAIN;
        bnd.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        bnd.descriptorCount = 1;
        bnd.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &bnd;

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

    SET_DEBUG_NAME(device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "AS manager Desc pool");

    VkDescriptorSetAllocateInfo descSetInfo = {};
    descSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetInfo.descriptorPool = descPool;
    descSetInfo.descriptorSetCount = 1;

    SET_DEBUG_NAME(device, buffersDescSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Vertex data Desc set layout");
    SET_DEBUG_NAME(device, asDescSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "TLAS Desc set layout");

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        descSetInfo.pSetLayouts = &buffersDescSetLayout;
        r = vkAllocateDescriptorSets(device, &descSetInfo, &buffersDescSets[i]);
        VK_CHECKERROR(r);

        descSetInfo.pSetLayouts = &asDescSetLayout;
        r = vkAllocateDescriptorSets(device, &descSetInfo, &asDescSets[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, buffersDescSets[i], VK_OBJECT_TYPE_DESCRIPTOR_SET, "Vertex data Desc set");
        SET_DEBUG_NAME(device, asDescSets[i], VK_OBJECT_TYPE_DESCRIPTOR_SET, "TLAS Desc set");
    }
}

void ASManager::UpdateBufferDescriptors(uint32_t frameIndex)
{
    const uint32_t bindingCount = 8;

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

    VkDescriptorBufferInfo &gsBufInfo = bufferInfos[BINDING_GEOMETRY_INSTANCES];
    gsBufInfo.buffer = geomInfoMgr->GetBuffer();
    gsBufInfo.offset = 0;
    gsBufInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo &gpBufInfo = bufferInfos[BINDING_GEOMETRY_INSTANCES_MATCH_PREV];
    gpBufInfo.buffer = geomInfoMgr->GetMatchPrevBuffer();
    gpBufInfo.offset = 0;
    gpBufInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo &ppBufInfo = bufferInfos[BINDING_PREV_POSITIONS_BUFFER_DYNAMIC];
    ppBufInfo.buffer = previousDynamicPositions.GetBuffer();
    ppBufInfo.offset = 0;
    ppBufInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo &piBufInfo = bufferInfos[BINDING_PREV_INDEX_BUFFER_DYNAMIC];
    piBufInfo.buffer = previousDynamicIndices.GetBuffer();
    piBufInfo.offset = 0;
    piBufInfo.range = VK_WHOLE_SIZE;


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

    VkWriteDescriptorSet &gmWrt = writes[BINDING_GEOMETRY_INSTANCES];
    gmWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    gmWrt.dstSet = buffersDescSets[frameIndex];
    gmWrt.dstBinding = BINDING_GEOMETRY_INSTANCES;
    gmWrt.dstArrayElement = 0;
    gmWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    gmWrt.descriptorCount = 1;
    gmWrt.pBufferInfo = &gsBufInfo;

    VkWriteDescriptorSet &gpWrt = writes[BINDING_GEOMETRY_INSTANCES_MATCH_PREV];
    gpWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    gpWrt.dstSet = buffersDescSets[frameIndex];
    gpWrt.dstBinding = BINDING_GEOMETRY_INSTANCES_MATCH_PREV;
    gpWrt.dstArrayElement = 0;
    gpWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    gpWrt.descriptorCount = 1;
    gpWrt.pBufferInfo = &gpBufInfo;
    
    VkWriteDescriptorSet &ppWrt = writes[BINDING_PREV_POSITIONS_BUFFER_DYNAMIC];
    ppWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    ppWrt.dstSet = buffersDescSets[frameIndex];
    ppWrt.dstBinding = BINDING_PREV_POSITIONS_BUFFER_DYNAMIC;
    ppWrt.dstArrayElement = 0;
    ppWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ppWrt.descriptorCount = 1;
    ppWrt.pBufferInfo = &ppBufInfo;

    VkWriteDescriptorSet &piWrt = writes[BINDING_PREV_INDEX_BUFFER_DYNAMIC];
    piWrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    piWrt.dstSet = buffersDescSets[frameIndex];
    piWrt.dstBinding = BINDING_PREV_INDEX_BUFFER_DYNAMIC;
    piWrt.dstArrayElement = 0;
    piWrt.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    piWrt.descriptorCount = 1;
    piWrt.pBufferInfo = &piBufInfo;

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}

void ASManager::UpdateASDescriptors(uint32_t frameIndex)
{
    VkAccelerationStructureKHR asHandle = tlas[frameIndex]->GetAS();
    assert(asHandle != VK_NULL_HANDLE);

    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {};
    asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &asHandle;

    VkWriteDescriptorSet wrt = {};
    wrt.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wrt.pNext = &asInfo;
    wrt.dstSet = asDescSets[frameIndex];
    wrt.dstBinding = BINDING_ACCELERATION_STRUCTURE_MAIN;
    wrt.dstArrayElement = 0;
    wrt.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    wrt.descriptorCount = 1;

    vkUpdateDescriptorSets(device, 1, &wrt, 0, nullptr);
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
    }

    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, buffersDescSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, asDescSetLayout, nullptr);
    vkDestroyFence(device, staticCopyFence, nullptr);
}

bool ASManager::SetupBLAS(BLASComponent &blas, const std::shared_ptr<VertexCollector> &vertCollector)
{
    auto filter = blas.GetFilter();
    const std::vector<VkAccelerationStructureGeometryKHR> &geoms = vertCollector->GetASGeometries(filter);

    blas.SetGeometryCount((uint32_t)geoms.size());

    if (blas.IsEmpty())
    {
        return false;
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
                       fastTrace, update, blas.GetFilter() & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE);

    return true;
}

void ASManager::UpdateBLAS(BLASComponent &blas, const std::shared_ptr<VertexCollector> &vertCollector)
{
    auto filter = blas.GetFilter();
    const std::vector<VkAccelerationStructureGeometryKHR> &geoms = vertCollector->GetASGeometries(filter);

    blas.SetGeometryCount((uint32_t)geoms.size());

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
                       fastTrace, update, blas.GetFilter() & VertexCollectorFilterTypeFlagBits::CF_STATIC_MOVABLE);
}

// separate functions to make adding between Begin..Geometry() and Submit..Geometry() a bit clearer

uint32_t ASManager::AddStaticGeometry(uint32_t frameIndex, const RgGeometryUploadInfo &info)
{
    if (info.geomType == RG_GEOMETRY_TYPE_STATIC || info.geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE)
    {
        MaterialTextures materials[3] =
        {
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[0]),
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[1]),
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[2])
        };

        return collectorStatic->AddGeometry(frameIndex, info, materials);
    }

    assert(0);
    return UINT32_MAX;
}

uint32_t ASManager::AddDynamicGeometry(uint32_t frameIndex, const RgGeometryUploadInfo &info)
{
    if (info.geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        MaterialTextures materials[3] =
        {
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[0]),
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[1]),
            textureMgr->GetMaterialTextures(info.geomMaterial.layerMaterials[2])
        };

        return collectorDynamic[frameIndex]->AddGeometry(frameIndex, info, materials);
    }

    assert(0);
    return UINT32_MAX;
}

void ASManager::ResetStaticGeometry()
{
    collectorStatic->Reset();
    geomInfoMgr->ResetWithStatic();
}

void ASManager::BeginStaticGeometry()
{
    // the whole static vertex data must be recreated, clear previous data
    collectorStatic->Reset();
    geomInfoMgr->ResetWithStatic();

    collectorStatic->BeginCollecting(true);
}

void ASManager::SubmitStaticGeometry()
{
    collectorStatic->EndCollecting();

    // static geometry submission happens very infrequently, e.g. on level load
    vkDeviceWaitIdle(device);

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
            staticBlas->SetGeometryCount(0);
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

void ASManager::BeginDynamicGeometry(VkCommandBuffer cmd, uint32_t frameIndex)
{
    scratchBuffer->Reset();

    static_assert(MAX_FRAMES_IN_FLIGHT == 2, "");
    uint32_t prevFrameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    // store data of current frame to use it in the next one
    CopyDynamicDataToPrevBuffers(cmd, prevFrameIndex);

    // dynamic AS must be recreated
    collectorDynamic[frameIndex]->Reset();
    collectorDynamic[frameIndex]->BeginCollecting(false);
}

void ASManager::SubmitDynamicGeometry(VkCommandBuffer cmd, uint32_t frameIndex)
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    CmdLabel label(cmd, "Building dynamic BLAS");

    const auto &colDyn = collectorDynamic[frameIndex];

    colDyn->EndCollecting();
    colDyn->CopyFromStaging(cmd, false);

    assert(asBuilder->IsEmpty());

    bool toBuild = false;

    // recreate dynamic blas
    for (auto &dynamicBlas : allDynamicBlas[frameIndex])
    {
        // must be dynamic
        assert(dynamicBlas->GetFilter() & FT::CF_DYNAMIC);

        toBuild |= SetupBLAS(*dynamicBlas, colDyn);
    }
    
    if (!toBuild)
    {
        return;
    }

    // build BLAS
    asBuilder->BuildBottomLevel(cmd);

    // sync AS access
    Utils::ASBuildMemoryBarrier(cmd);
}

void ASManager::UpdateStaticMovableTransform(uint32_t simpleIndex, const RgUpdateTransformInfo &updateInfo)
{
    collectorStatic->UpdateTransform(simpleIndex, updateInfo);
}

void RTGL1::ASManager::UpdateStaticTexCoords(uint32_t simpleIndex, const RgUpdateTexCoordsInfo &texCoordsInfo)
{
    collectorStatic->UpdateTexCoords(simpleIndex, texCoordsInfo);
}

void RTGL1::ASManager::ResubmitStaticTexCoords(VkCommandBuffer cmd)
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    if (collectorStatic->AreGeometriesEmpty(FT::CF_STATIC_NON_MOVABLE | FT::CF_STATIC_MOVABLE))
    {
        return;
    }

    CmdLabel label(cmd, "Recopying static tex coords");

    collectorStatic->RecopyTexCoordsFromStaging(cmd);
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

        if (blas->GetFilter() & FT::CF_STATIC_MOVABLE)
        {
            UpdateBLAS(*blas, collectorStatic);
        }
    }

    CmdLabel label(cmd, "Building static movable BLAS");

    // copy transforms to device-local memory
    collectorStatic->RecopyTransformsFromStaging(cmd);

    asBuilder->BuildBottomLevel(cmd);
}

bool ASManager::SetupTLASInstanceFromBLAS(const BLASComponent &blas, uint32_t rayCullMaskWorld, VkAccelerationStructureInstanceKHR &instance)
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


    if (filter & FT::PV_FIRST_PERSON)
    {
        instance.mask = INSTANCE_MASK_FIRST_PERSON;
        instance.instanceCustomIndex |= INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON;
    }
    else if (filter & FT::PV_FIRST_PERSON_VIEWER)
    {
        instance.mask = INSTANCE_MASK_FIRST_PERSON_VIEWER;
        instance.instanceCustomIndex |= INSTANCE_CUSTOM_INDEX_FLAG_FIRST_PERSON_VIEWER;
    }
    else
    {
        // also check rayCullMaskWorld, if world part is not included in the cull mask,
        // then don't add it to BLAS at all, it helps culling PT_REFLECT if it was a world part

        if (filter & FT::PV_WORLD_0)
        {
            instance.mask = INSTANCE_MASK_WORLD_0;

            if (!(rayCullMaskWorld & (1 << 0)))
            {
                instance = {};
                return false;
            }
        }
        else if (filter & FT::PV_WORLD_1)
        {
            instance.mask = INSTANCE_MASK_WORLD_1;

            if (!(rayCullMaskWorld & (1 << 1)))
            {
                instance = {};
                return false;
            }
        }
        else if (filter & FT::PV_WORLD_2)
        {
            instance.mask = INSTANCE_MASK_WORLD_2;

            if (!(rayCullMaskWorld & (1 << 2)))
            {
                instance = {};
                return false;
            }
        }
        else
        {
            assert(0);
        }
    }


    if (filter & FT::PT_REFLECT)
    {
        // completely rewrite mask, ignoring INSTANCE_MASK_WORLD_*,
        // if mask contains those world bits, then (mask & (~INSTANCE_MASK_REFLECT_REFRACT))
        // won't actually cull INSTANCE_MASK_REFLECT_REFRACT
        instance.mask = INSTANCE_MASK_REFLECT_REFRACT;
        instance.instanceCustomIndex |= INSTANCE_CUSTOM_INDEX_FLAG_REFLECT;
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
        
        instance.flags =
            VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR |
            VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    }


    return true;
}

static void WriteInstanceGeomInfo(int32_t *instanceGeomInfoOffset, int32_t *instanceGeomCount, uint32_t index, const BLASComponent &blas)
{
    assert(index < MAX_TOP_LEVEL_INSTANCE_COUNT);

    int32_t arrayOffset = VertexCollectorFilterTypeFlags_GetOffsetInGlobalArray(blas.GetFilter());
    int32_t geomCount = blas.GetGeomCount();

    // BLAS must not be empty, if it's added to TLAS
    assert(geomCount > 0);

    instanceGeomInfoOffset[index] = arrayOffset;
    instanceGeomCount[index] = geomCount;
}

void ASManager::PrepareForBuildingTLAS(
    uint32_t frameIndex,
    ShGlobalUniform &uniformData,
    ShVertPreprocessing *outPush,
    TLASPrepareResult *outResult) const
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    static_assert(sizeof(TLASPrepareResult::instances) / sizeof(TLASPrepareResult::instances[0]) == MAX_TOP_LEVEL_INSTANCE_COUNT, "Change TLASPrepareResult sizes");


    *outResult = {};
    *outPush = {};


    auto &r = *outResult;


    // write geometry offsets to uniform to access geomInfos
    // with instance ID and local (in terms of BLAS) geometry index in shaders;
    // Note: std140 requires elements to be aligned by sizeof(vec4)
    int32_t *instanceGeomInfoOffset = uniformData.instanceGeomInfoOffset;

    // write geometry counts of each BLAS for iterating in vertex preprocessing 
    int32_t *instanceGeomCount = uniformData.instanceGeomCount;

    const std::vector<std::unique_ptr<BLASComponent>> *blasArrays[] =
    {
        &allStaticBlas,
        &allDynamicBlas[frameIndex],
    };

    for (const auto *blasArr : blasArrays)
    {
        for (const auto &blas : *blasArr)
        {
            bool isDynamic = blas->GetFilter() & FT::CF_DYNAMIC;

            // add to TLAS instances array
            bool isAdded = ASManager::SetupTLASInstanceFromBLAS(*blas, uniformData.rayCullMaskWorld, r.instances[r.instanceCount]);

            if (isAdded)
            {
                // mark bit if dynamic
                if (isDynamic)
                {
                    outPush->tlasInstanceIsDynamicBits[r.instanceCount / MAX_TOP_LEVEL_INSTANCE_COUNT] |= 1 << (r.instanceCount % MAX_TOP_LEVEL_INSTANCE_COUNT);
                }

                WriteInstanceGeomInfo(instanceGeomInfoOffset, instanceGeomCount, r.instanceCount, *blas);
                r.instanceCount++;
            }
        }
    }

    outPush->tlasInstanceCount = r.instanceCount;
}

bool ASManager::TryBuildTLAS(VkCommandBuffer cmd, uint32_t frameIndex, const TLASPrepareResult &r)
{
    if (r.instanceCount == 0)
    {
        return false;
    }


    CmdLabel label(cmd, "Building TLAS");


    // fill buffer
    auto *mapped = (VkAccelerationStructureInstanceKHR*)instanceBuffer->GetMapped(frameIndex);

    memcpy(mapped, r.instances, r.instanceCount * sizeof(VkAccelerationStructureInstanceKHR));

    instanceBuffer->CopyFromStaging(cmd, frameIndex);


    TLASComponent *pCurrentTLAS = tlas[frameIndex].get();
    uint32_t instanceCount = r.instanceCount;


    VkAccelerationStructureGeometryKHR instGeom = {};
    instGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    instGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    instGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    auto &instData = instGeom.geometry.instances;
    instData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instData.arrayOfPointers = VK_FALSE;
    instData.data.deviceAddress = instanceBuffer->GetDeviceAddress();

    // get AS size and create buffer for AS
    VkAccelerationStructureBuildSizesInfoKHR buildSizes = asBuilder->GetTopBuildSizes(&instGeom, instanceCount, false);

    // if previous buffer's size is not enough
    pCurrentTLAS->RecreateIfNotValid(buildSizes, allocator);

    VkAccelerationStructureBuildRangeInfoKHR range = {};
    range.primitiveCount = instanceCount;


    // build
    assert(asBuilder->IsEmpty());

    assert(pCurrentTLAS->GetAS() != VK_NULL_HANDLE);
    asBuilder->AddTLAS(pCurrentTLAS->GetAS(), &instGeom, &range, buildSizes, true, false);

    asBuilder->BuildTopLevel(cmd);


    // sync AS access
    Utils::ASBuildMemoryBarrier(cmd);


    // shader desc access
    UpdateASDescriptors(frameIndex);


    return true;
}

void ASManager::CopyDynamicDataToPrevBuffers(VkCommandBuffer cmd, uint32_t frameIndex)
{
    uint32_t vertCount = collectorDynamic[frameIndex]->GetCurrentVertexCount();
    uint32_t indexCount = collectorDynamic[frameIndex]->GetCurrentIndexCount();

    if (vertCount > 0)
    {
        VkBufferCopy vertRegion = {};
        vertRegion.srcOffset = 0;
        vertRegion.dstOffset = 0;
        vertRegion.size = (uint64_t)vertCount * properties.positionStride;

        vkCmdCopyBuffer(
            cmd, 
            collectorDynamic[frameIndex]->GetVertexBuffer(), 
            previousDynamicPositions.GetBuffer(),
            1, &vertRegion);
    }

    if (indexCount > 0)
    {
        VkBufferCopy indexRegion = {};
        indexRegion.srcOffset = 0;
        indexRegion.dstOffset = 0;
        indexRegion.size = (uint64_t)indexCount * sizeof(uint32_t);

        vkCmdCopyBuffer(
            cmd, 
            collectorDynamic[frameIndex]->GetIndexBuffer(), 
            previousDynamicIndices.GetBuffer(),
            1, &indexRegion);
    }
}

void ASManager::OnVertexPreprocessingBegin(VkCommandBuffer cmd, uint32_t frameIndex, bool onlyDynamic)
{
    if (!onlyDynamic)
    {
        collectorStatic->InsertVertexPreprocessBeginBarrier(cmd);
    }

    collectorDynamic[frameIndex]->InsertVertexPreprocessBeginBarrier(cmd);
}

void ASManager::OnVertexPreprocessingFinish(VkCommandBuffer cmd, uint32_t frameIndex, bool onlyDynamic)
{
    if (!onlyDynamic)
    {
        collectorStatic->InsertVertexPreprocessFinishBarrier(cmd);
    }

    collectorDynamic[frameIndex]->InsertVertexPreprocessFinishBarrier(cmd);
}

bool ASManager::IsFastBuild(VertexCollectorFilterTypeFlags filter)
{
    typedef VertexCollectorFilterTypeFlagBits FT;

    // fast trace for static non-movable,
    // fast build for dynamic and movable
    // (TODO: fix: device lost occurs on heavy scenes if with movable)
    return (filter & FT::CF_DYNAMIC)/* || (filter & FT::CF_STATIC_MOVABLE)*/;
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
