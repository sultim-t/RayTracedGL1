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
#include "CmdLabel.h"
#include "RgException.h"
#include "Utils.h"

namespace RTGL1
{
constexpr double RG_PI = 3.1415926535897932384626433;

constexpr float MIN_COLOR_SUM = 0.0001f;
constexpr float MIN_SPHERE_RADIUS = 0.005f;

constexpr uint32_t MAX_LIGHT_COUNT = 4096;
constexpr uint32_t MAX_LIGHT_COUNT_DIRECTIONAL = 1;
}

RTGL1::LightManager::LightManager(
    VkDevice _device, 
    std::shared_ptr<MemoryAllocator> &_allocator, 
    std::shared_ptr<SectorVisibility> &_sectorVisibility)
:
    device(_device),
    allLightCount(0),
    allLightCount_Prev(0),
    dirLightSingleton{},
    descSetLayout(VK_NULL_HANDLE),
    descPool(VK_NULL_HANDLE),
    descSets{},
    needDescSetUpdate{}
{
    lightLists      = std::make_shared<LightLists>(device, _allocator, _sectorVisibility);

    lightsBuffer    = std::make_shared<AutoBuffer>(device, _allocator);
    lightsBuffer->Create(sizeof(ShLightEncoded) * MAX_LIGHT_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "Lights buffer");

    lightsBuffer_Prev.Init(_allocator, sizeof(ShLightEncoded) * MAX_LIGHT_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Lights buffer - prev");

    matchPrev = std::make_shared<AutoBuffer>(device, _allocator);
    matchPrev->Create(sizeof(uint32_t) * MAX_LIGHT_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Lights buffer - match previous");

    CreateDescriptors();
}

RTGL1::LightManager::~LightManager()
{
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);
}

static RTGL1::ShLightEncoded EncodeAsDirectionalLight(const RgDirectionalLightUploadInfo &info)
{
    RgFloat3D direction = info.direction;
    RTGL1::Utils::Normalize(direction.data);

    float angularRadius = static_cast<float>(0.5 * static_cast<double>(info.angularDiameterDegrees) * RTGL1::RG_PI / 180.0);;


    RTGL1::ShLightEncoded lt = {};
    lt.lightType = LIGHT_TYPE_DIRECTIONAL;

    lt.color[0] = info.color.data[0];
    lt.color[1] = info.color.data[1];
    lt.color[2] = info.color.data[2];

    lt.data_0[0] = direction.data[0];
    lt.data_0[1] = direction.data[1];
    lt.data_0[2] = direction.data[2];

    lt.data_0[3] = angularRadius;

    return lt;
}

static RTGL1::ShLightEncoded EncodeAsSphereLight(const RgSphericalLightUploadInfo &info)
{
    float radius = std::max(RTGL1::MIN_SPHERE_RADIUS, info.radius);
    // disk is visible from the point
    float area = static_cast<float>(RTGL1::RG_PI) * radius * radius;


    RTGL1::ShLightEncoded lt = {};
    lt.lightType = LIGHT_TYPE_SPHERE;

    lt.color[0] = info.color.data[0] / area;
    lt.color[1] = info.color.data[1] / area;
    lt.color[2] = info.color.data[2] / area;

    lt.data_0[0] = info.position.data[0];
    lt.data_0[1] = info.position.data[1];
    lt.data_0[2] = info.position.data[2];

    lt.data_0[3] = radius;

    return lt;
}

static RTGL1::ShLightEncoded EncodeAsTriangleLight(const RgPolygonalLightUploadInfo &info, const RgFloat3D &unnormalizedNormal)
{
    RgFloat3D n = unnormalizedNormal;
    float len = RTGL1::Utils::Length(n.data);
    n.data[0] /= len;
    n.data[1] /= len;
    n.data[2] /= len;

    float area = len * 0.5f;
    assert(area > 0.0f);


    RTGL1::ShLightEncoded lt = {};
    lt.lightType = LIGHT_TYPE_TRIANGLE;

    lt.color[0] = info.color.data[0] / area;
    lt.color[1] = info.color.data[1] / area;
    lt.color[2] = info.color.data[2] / area;

    lt.data_0[0] = info.positions[0].data[0];
    lt.data_0[1] = info.positions[0].data[1];
    lt.data_0[2] = info.positions[0].data[2];

    lt.data_1[0] = info.positions[1].data[0];
    lt.data_1[1] = info.positions[1].data[1];
    lt.data_1[2] = info.positions[1].data[2];

    lt.data_2[0] = info.positions[2].data[0];
    lt.data_2[1] = info.positions[2].data[1];
    lt.data_2[2] = info.positions[2].data[2];

    lt.data_0[3] = unnormalizedNormal.data[0];
    lt.data_1[3] = unnormalizedNormal.data[1];
    lt.data_2[3] = unnormalizedNormal.data[2];

    return lt;
}

static RTGL1::ShLightEncoded EncodeAsSpotLight(const RgSpotlightUploadInfo &info)
{
    RgFloat3D direction = info.direction;
    RTGL1::Utils::Normalize(direction.data);

    float radius = std::max(RTGL1::MIN_SPHERE_RADIUS, info.radius);
    float area = static_cast<float>(RTGL1::RG_PI) * radius * radius;

    float cosAngleInner = std::cos(std::min(info.angleInner, info.angleOuter));
    float cosAngleOuter = std::cos(info.angleOuter);


    RTGL1::ShLightEncoded lt = {};
    lt.lightType = LIGHT_TYPE_SPOT;

    lt.color[0] = info.color.data[0] / area;
    lt.color[1] = info.color.data[1] / area;
    lt.color[2] = info.color.data[2] / area;

    lt.data_0[0] = info.position.data[0];
    lt.data_0[1] = info.position.data[1];
    lt.data_0[2] = info.position.data[2];

    lt.data_0[3] = radius;

    lt.data_1[0] = direction.data[0];
    lt.data_1[1] = direction.data[1];
    lt.data_1[2] = direction.data[2];

    lt.data_2[0] = cosAngleInner;
    lt.data_2[1] = cosAngleOuter;

    return lt;
}

void RTGL1::LightManager::PrepareForFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
    allLightCount_Prev = allLightCount;

    allLightCount = 0;
    dirLightSingleton.dirLightCount = 0;

    // TODO: similar system to just swap desc sets, instead of actual copying
    if (allLightCount_Prev > 0)
    {
        VkBufferCopy info = {};
        info.srcOffset = 0;
        info.dstOffset = 0;
        info.size = allLightCount_Prev * sizeof(ShLightEncoded);

        vkCmdCopyBuffer(
            cmd,
            lightsBuffer->GetDeviceLocal(), lightsBuffer_Prev.GetBuffer(),
            1, &info);
    }

    memset(matchPrev->GetMapped(frameIndex), 0xFF, sizeof(uint32_t) * allLightCount_Prev);

    uniqueIDToPrevIndex[frameIndex].clear();

    lightLists->PrepareForFrame();
}

