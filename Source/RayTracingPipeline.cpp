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

using namespace RTGL1;

RayTracingPipeline::RayTracingPipeline(
    VkDevice _device,
    const std::shared_ptr<PhysicalDevice> &_physDevice,
    const std::shared_ptr<MemoryAllocator> &_allocator,
    const std::shared_ptr<ShaderManager> &_shaderMgr,
    const std::shared_ptr<ASManager> &_asMgr,
    const std::shared_ptr<GlobalUniform> &_uniform,
    const std::shared_ptr<TextureManager> &_textureMgr,
    const std::shared_ptr<Framebuffers> &_framebuffers,
    const std::shared_ptr<BlueNoise> &_blueNoise)
:
    device(_device),
    rtPipelineLayout(VK_NULL_HANDLE),
    rtPipeline(VK_NULL_HANDLE),
    groupBaseAlignment(0),
    handleSize(0),
    alignedHandleSize(0),
    raygenShaderCount(0),
    hitGroupCount(0),
    missShaderCount(0)
{
    std::vector<const char *> stageNames =
    {
        "RGenPrimary",
        "RGenDirect",
        "RGenIndirect",
        "RMiss",
        "RMissShadow",
        "RClsOpaque",
        "RAlphaTest",
        "RBlendAdditive",
        "RBlendUnder",
    };

#pragma region Utilities
    // get stage infos by names 
    std::vector<VkPipelineShaderStageCreateInfo> stages(stageNames.size());

    for (uint32_t i = 0; i < stageNames.size(); i++)
    {
        stages[i] = _shaderMgr->GetStageInfo(stageNames[i]);
    }

    // simple lambda to get index in "stages" by name
    auto toIndex = [&stageNames] (const char *shaderName)
    {
        for (uint32_t i = 0; i < stageNames.size(); i++)
        {
            if (std::strcmp(shaderName, stageNames[i]) == 0)
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
    AddRayGenGroup(toIndex("RGenDirect"));                          assert(raygenShaderCount - 1 == SBT_INDEX_RAYGEN_DIRECT);
    AddRayGenGroup(toIndex("RGenIndirect"));                        assert(raygenShaderCount - 1 == SBT_INDEX_RAYGEN_INDIRECT);

    AddMissGroup(toIndex("RMiss"));                                 assert(missShaderCount - 1 == SBT_INDEX_MISS_DEFAULT);
    AddMissGroup(toIndex("RMissShadow"));                           assert(missShaderCount - 1 == SBT_INDEX_MISS_SHADOW);

    // only opaque
    AddHitGroup(toIndex("RClsOpaque"));                             assert(hitGroupCount - 1 == SBT_INDEX_HITGROUP_FULLY_OPAQUE);
    // alpha tested and then opaque
    AddHitGroup(toIndex("RClsOpaque"), toIndex("RAlphaTest"));      assert(hitGroupCount - 1 == SBT_INDEX_HITGROUP_ALPHA_TESTED);
    // blend additive and then opaque
    AddHitGroup(toIndex("RClsOpaque"), toIndex("RBlendAdditive"));  assert(hitGroupCount - 1 == SBT_INDEX_HITGROUP_BLEND_ADDITIVE);
    // blend under and then opaque
    AddHitGroup(toIndex("RClsOpaque"), toIndex("RBlendUnder"));     assert(hitGroupCount - 1 == SBT_INDEX_HITGROUP_BLEND_UNDER);


    // all set layouts to be used
    std::vector<VkDescriptorSetLayout> setLayouts =
    {
        // ray tracing acceleration structures
        _asMgr->GetTLASDescSetLayout(),
        // storage images
        _framebuffers->GetDescSetLayout(),
        // uniform
        _uniform->GetDescSetLayout(),
        // vertex data
        _asMgr->GetBuffersDescSetLayout(),
        // textures
        _textureMgr->GetDescSetLayout(),
        // uniform random
        _blueNoise->GetDescSetLayout()
    };

    CreatePipeline(setLayouts.data(), setLayouts.size(),
                   stages.data(), stages.size());

    CreateSBT(_physDevice, _allocator);
}

RayTracingPipeline::~RayTracingPipeline()
{
    vkDestroyPipeline(device, rtPipeline, nullptr);
    vkDestroyPipelineLayout(device, rtPipelineLayout, nullptr);
}

void RayTracingPipeline::CreatePipeline(const VkDescriptorSetLayout *pSetLayouts, uint32_t setLayoutCount,
                                        const VkPipelineShaderStageCreateInfo *pStages, uint32_t stageCount)
{
    VkPipelineLayoutCreateInfo plLayoutInfo = {};
    plLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plLayoutInfo.setLayoutCount = setLayoutCount;
    plLayoutInfo.pSetLayouts = pSetLayouts;

    VkResult r = vkCreatePipelineLayout(device, &plLayoutInfo, nullptr, &rtPipelineLayout);
    VK_CHECKERROR(r);

    VkPipelineLibraryCreateInfoKHR libInfo = {};
    libInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = stageCount;
    pipelineInfo.pStages = pStages;
    pipelineInfo.groupCount = shaderGroups.size();
    pipelineInfo.pGroups = shaderGroups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 2;
    pipelineInfo.layout = rtPipelineLayout;
    pipelineInfo.pLibraryInfo = &libInfo;

    r = svkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rtPipeline);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, rtPipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Ray tracing pipeline Layout");
    SET_DEBUG_NAME(device, rtPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Ray tracing pipeline");
}

void RayTracingPipeline::CreateSBT(
    const std::shared_ptr<PhysicalDevice> &physDevice, 
    const std::shared_ptr<MemoryAllocator> &allocator)
{
    VkResult r;

    uint32_t groupCount = shaderGroups.size();
    groupBaseAlignment = physDevice->GetRTPipelineProperties().shaderGroupBaseAlignment;

    handleSize = physDevice->GetRTPipelineProperties().shaderGroupHandleSize;
    alignedHandleSize = Utils::Align(handleSize, groupBaseAlignment);

    uint32_t sbtSize = alignedHandleSize * groupCount;

    shaderBindingTable = std::make_shared<Buffer>();
    shaderBindingTable->Init(
        allocator, sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "Shader binding table buffer");

    std::vector<uint8_t> shaderHandles(handleSize * groupCount);
    r = svkGetRayTracingShaderGroupHandlesKHR(device, rtPipeline, 0, groupCount, shaderHandles.size(), shaderHandles.data());
    VK_CHECKERROR(r);

    uint8_t *mapped = (uint8_t *) shaderBindingTable->Map();

    for (uint32_t i = 0; i < groupCount; i++)
    {
        memcpy(
            mapped + i * alignedHandleSize, 
            shaderHandles.data() + i * handleSize,
            handleSize);
    }

    shaderBindingTable->Unmap();
}

void RayTracingPipeline::Bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);
}

void RayTracingPipeline::GetEntries(
    uint32_t sbtRayGenIndex,
    VkStridedDeviceAddressRegionKHR &raygenEntry,
    VkStridedDeviceAddressRegionKHR &missEntry,
    VkStridedDeviceAddressRegionKHR &hitEntry,
    VkStridedDeviceAddressRegionKHR &callableEntry) const
{
    assert(sbtRayGenIndex == SBT_INDEX_RAYGEN_PRIMARY || 
           sbtRayGenIndex == SBT_INDEX_RAYGEN_DIRECT  ||
           sbtRayGenIndex == SBT_INDEX_RAYGEN_INDIRECT);

    VkDeviceAddress bufferAddress = shaderBindingTable->GetAddress();

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