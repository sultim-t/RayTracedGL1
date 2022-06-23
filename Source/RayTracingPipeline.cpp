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

#include "RayTracingPipeline.h"

#include "Generated/ShaderCommonC.h"
#include "Utils.h"
#include <cstring>
#include <string>

using namespace RTGL1;

RayTracingPipeline::RayTracingPipeline(
    VkDevice _device,
    std::shared_ptr<PhysicalDevice> _physDevice,
    std::shared_ptr<MemoryAllocator> _allocator,
    const std::shared_ptr<ShaderManager> &_shaderMgr,
    const std::shared_ptr<Scene> &_scene,
    const std::shared_ptr<GlobalUniform> &_uniform,
    const std::shared_ptr<TextureManager> &_textureMgr,
    const std::shared_ptr<Framebuffers> &_framebuffers,
    const std::shared_ptr<BlueNoise> &_blueNoise,
    const std::shared_ptr<CubemapManager> &_cubemapMgr,
    const std::shared_ptr<RenderCubemap> &_renderCubemap,
    const RgInstanceCreateInfo &_rgInfo)
:
    device(_device),
    physDevice(std::move(_physDevice)),
    rtPipelineLayout(VK_NULL_HANDLE),
    rtPipeline(VK_NULL_HANDLE),
    copySBTFromStaging(false),
    groupBaseAlignment(0),
    handleSize(0),
    alignedHandleSize(0),
    raygenShaderCount(0),
    hitGroupCount(0),
    missShaderCount(0),
    primaryRaysMaxAlbedoLayers(_rgInfo.primaryRaysMaxAlbedoLayers),
    indirectIlluminationMaxAlbedoLayers(_rgInfo.indirectIlluminationMaxAlbedoLayers)
{
    shaderBindingTable = std::make_shared<AutoBuffer>(device, std::move(_allocator));

    // all set layouts to be used
    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        // ray tracing acceleration structures
        _scene->GetASManager()->GetTLASDescSetLayout(),
        // storage images
        _framebuffers->GetDescSetLayout(),
        // uniform
        _uniform->GetDescSetLayout(),
        // vertex data
        _scene->GetASManager()->GetBuffersDescSetLayout(),
        // textures
        _textureMgr->GetDescSetLayout(),
        // uniform random
        _blueNoise->GetDescSetLayout(),
        // light sources
        _scene->GetLightManager()->GetDescSetLayout(),
        // cubemaps, for a cubemap type of skyboxes
        _cubemapMgr->GetDescSetLayout(),
        // dynamic cubemaps
        _renderCubemap->GetDescSetLayout()
    };

    CreatePipelineLayout(setLayouts.data(), setLayouts.size());

    assert(primaryRaysMaxAlbedoLayers <= MATERIALS_MAX_LAYER_COUNT);
    assert(indirectIlluminationMaxAlbedoLayers <= MATERIALS_MAX_LAYER_COUNT);

    // shader modules in the pipeline will have the exact order
    shaderStageInfos =
    {
        { "RGenPrimary",            &primaryRaysMaxAlbedoLayers },
        { "RGenReflRefr",           &primaryRaysMaxAlbedoLayers },
        { "RGenDirect",             nullptr },
        { "RGenIndirect",           &indirectIlluminationMaxAlbedoLayers },
        { "RGenGradients",          nullptr },
        { "RInitialReservoirs",     nullptr },
        { "RMiss",                  nullptr },
        { "RMissShadow",            nullptr },
        { "RClsOpaque",             nullptr },
        { "RAlphaTest",             nullptr },
    };

#pragma region Utilities
    // simple lambda to get index in "stages" by name
    auto toIndex = [this] (const char *shaderName)
    {
        for (uint32_t i = 0; i < shaderStageInfos.size(); i++)
        {
            if (std::strcmp(shaderName, shaderStageInfos[i].pName) == 0)
            {
                return i;
            }
        }

        assert(0);
        return UINT32_MAX;
    };