void RTGL1::LightManager::Reset()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        memset(matchPrev->GetMapped(i), 0xFF, sizeof(uint32_t) * std::max(allLightCount, allLightCount_Prev));

        uniqueIDToPrevIndex[i].clear();
    }

    allLightCount = allLightCount_Prev = 0;
    dirLightSingleton.dirLightCount = 0;

    lightLists->Reset();
}

static bool IsColorTooDim(const float c[3])
{
    return std::abs(c[0]) + std::abs(c[1]) + std::abs(c[2]) < RTGL1::MIN_COLOR_SUM;
}

void RTGL1::LightManager::AddLight(uint32_t frameIndex, uint64_t uniqueId, const SectorID sectorId, const RTGL1::ShLightEncoded &encodedLight)
{
    if (allLightCount >= MAX_LIGHT_COUNT)
    {
        assert(0);
        return;
    }

    const LightArrayIndex index = LightArrayIndex{ allLightCount };
    allLightCount++;


    auto *dst = (ShLightEncoded *)lightsBuffer->GetMapped(frameIndex);
    memcpy(&dst[index.GetArrayIndex()], &encodedLight, sizeof(RTGL1::ShLightEncoded));


    FillMatchPrev(uniqueIDToPrevIndex, matchPrev, frameIndex, index, uniqueId);
    // must be unique
    assert(uniqueIDToPrevIndex[frameIndex].find(uniqueId) == uniqueIDToPrevIndex[frameIndex].end());
    // save index for the next frame
    uniqueIDToPrevIndex[frameIndex][uniqueId] = index;


    lightLists->InsertLight(index, lightLists->SectorIDToArrayIndex(sectorId));
}

void RTGL1::LightManager::AddSphericalLight(uint32_t frameIndex, const RgSphericalLightUploadInfo &info)
{
    if (IsColorTooDim(info.color.data))
    {
        return;
    }

    AddLight(frameIndex, info.uniqueID, SectorID{ info.sectorID }, EncodeAsSphereLight(info));
}

void RTGL1::LightManager::AddPolygonalLight(uint32_t frameIndex, const RgPolygonalLightUploadInfo &info)
{
    if (IsColorTooDim(info.color.data))
    {
        return;
    }

    RgFloat3D unnormalizedNormal = Utils::GetUnnormalizedNormal(info.positions);
    if (Utils::Dot(unnormalizedNormal.data, unnormalizedNormal.data) <= 0.0f)
    {
        return;
    }

    AddLight(frameIndex, info.uniqueID, SectorID{ info.sectorID }, EncodeAsTriangleLight(info, unnormalizedNormal));
}

void RTGL1::LightManager::AddSpotlight(uint32_t frameIndex, const RgSpotlightUploadInfo &info)
{
    if (IsColorTooDim(info.color.data) || info.radius < 0.0f || info.angleOuter <= 0.0f)
    {
        return;
    }

    AddLight(frameIndex, info.uniqueID, SectorID{ info.sectorID }, EncodeAsSpotLight(info));
}

