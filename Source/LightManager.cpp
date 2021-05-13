// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include "LightManager.h"

#include <cmath>
#include <array>

#include "Generated/ShaderCommonC.h"

namespace RTGL1
{
constexpr double RG_PI = 3.141592653589793238462643383279502884197169399375105820974944592307816406;

constexpr uint32_t START_MAX_LIGHT_COUNT_SPHERICAL = 1024;
constexpr uint32_t START_MAX_LIGHT_COUNT_DIRECTIONAL = 32;

constexpr uint32_t STEP_LIGHT_COUNT_SPHERICAL = 1024;
constexpr uint32_t STEP_LIGHT_COUNT_DIRECTIONAL = 32;
}

RTGL1::LightManager::LightManager(
    VkDevice _device, 
    std::shared_ptr<MemoryAllocator> &_allocator)
:
    device(_device),
    sphLightCount(0),
    dirLightCount(0),
    sphLightCountPrev(0),
    dirLightCountPrev(0),
    maxSphericalLightCount(START_MAX_LIGHT_COUNT_SPHERICAL),
    maxDirectionalLightCount(START_MAX_LIGHT_COUNT_DIRECTIONAL),
    descSetLayout(VK_NULL_HANDLE),
    descPool(VK_NULL_HANDLE),
    descSets{},
    needDescSetUpdate{}
{
    sphericalLights           = std::make_shared<AutoBuffer>(device, _allocator, "Lights spherical staging", "Lights spherical");
    directionalLights         = std::make_shared<AutoBuffer>(device, _allocator, "Lights directional staging", "Lights directional");
    sphericalLightMatchPrev   = std::make_shared<AutoBuffer>(device, _allocator, "Match previous Lights spherical staging", "Match previous Lights spherical");
    directionalLightMatchPrev = std::make_shared<AutoBuffer>(device, _allocator, "Match previous Lights directional staging", "Match previous Lights directional");

    sphericalLights->Create(sizeof(ShLightSpherical) * maxSphericalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    directionalLights->Create(sizeof(ShLightDirectional) * maxDirectionalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    sphericalLightMatchPrev->Create(sizeof(uint32_t) * maxSphericalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    directionalLightMatchPrev->Create(sizeof(uint32_t) * maxDirectionalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    CreateDescriptors();
}

RTGL1::LightManager::~LightManager()
{
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);
}

void RTGL1::LightManager::PrepareForFrame(uint32_t frameIndex)
{
    sphLightCountPrev = sphLightCount;
    dirLightCountPrev = dirLightCount;

    sphLightCount = 0;
    dirLightCount = 0;

    memset(sphericalLightMatchPrev->GetMapped(frameIndex), 0xFF, sizeof(uint32_t) * sphLightCountPrev);
    memset(directionalLightMatchPrev->GetMapped(frameIndex), 0xFF, sizeof(uint32_t) *  dirLightCountPrev);

    sphUniqueIDToPrevIndex[frameIndex].clear();
    dirUniqueIDToPrevIndex[frameIndex].clear();
}

void RTGL1::LightManager::Reset()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        memset(sphericalLightMatchPrev->GetMapped(i), 0xFF, sizeof(uint32_t) * std::max(sphLightCount, dirLightCountPrev));
        memset(directionalLightMatchPrev->GetMapped(i), 0xFF, sizeof(uint32_t) *  std::max(sphLightCount, dirLightCountPrev));

        sphUniqueIDToPrevIndex[i].clear();
        dirUniqueIDToPrevIndex[i].clear();
    }

    sphLightCount = sphLightCountPrev = 0;
    dirLightCount = dirLightCountPrev = 0;
}

uint32_t RTGL1::LightManager::GetSphericalLightCount() const
{
    return sphLightCount;
}

uint32_t RTGL1::LightManager::GetDirectionalLightCount() const
{
    return dirLightCount;
}

uint32_t RTGL1::LightManager::GetSphericalLightCountPrev() const
{
    return sphLightCountPrev;
}

uint32_t RTGL1::LightManager::GetDirectionalLightCountPrev() const
{
    return dirLightCountPrev;
}

void RTGL1::LightManager::AddSphericalLight(uint32_t frameIndex, const RgSphericalLightUploadInfo &info)
{
    uint32_t index = sphLightCount;
    sphLightCount++;

    if (sphLightCount >= maxSphericalLightCount)
    {
        // TODO: schedule buffer to be destroyed in the next frame
        return;
    }

    auto *dst = (ShLightSpherical*)sphericalLights->GetMapped(frameIndex);
    FillInfo(info, &dst[index]);

    FillMatchPrev(sphUniqueIDToPrevIndex, sphericalLightMatchPrev, frameIndex, index, info.uniqueID);

    // must be unique
    assert(sphUniqueIDToPrevIndex[frameIndex].find(info.uniqueID) == sphUniqueIDToPrevIndex[frameIndex].end());

    // save index for the next frame
    sphUniqueIDToPrevIndex[frameIndex][info.uniqueID] = index;
}

void RTGL1::LightManager::AddDirectionalLight(uint32_t frameIndex, const RgDirectionalLightUploadInfo &info)
{
    uint32_t index = dirLightCount;
    dirLightCount++;

    if (dirLightCount >= maxDirectionalLightCount)
    {
        // TODO: schedule buffer to be destroyed in the next frame
        return;
    }

    auto *dst = (ShLightDirectional*)directionalLights->GetMapped(frameIndex);
    FillInfo(info, &dst[index]);

    FillMatchPrev(dirUniqueIDToPrevIndex, directionalLightMatchPrev, frameIndex, index, info.uniqueID);
    
    // must be unique
    assert(dirUniqueIDToPrevIndex[frameIndex].find(info.uniqueID) == dirUniqueIDToPrevIndex[frameIndex].end());

    // save index for the next frame
    dirUniqueIDToPrevIndex[frameIndex][info.uniqueID] = index;
}

void RTGL1::LightManager::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex)
{
    sphericalLights->CopyFromStaging(cmd, frameIndex, sizeof(ShLightSpherical) * sphLightCount);
    directionalLights->CopyFromStaging(cmd, frameIndex, sizeof(ShLightDirectional) * dirLightCount);

    sphericalLightMatchPrev->CopyFromStaging(cmd, frameIndex, sizeof(uint32_t) * sphLightCountPrev);
    directionalLightMatchPrev->CopyFromStaging(cmd, frameIndex, sizeof(uint32_t) * dirLightCountPrev);

    if (needDescSetUpdate[frameIndex])
    {
        UpdateDescriptors(frameIndex);
        needDescSetUpdate[frameIndex] = false;
    }
}