#pragma endregion


    // set shader binding table structure the same as defined with SBT_INDEX_* 

    AddRayGenGroup(toIndex("RGenPrimary"));                         assert(raygenShaderCount - 1 == SBT_INDEX_RAYGEN_PRIMARY);
    AddRayGenGroup(toIndex("RGenReflRefr"));                        assert(raygenShaderCount - 1 == SBT_INDEX_RAYGEN_REFL_REFR);
    AddRayGenGroup(toIndex("RGenDirect"));                          assert(raygenShaderCount - 1 == SBT_INDEX_RAYGEN_DIRECT);
    AddRayGenGroup(toIndex("RGenIndirect"));                        assert(raygenShaderCount - 1 == SBT_INDEX_RAYGEN_INDIRECT);
    AddRayGenGroup(toIndex("RGenGradients"));                       assert(raygenShaderCount - 1 == SBT_INDEX_RAYGEN_GRADIENTS);
    AddRayGenGroup(toIndex("RInitialReservoirs"));                  assert(raygenShaderCount - 1 == SBT_INDEX_RAYGEN_INITIAL_RESERVOIRS);

    AddMissGroup(toIndex("RMiss"));                                 assert(missShaderCount - 1 == SBT_INDEX_MISS_DEFAULT);
    AddMissGroup(toIndex("RMissShadow"));                           assert(missShaderCount - 1 == SBT_INDEX_MISS_SHADOW);

    // only opaque
    AddHitGroup(toIndex("RClsOpaque"));                             assert(hitGroupCount - 1 == SBT_INDEX_HITGROUP_FULLY_OPAQUE);
    // alpha tested and then opaque
    AddHitGroup(toIndex("RClsOpaque"), toIndex("RAlphaTest"));      assert(hitGroupCount - 1 == SBT_INDEX_HITGROUP_ALPHA_TESTED);

    CreatePipeline(_shaderMgr.get());
    CreateSBT();
}

RayTracingPipeline::~RayTracingPipeline()
{
    DestroyPipeline();
    vkDestroyPipelineLayout(device, rtPipelineLayout, nullptr);
}

void RayTracingPipeline::CreatePipelineLayout(const VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount)
{
    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;

    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &rtPipelineLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, rtPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Ray tracing pipeline Layout");
}

void RayTracingPipeline::CreatePipeline(const ShaderManager *shaderManager)
{
    std::vector<VkPipelineShaderStageCreateInfo> stages(shaderStageInfos.size());

    // for optional specialization constants,
    // pre-allocate whole vector to prevent wrong pointers in pSpecializationInfo
    std::vector<VkSpecializationMapEntry> specEntries(shaderStageInfos.size());
    std::vector<VkSpecializationInfo> specInfos(shaderStageInfos.size());

    for (uint32_t i = 0; i < shaderStageInfos.size(); i++)
    {
        stages[i] = shaderManager->GetStageInfo(shaderStageInfos[i].pName);

        if (shaderStageInfos[i].pSpecConst != nullptr)
        {
            VkSpecializationMapEntry &specEntry = specEntries[i];
            specEntry.constantID = 0;
            specEntry.offset = 0;
            specEntry.size = sizeof(uint32_t);

            VkSpecializationInfo &specInfo = specInfos[i];
            specInfo.mapEntryCount = 1;
            specInfo.pMapEntries = &specEntry;
            specInfo.dataSize = sizeof(*shaderStageInfos[i].pSpecConst);
            specInfo.pData = shaderStageInfos[i].pSpecConst;

            stages[i].pSpecializationInfo = &specInfo;
        }
    }

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

    VkResult r = svkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rtPipeline);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, rtPipeline, VK_OBJECT_TYPE_PIPELINE, "Ray tracing pipeline");
}

void RayTracingPipeline::DestroyPipeline()
{
    vkDestroyPipeline(device, rtPipeline, nullptr);
    rtPipeline = VK_NULL_HANDLE;
}

void RayTracingPipeline::CreateSBT()
{
    VkResult r;

    uint32_t groupCount = shaderGroups.size();
    groupBaseAlignment = physDevice->GetRTPipelineProperties().shaderGroupBaseAlignment;

    handleSize = physDevice->GetRTPipelineProperties().shaderGroupHandleSize;
    alignedHandleSize = Utils::Align(handleSize, groupBaseAlignment);

    uint32_t sbtSize = alignedHandleSize * groupCount;

    shaderBindingTable->Create(
        sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
        "SBT",
        1);

    std::vector<uint8_t> shaderHandles(handleSize * groupCount);
    r = svkGetRayTracingShaderGroupHandlesKHR(device, rtPipeline, 0, groupCount, shaderHandles.size(), shaderHandles.data());
    VK_CHECKERROR(r);

    uint8_t *mapped = (uint8_t *) shaderBindingTable->GetMapped(0);

    for (uint32_t i = 0; i < groupCount; i++)
    {
        memcpy(
            mapped + i * alignedHandleSize, 
            shaderHandles.data() + i * handleSize,
            handleSize);
    }

    copySBTFromStaging = true;
}