void RTGL1::LightManager::AddDirectionalLight(uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, const RgDirectionalLightUploadInfo &info)
{
    if (IsColorTooDim(info.color.data))
    {
        return;
    }

    if (dirLightSingleton.dirLightCount >= MAX_LIGHT_COUNT_DIRECTIONAL)
    {
        assert(0);
        throw RgException(RG_WRONG_ARGUMENT, "Only one directional light can be added");
    }
    
    auto enc = EncodeAsDirectionalLight(info);
    // TODO: move from uniform to the lightBuffer
    {
        memcpy(uniform->GetData()->directionalLight_color, enc.color, 3 * sizeof(float));
        memcpy(uniform->GetData()->directionalLight_data_0, enc.data_0, 4 * sizeof(float));
    }
    dirLightSingleton.dirLightCount++;
}

void RTGL1::LightManager::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex)
{
    CmdLabel label(cmd, "Copying lights");

    lightsBuffer->CopyFromStaging(cmd, frameIndex, sizeof(ShLightEncoded) * allLightCount);

    matchPrev->CopyFromStaging(cmd, frameIndex, sizeof(uint32_t) * allLightCount_Prev);

    lightLists->BuildAndCopyFromStaging(cmd, frameIndex);

    // should be used when buffers changed
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
    const rgl::unordered_map<UniqueLightID, LightArrayIndex> *pUniqueToPrevIndex,
    const std::shared_ptr<AutoBuffer> &matchPrev,
    uint32_t curFrameIndex, LightArrayIndex lightIndexInCurFrame, UniqueLightID uniqueID)
{
    uint32_t prevFrame = (curFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    const rgl::unordered_map<UniqueLightID, LightArrayIndex> &uniqueToPrevIndex = pUniqueToPrevIndex[prevFrame];

    auto found = uniqueToPrevIndex.find(uniqueID);
    if (found == uniqueToPrevIndex.end())
    {
        return;
    }

    LightArrayIndex lightIndexInPrevFrame = found->second;

    uint32_t *dst = (uint32_t*)matchPrev->GetMapped(curFrameIndex);
    dst[lightIndexInPrevFrame.GetArrayIndex()] = lightIndexInCurFrame.GetArrayIndex();
}

constexpr uint32_t BINDINGS[] =
{
    BINDING_LIGHT_SOURCES,
    BINDING_LIGHT_SOURCES_PREV,
    BINDING_LIGHT_SOURCES_MATCH_PREV,
    BINDING_PLAIN_LIGHT_LIST,
    BINDING_SECTOR_TO_LIGHT_LIST_REGION,
};

void RTGL1::LightManager::CreateDescriptors()
{
    VkResult r;
    
    std::array<VkDescriptorSetLayoutBinding, std::size(BINDINGS)> bindings = {};

    for (uint32_t i = 0; i < std::size(BINDINGS); i++)
    {
        uint32_t bnd = BINDINGS[i];
        assert(i == bnd);

        VkDescriptorSetLayoutBinding &b = bindings[bnd];
        b.binding = bnd;
        b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    }

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
    const VkBuffer buffers[] =
    {
        lightsBuffer->GetDeviceLocal(),
        lightsBuffer_Prev.GetBuffer(),
        matchPrev->GetDeviceLocal(),
        lightLists->GetPlainLightListDeviceLocalBuffer(),
        lightLists->GetSectorToLightListRegionDeviceLocalBuffer(),
    };
    static_assert(std::size(BINDINGS) == std::size(buffers), "");

    std::array<VkDescriptorBufferInfo, std::size(BINDINGS)> bufs = {};
    std::array<VkWriteDescriptorSet, std::size(BINDINGS)> wrts = {};

    for (uint32_t i = 0; i < std::size(BINDINGS); i++)
    {
        uint32_t bnd = BINDINGS[i];
        // 'buffers' should be actually a map (binding->buffer), but a plain array works too, if this is true
        assert(i == bnd);

        VkDescriptorBufferInfo &b = bufs[bnd];
        b.buffer = buffers[bnd];
        b.offset = 0;
        b.range = VK_WHOLE_SIZE;
        
        VkWriteDescriptorSet &w = wrts[bnd];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descSets[frameIndex];
        w.dstBinding = bnd;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &b;
    }

    vkUpdateDescriptorSets(device, wrts.size(), wrts.data(), 0, nullptr);
}

uint32_t RTGL1::LightManager::GetLightCount() const
{
    return allLightCount;
}

uint32_t RTGL1::LightManager::GetLightCountPrev() const
{
    return allLightCount_Prev;
}


uint32_t RTGL1::LightManager::GetDirectionalLightCount() const
{
    return dirLightSingleton.dirLightCount;
}

static_assert(RTGL1::MAX_FRAMES_IN_FLIGHT == 2, "");