VkDescriptorSetLayout RTGL1::LightManager::GetDescSetLayout()
{
    return descSetLayout;
}

VkDescriptorSet RTGL1::LightManager::GetDescSet(uint32_t frameIndex)
{
    return descSets[frameIndex];
}

void RTGL1::LightManager::FillMatchPrev(
    const std::map<uint64_t, uint32_t> *pUniqueToPrevIndex,
    const std::shared_ptr<AutoBuffer> &matchPrev,
    uint32_t curFrameIndex, uint32_t curLightIndex, uint64_t uniqueID)
{
    uint32_t prevFrame = (curFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    const std::map<uint64_t, uint32_t> &uniqueToPrevIndex = pUniqueToPrevIndex[prevFrame];

    auto found = uniqueToPrevIndex.find(uniqueID);

    if (found != uniqueToPrevIndex.end())
    {        
        uint32_t prevLightIndex = found->second;

        uint32_t *dst = (uint32_t*)matchPrev->GetMapped(curFrameIndex);
        dst[prevLightIndex] = curLightIndex;
    }
}

void RTGL1::LightManager::FillInfo(const RgSphericalLightUploadInfo &info, ShLightSpherical *dst)
{
    ShLightSpherical lt = {};
    memcpy(lt.color, info.color.data, sizeof(float) * 3);
    memcpy(lt.position, info.position.data, sizeof(float) * 3);
    lt.radius = std::max(0.0f, info.radius);
    lt.falloff = std::max(lt.radius, std::max(0.0f, info.falloffDistance));

    memcpy(dst, &lt, sizeof(ShLightSpherical));  
}

void RTGL1::LightManager::FillInfo(const RgDirectionalLightUploadInfo &info, ShLightDirectional *dst)
{
    ShLightDirectional lt = {};
    memcpy(lt.color, info.color.data, sizeof(float) * 3);
    lt.direction[0] = -info.direction.data[0];
    lt.direction[1] = -info.direction.data[1];
    lt.direction[2] = -info.direction.data[2];
    lt.tanAngularRadius = (float)tan(std::max(0.0, 0.5 * (double)info.angularDiameterDegrees) * RG_PI / 180.0);

    memcpy(dst, &lt, sizeof(ShLightDirectional));  
}

void RTGL1::LightManager::CreateDescriptors()
{
    VkResult r;

    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {};

    auto &bndSph = bindings[0];
    bndSph.binding = BINDING_LIGHT_SOURCES_SPHERICAL;
    bndSph.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bndSph.descriptorCount = 1;
    bndSph.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    auto &bndDir = bindings[1];
    bndDir.binding = BINDING_LIGHT_SOURCES_DIRECTIONAL;
    bndDir.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bndDir.descriptorCount = 1;
    bndDir.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    auto &bndMsp = bindings[2];
    bndMsp.binding = BINDING_LIGHT_SOURCES_SPH_MATCH_PREV;
    bndMsp.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bndMsp.descriptorCount = 1;
    bndMsp.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    auto &bndMdr = bindings[3];
    bndMdr.binding = BINDING_LIGHT_SOURCES_DIR_MATCH_PREV;
    bndMdr.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bndMdr.descriptorCount = 1;
    bndMdr.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Light buffers Desc set layout");

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = bindings.size() * MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Light buffers Desc set pool");

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descSetLayout;
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkAllocateDescriptorSets(device, &allocInfo, &descSets[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descSets[i], VK_OBJECT_TYPE_DESCRIPTOR_SET, "Light buffers Desc set");
    }
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateDescriptors(i);
    }
}

