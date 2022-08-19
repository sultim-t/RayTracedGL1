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

constexpr uint32_t LIGHT_ARRAY_MAX_SIZE = 4096;

constexpr VkDeviceSize GRID_LIGHTS_COUNT =
    LIGHT_GRID_CELL_SIZE * (LIGHT_GRID_SIZE_X * LIGHT_GRID_SIZE_Y * LIGHT_GRID_SIZE_Z);

}

RTGL1::LightManager::LightManager(
    VkDevice _device, 
    std::shared_ptr<MemoryAllocator> &_allocator)
:
    device(_device),
    regLightCount(0),
    regLightCount_Prev(0),
    dirLightCount(0),
    dirLightCount_Prev(0),
    descSetLayout(VK_NULL_HANDLE),
    descPool(VK_NULL_HANDLE),
    descSets{},
    needDescSetUpdate{}
{
    lightsBuffer    = std::make_shared<AutoBuffer>(device, _allocator);
    lightsBuffer->Create(sizeof(ShLightEncoded) * LIGHT_ARRAY_MAX_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "Lights buffer");

    lightsBuffer_Prev.Init(_allocator, sizeof(ShLightEncoded) * LIGHT_ARRAY_MAX_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Lights buffer - prev");

    for (auto &buf : initialLightsGrid)
    {
        buf.Init(_allocator, sizeof(ShLightInCell) * GRID_LIGHTS_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Lights grid");
    }

    prevToCurIndex = std::make_shared<AutoBuffer>(device, _allocator);
    prevToCurIndex->Create(sizeof(uint32_t) * LIGHT_ARRAY_MAX_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Lights buffer - prev to cur");

    curToPrevIndex = std::make_shared<AutoBuffer>(device, _allocator);
    curToPrevIndex->Create(sizeof(uint32_t) * LIGHT_ARRAY_MAX_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Lights buffer - cur to prev");

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

    float angularRadius = static_cast<float>(0.5 * static_cast<double>(info.angularDiameterDegrees) * RTGL1::RG_PI / 180.0);


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

static RTGL1::ShLightEncoded EncodeAsSpotLight(const RgSpotLightUploadInfo &info)
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

static uint32_t GetLightArrayEnd(uint32_t regCount, uint32_t dirCount)
{
    // assuming that reg lights are always after directional ones
    return LIGHT_ARRAY_REGULAR_LIGHTS_OFFSET + regCount;
}

void RTGL1::LightManager::PrepareForFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
    regLightCount_Prev = regLightCount;
    dirLightCount_Prev = dirLightCount;

    regLightCount = 0;
    dirLightCount = 0;

    // TODO: similar system to just swap desc sets, instead of actual copying
    if (GetLightArrayEnd(regLightCount_Prev, dirLightCount_Prev) > 0)
    {
        VkBufferCopy info = {};
        info.srcOffset = 0;
        info.dstOffset = 0;
        info.size = GetLightArrayEnd(regLightCount_Prev, dirLightCount_Prev) * sizeof(ShLightEncoded);

        vkCmdCopyBuffer(
            cmd,
            lightsBuffer->GetDeviceLocal(), lightsBuffer_Prev.GetBuffer(),
            1, &info);
    }

    memset(prevToCurIndex->GetMapped(frameIndex), 0xFF, sizeof(uint32_t) * GetLightArrayEnd(regLightCount_Prev, dirLightCount_Prev));
    // no need to clear curToPrevIndex, as it'll be filled in the cur frame

    uniqueIDToArrayIndex[frameIndex].clear();
}

void RTGL1::LightManager::Reset()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        memset(prevToCurIndex->GetMapped(i), 0xFF, sizeof(uint32_t) * std::max(GetLightArrayEnd(regLightCount, dirLightCount), GetLightArrayEnd(regLightCount_Prev, dirLightCount_Prev)));
        memset(curToPrevIndex->GetMapped(i), 0xFF, sizeof(uint32_t) * std::max(GetLightArrayEnd(regLightCount, dirLightCount), GetLightArrayEnd(regLightCount_Prev, dirLightCount_Prev)));

        uniqueIDToArrayIndex[i].clear();
    }

    regLightCount_Prev = regLightCount = 0;
    dirLightCount_Prev = dirLightCount = 0;
}

static bool IsColorTooDim(const float c[3])
{
    return
        std::max(c[0], 0.0f) +
        std::max(c[1], 0.0f) + 
        std::max(c[2], 0.0f) < RTGL1::MIN_COLOR_SUM;
}

RTGL1::LightArrayIndex RTGL1::LightManager::GetIndex(const RTGL1::ShLightEncoded &encodedLight) const
{
    switch (encodedLight.lightType)
    {
    case LIGHT_TYPE_DIRECTIONAL:
        return LightArrayIndex{ LIGHT_ARRAY_DIRECTIONAL_LIGHT_OFFSET + dirLightCount };
    case LIGHT_TYPE_SPHERE:
    case LIGHT_TYPE_TRIANGLE:
    case LIGHT_TYPE_SPOT:
        return LightArrayIndex{ LIGHT_ARRAY_REGULAR_LIGHTS_OFFSET + regLightCount };
    default:
        assert(0);
        return LightArrayIndex{ 0 };
    }
}

void RTGL1::LightManager::IncrementCount(const ShLightEncoded& encodedLight)
{
    switch (encodedLight.lightType)
    {
        case LIGHT_TYPE_DIRECTIONAL:
            dirLightCount++;
            break;
        case LIGHT_TYPE_SPHERE:
        case LIGHT_TYPE_TRIANGLE:
        case LIGHT_TYPE_SPOT:
            regLightCount++;
            break;
        default:
            assert(0);
    }
}

void RTGL1::LightManager::AddLight(uint32_t frameIndex, uint64_t uniqueId, const RTGL1::ShLightEncoded &encodedLight)
{
    if (GetLightArrayEnd(regLightCount, dirLightCount) >= LIGHT_ARRAY_MAX_SIZE)
    {
        assert(0);
        return;
    }

    const LightArrayIndex index = GetIndex(encodedLight);
    IncrementCount(encodedLight);

    auto *dst = (ShLightEncoded *)lightsBuffer->GetMapped(frameIndex);
    memcpy(&dst[index.GetArrayIndex()], &encodedLight, sizeof(RTGL1::ShLightEncoded));


    FillMatchPrev(frameIndex, index, uniqueId);
    // must be unique
    assert(uniqueIDToArrayIndex[frameIndex].find(uniqueId) == uniqueIDToArrayIndex[frameIndex].end());
    // save index for the next frame
    uniqueIDToArrayIndex[frameIndex][uniqueId] = index;
}

void RTGL1::LightManager::AddSphericalLight(uint32_t frameIndex, const RgSphericalLightUploadInfo &info)
{
    if (IsColorTooDim(info.color.data))
    {
        return;
    }

    AddLight(frameIndex, info.uniqueID, EncodeAsSphereLight(info));
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

    AddLight(frameIndex, info.uniqueID, EncodeAsTriangleLight(info, unnormalizedNormal));
}

void RTGL1::LightManager::AddSpotlight(uint32_t frameIndex, const RgSpotLightUploadInfo &info)
{
    if (IsColorTooDim(info.color.data) || info.radius < 0.0f || info.angleOuter <= 0.0f)
    {
        return;
    }

    AddLight(frameIndex, info.uniqueID, EncodeAsSpotLight(info));
}

void RTGL1::LightManager::AddDirectionalLight(uint32_t frameIndex, const RgDirectionalLightUploadInfo &info)
{
    if (dirLightCount > 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Only one directional light is allowed");
    }

    if (IsColorTooDim(info.color.data) || info.angularDiameterDegrees < 0.0f)
    {
        return;
    }
    
    AddLight(frameIndex, info.uniqueID, EncodeAsDirectionalLight(info));
}

void RTGL1::LightManager::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex)
{
    CmdLabel label(cmd, "Copying lights");

    lightsBuffer->CopyFromStaging(cmd, frameIndex, sizeof(ShLightEncoded) * GetLightArrayEnd(regLightCount, dirLightCount));

    prevToCurIndex->CopyFromStaging(cmd, frameIndex, sizeof(uint32_t) * GetLightArrayEnd(regLightCount_Prev, dirLightCount_Prev));
    curToPrevIndex->CopyFromStaging(cmd, frameIndex, sizeof(uint32_t) * GetLightArrayEnd(regLightCount, dirLightCount));

    // should be used when buffers changed
    if (needDescSetUpdate[frameIndex])
    {
        UpdateDescriptors(frameIndex);
        needDescSetUpdate[frameIndex] = false;
    }
}

void RTGL1::LightManager::BarrierLightGrid(VkCommandBuffer cmd, uint32_t frameIndex)
{
    VkBufferMemoryBarrier2 barrier =
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        .srcQueueFamilyIndex = 0,
        .dstQueueFamilyIndex = 0,
        .buffer = initialLightsGrid[frameIndex].GetBuffer(),
        .offset = 0,
        .size = VK_WHOLE_SIZE
    };

    VkDependencyInfo dependency =
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier
    };

    svkCmdPipelineBarrier2KHR(cmd, &dependency);
}