void RayTracingPipeline::DestroySBT()
{
    shaderBindingTable->Destroy();
}

void RayTracingPipeline::Bind(VkCommandBuffer cmd)
{
    if (copySBTFromStaging)
    {
        shaderBindingTable->CopyFromStaging(cmd, 0);
        copySBTFromStaging = false;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);
}

void RayTracingPipeline::GetEntries(
    uint32_t sbtRayGenIndex,
    VkStridedDeviceAddressRegionKHR &raygenEntry,
    VkStridedDeviceAddressRegionKHR &missEntry,
    VkStridedDeviceAddressRegionKHR &hitEntry,
    VkStridedDeviceAddressRegionKHR &callableEntry) const
{
    assert(sbtRayGenIndex == SBT_INDEX_RAYGEN_PRIMARY   || 
           sbtRayGenIndex == SBT_INDEX_RAYGEN_REFL_REFR ||
           sbtRayGenIndex == SBT_INDEX_RAYGEN_DIRECT    ||
           sbtRayGenIndex == SBT_INDEX_RAYGEN_INDIRECT  ||
           sbtRayGenIndex == SBT_INDEX_RAYGEN_GRADIENTS ||
           sbtRayGenIndex == SBT_INDEX_RAYGEN_INITIAL_RESERVOIRS);

    VkDeviceAddress bufferAddress = shaderBindingTable->GetDeviceAddress();

    uint64_t offset = 0;


    raygenEntry = {};
    raygenEntry.deviceAddress = bufferAddress + offset + (uint64_t)sbtRayGenIndex * alignedHandleSize;
    raygenEntry.stride = alignedHandleSize;
    raygenEntry.size = alignedHandleSize;
    // vk spec
    assert(raygenEntry.size == raygenEntry.stride);

    offset += (uint64_t)raygenShaderCount * alignedHandleSize;


    missEntry = {};
    missEntry.deviceAddress = bufferAddress + offset;
    missEntry.stride = alignedHandleSize;
    missEntry.size = (uint64_t)missShaderCount * alignedHandleSize;

    offset += (uint64_t)missShaderCount * alignedHandleSize;


    hitEntry = {};
    hitEntry.deviceAddress = bufferAddress + offset;
    hitEntry.stride = alignedHandleSize;
    hitEntry.size = (uint64_t)hitGroupCount * alignedHandleSize;

    offset += (uint64_t)hitGroupCount * alignedHandleSize;


    callableEntry = {};
}

VkPipelineLayout RayTracingPipeline::GetLayout() const
{
    return rtPipelineLayout;
}

void RayTracingPipeline::OnShaderReload(const ShaderManager *shaderManager)
{
    DestroySBT();
    DestroyPipeline();

    CreatePipeline(shaderManager);
    CreateSBT();
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

void RayTracingPipeline::AddRayGenGroup(uint32_t raygenIndex)
{
    AddGeneralGroup(raygenIndex);

    raygenShaderCount++;
}

void RayTracingPipeline::AddMissGroup(uint32_t missIndex)
{
    AddGeneralGroup(missIndex);

    missShaderCount++;
}

void RayTracingPipeline::AddHitGroup(uint32_t closestHitIndex)
{
    AddHitGroup(closestHitIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
}

void RayTracingPipeline::AddHitGroupOnlyAny(uint32_t anyHitIndex)
{
    AddHitGroup(VK_SHADER_UNUSED_KHR, anyHitIndex, VK_SHADER_UNUSED_KHR);
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
    group.generalShader = VK_SHADER_UNUSED_KHR;
    group.closestHitShader = closestHitIndex;
    group.anyHitShader = anyHitIndex;
    group.intersectionShader = intersectionIndex;

    shaderGroups.push_back(group);

    hitGroupCount++;
}