void RTGL1::LightManager::UpdateDescriptors(uint32_t frameIndex)
{
    VkDescriptorBufferInfo bfSphInfo = {};
    bfSphInfo.buffer = sphericalLights->GetDeviceLocal();
    bfSphInfo.offset = 0;
    bfSphInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo bfDirInfo = {};
    bfDirInfo.buffer = directionalLights->GetDeviceLocal();
    bfDirInfo.offset = 0;
    bfDirInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo bfMpSInfo = {};
    bfMpSInfo.buffer = sphericalLightMatchPrev->GetDeviceLocal();
    bfMpSInfo.offset = 0;
    bfMpSInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo bfMpDInfo = {};
    bfMpDInfo.buffer = directionalLightMatchPrev->GetDeviceLocal();
    bfMpDInfo.offset = 0;
    bfMpDInfo.range = VK_WHOLE_SIZE;

    std::array<VkWriteDescriptorSet, 4> wrts = {};

    auto &wrtSph = wrts[0];
    wrtSph.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wrtSph.dstSet = descSets[frameIndex];
    wrtSph.dstBinding = BINDING_LIGHT_SOURCES_SPHERICAL;
    wrtSph.dstArrayElement = 0;
    wrtSph.descriptorCount = 1;
    wrtSph.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wrtSph.pBufferInfo = &bfSphInfo;

    auto &wrtDir = wrts[1];
    wrtDir.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wrtDir.dstSet = descSets[frameIndex];
    wrtDir.dstBinding = BINDING_LIGHT_SOURCES_DIRECTIONAL;
    wrtDir.dstArrayElement = 0;
    wrtDir.descriptorCount = 1;
    wrtDir.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wrtDir.pBufferInfo = &bfDirInfo;

    auto &wrtMpS = wrts[2];
    wrtMpS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wrtMpS.dstSet = descSets[frameIndex];
    wrtMpS.dstBinding = BINDING_LIGHT_SOURCES_SPH_MATCH_PREV;
    wrtMpS.dstArrayElement = 0;
    wrtMpS.descriptorCount = 1;
    wrtMpS.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wrtMpS.pBufferInfo = &bfMpSInfo;

    auto &wrtMpD = wrts[3];
    wrtMpD.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wrtMpD.dstSet = descSets[frameIndex];
    wrtMpD.dstBinding = BINDING_LIGHT_SOURCES_DIR_MATCH_PREV;
    wrtMpD.dstArrayElement = 0;
    wrtMpD.descriptorCount = 1;
    wrtMpD.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wrtMpD.pBufferInfo = &bfMpDInfo;

    vkUpdateDescriptorSets(device, wrts.size(), wrts.data(), 0, nullptr);
}

static_assert(RTGL1::MAX_FRAMES_IN_FLIGHT == 2, "");