VkDescriptorSetLayout RTGL1::LightManager::GetDescSetLayout()
{
    return descSetLayout;
}

VkDescriptorSet RTGL1::LightManager::GetDescSet(uint32_t frameIndex)
{
    return descSets[frameIndex];
}

void RTGL1::LightManager::FillMatchPrev(uint32_t curFrameIndex, LightArrayIndex lightIndexInCurFrame, UniqueLightID uniqueID)
{
    uint32_t prevFrame = (curFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    const rgl::unordered_map<UniqueLightID, LightArrayIndex> &uniqueToPrevIndex = uniqueIDToArrayIndex[prevFrame];

    auto found = uniqueToPrevIndex.find(uniqueID);
    if (found == uniqueToPrevIndex.end())
    {
        return;
    }

    LightArrayIndex lightIndexInPrevFrame = found->second;

    uint32_t *prev2cur = static_cast<uint32_t *>(prevToCurIndex->GetMapped(curFrameIndex));
    prev2cur[lightIndexInPrevFrame.GetArrayIndex()] = lightIndexInCurFrame.GetArrayIndex();

    uint32_t *cur2prev = static_cast<uint32_t *>(curToPrevIndex->GetMapped(curFrameIndex));
    cur2prev[lightIndexInCurFrame.GetArrayIndex()] = lightIndexInPrevFrame.GetArrayIndex();
}

constexpr uint32_t BINDINGS[] =
{
    BINDING_LIGHT_SOURCES,
    BINDING_LIGHT_SOURCES_PREV,
    BINDING_LIGHT_SOURCES_INDEX_PREV_TO_CUR,
    BINDING_LIGHT_SOURCES_INDEX_CUR_TO_PREV,
    BINDING_INITIAL_LIGHTS_GRID,
    BINDING_INITIAL_LIGHTS_GRID_PREV,
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
        b.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;
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
        prevToCurIndex->GetDeviceLocal(),
        curToPrevIndex->GetDeviceLocal(),
        initialLightsGrid[frameIndex].GetBuffer(),
        initialLightsGrid[Utils::GetPreviousByModulo(frameIndex, MAX_FRAMES_IN_FLIGHT)].GetBuffer(),
    };
    static_assert(std::size(BINDINGS) == std::size(buffers));

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
    return regLightCount;
}

uint32_t RTGL1::LightManager::GetLightCountPrev() const
{
    return regLightCount_Prev;
}


uint32_t RTGL1::LightManager::DoesDirectionalLightExist() const
{
    return dirLightCount > 0 ? 1 : 0;
}

uint32_t RTGL1::LightManager::GetLightIndexIgnoreFPVShadows(uint32_t frameIndex, uint64_t *pLightUniqueId) const
{
    if (pLightUniqueId == nullptr)
    {
        return LIGHT_INDEX_NONE;
    }
    UniqueLightID uniqueId = { *pLightUniqueId };

    const auto f = uniqueIDToArrayIndex[frameIndex].find(uniqueId);
    if (f == uniqueIDToArrayIndex[frameIndex].end())
    {
        return LIGHT_INDEX_NONE;
    }

    return f->second.GetArrayIndex();
}

static_assert(RTGL1::MAX_FRAMES_IN_FLIGHT == 2);
