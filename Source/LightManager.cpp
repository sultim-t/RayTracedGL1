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

namespace RTGL1
{
constexpr double RG_PI = 3.1415926535897932384626433;

constexpr float MIN_COLOR_SUM = 0.0001f;

constexpr uint32_t MAX_LIGHT_COUNT_SPHERICAL = 1024;
constexpr uint32_t MAX_LIGHT_COUNT_DIRECTIONAL = 1;
constexpr uint32_t MAX_LIGHT_COUNT_SPOT = 1;
constexpr uint32_t MAX_LIGHT_COUNT_POLYGONAL = 1024;
}

RTGL1::LightManager::LightManager(
    VkDevice _device, 
    std::shared_ptr<MemoryAllocator> &_allocator, 
    std::shared_ptr<SectorVisibility> &_sectorVisibility)
:
    device(_device),
    sphLightCount(0),
    sphLightCountPrev(0),
    dirLightCount(0),
    dirLightCountPrev(0),
    spotLightCount(0),
    spotLightCountPrev(0),
    polyLightCount(0),
    polyLightCountPrev(0),
    descSetLayout(VK_NULL_HANDLE),
    descPool(VK_NULL_HANDLE),
    descSets{},
    needDescSetUpdate{}
{
    lightListsForPolygonal  = std::make_shared<LightLists>(device, _allocator, _sectorVisibility, "polygonal");
    lightListsForSpherical  = std::make_shared<LightLists>(device, _allocator, _sectorVisibility, "spherical");

    sphericalLights         = std::make_shared<AutoBuffer>(device, _allocator, "Lights spherical staging", "Lights spherical");
    polygonalLights         = std::make_shared<AutoBuffer>(device, _allocator, "Lights polugonal staging", "Lights polygonal");
    sphericalLightMatchPrev = std::make_shared<AutoBuffer>(device, _allocator, "Match previous Lights spherical staging", "Match previous Lights spherical");
    polygonalLightMatchPrev = std::make_shared<AutoBuffer>(device, _allocator, "Match previous Lights polygonal staging", "Match previous Lights polygonal");

    sphericalLights->Create(sizeof(ShLightSpherical) * MAX_LIGHT_COUNT_SPHERICAL, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    polygonalLights->Create(sizeof(ShLightPolygonal) * MAX_LIGHT_COUNT_POLYGONAL, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    sphericalLightMatchPrev->Create(sizeof(uint32_t) * MAX_LIGHT_COUNT_SPHERICAL, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    polygonalLightMatchPrev->Create(sizeof(uint32_t) * MAX_LIGHT_COUNT_POLYGONAL, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    CreateDescriptors();
}

RTGL1::LightManager::~LightManager()
{
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);
}

static void FillInfoSpherical(const RgSphericalLightUploadInfo &info, RTGL1::ShLightSpherical *dst)
{
    RTGL1::ShLightSpherical lt = {};

    memcpy(lt.color, info.color.data, sizeof(float) * 3);
    memcpy(lt.position, info.position.data, sizeof(float) * 3);

    lt.radius = std::max(0.0f, info.radius);
    lt.falloff = std::max(lt.radius, std::max(0.0f, info.falloffDistance));

    memcpy(dst, &lt, sizeof(RTGL1::ShLightSpherical));
}

static void FillInfoPolygonal(const RgPolygonalLightUploadInfo &info, RTGL1::ShLightPolygonal *dst)
{
    RTGL1::ShLightPolygonal lt = {};

    memcpy(lt.position_0, info.positions[0].data, sizeof(float) * 3);
    memcpy(lt.position_1, info.positions[1].data, sizeof(float) * 3);
    memcpy(lt.position_2, info.positions[2].data, sizeof(float) * 3);

    memcpy(lt.color, info.color.data, sizeof(float) * 3);

    memcpy(dst, &lt, sizeof(RTGL1::ShLightPolygonal));
}

static void FillInfoDirectional(const RgDirectionalLightUploadInfo &info, RTGL1::ShGlobalUniform *dst)
{
    memcpy(dst->directionalLightColor, info.color.data, sizeof(float) * 3);
    dst->directionalLightColor[3] = 0.0f;

    dst->directionalLightDirection[0] = -info.direction.data[0];
    dst->directionalLightDirection[1] = -info.direction.data[1];
    dst->directionalLightDirection[2] = -info.direction.data[2];
    dst->directionalLightDirection[3] = 0.0f;

    dst->directionalLightTanAngularRadius = (float)tan(std::max(0.0, 0.5 * (double)info.angularDiameterDegrees) * RTGL1::RG_PI / 180.0);
}

static void ResetInfoDirectional(RTGL1::ShGlobalUniform *gu)
{
    memset(gu->directionalLightColor, 0, sizeof(gu->directionalLightColor));
    memset(gu->directionalLightDirection, 0, sizeof(gu->directionalLightDirection));

    gu->directionalLightTanAngularRadius = 0.0f;
}

static void FillInfoSpotlight(const RgSpotlightUploadInfo &info, RTGL1::ShGlobalUniform *gu)
{
    // use global uniform buffer for one spotlight instance
    memcpy(gu->spotlightPosition, info.position.data, 3 * sizeof(float));
    memcpy(gu->spotlightDirection, info.direction.data, 3 * sizeof(float));
    memcpy(gu->spotlightUpVector, info.upVector.data, 3 * sizeof(float));
    memcpy(gu->spotlightColor, info.color.data, 3 * sizeof(float));

    gu->spotlightRadius = info.radius;
    gu->spotlightCosAngleOuter = std::cos(info.angleOuter);
    gu->spotlightCosAngleInner = std::cos(info.angleInner);
    gu->spotlightFalloffDistance = info.falloffDistance;

    gu->spotlightCosAngleInner = std::max(gu->spotlightCosAngleOuter, gu->spotlightCosAngleInner);

}

static void ResetInfoSpotlight(RTGL1::ShGlobalUniform *gu)
{
    memset(gu->spotlightPosition, 0, sizeof(gu->spotlightPosition));
    memset(gu->spotlightDirection, 0, sizeof(gu->spotlightDirection));
    memset(gu->spotlightUpVector, 0, sizeof(gu->spotlightUpVector));
    memset(gu->spotlightColor, 0, sizeof(gu->spotlightColor));

    gu->spotlightRadius = -1;
    gu->spotlightCosAngleOuter = -1;
    gu->spotlightCosAngleInner = -1;
    gu->spotlightFalloffDistance = -1;
}

void RTGL1::LightManager::PrepareForFrame(uint32_t frameIndex)
{
    sphLightCountPrev = sphLightCount;
    dirLightCountPrev = dirLightCount;
    spotLightCountPrev = spotLightCount;
    polyLightCountPrev = polyLightCount;

    sphLightCount = 0;
    dirLightCount = 0;
    spotLightCount = 0;
    polyLightCount = 0;

    memset(sphericalLightMatchPrev->GetMapped(frameIndex), 0xFF, sizeof(uint32_t) * sphLightCountPrev);
    memset(polygonalLightMatchPrev->GetMapped(frameIndex), 0xFF, sizeof(uint32_t) * polyLightCountPrev);

    sphericalUniqueIDToPrevIndex[frameIndex].clear();
    polygonalUniqueIDToPrevIndex[frameIndex].clear();

    lightListsForSpherical->PrepareForFrame();
    lightListsForPolygonal->PrepareForFrame();
}

void RTGL1::LightManager::Reset()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        memset(sphericalLightMatchPrev->GetMapped(i), 0xFF, sizeof(uint32_t) * std::max(sphLightCount, sphLightCountPrev));
        memset(polygonalLightMatchPrev->GetMapped(i), 0xFF, sizeof(uint32_t) * std::max(polyLightCount, polyLightCountPrev));

        sphericalUniqueIDToPrevIndex[i].clear();
        polygonalUniqueIDToPrevIndex[i].clear();
    }

    sphLightCount = sphLightCountPrev = 0;
    dirLightCount = dirLightCountPrev = 0;
    spotLightCount = spotLightCountPrev = 0;
    polyLightCount = polyLightCountPrev = 0;

    lightListsForSpherical->Reset();
    lightListsForPolygonal->Reset();
}

static bool IsColorTooDim(const RgFloat3D &c)
{
    return c.data[0] + c.data[1] + c.data[2] < RTGL1::MIN_COLOR_SUM;
}

void RTGL1::LightManager::AddSphericalLight(uint32_t frameIndex, const RgSphericalLightUploadInfo &info)
{
    if (IsColorTooDim(info.color))
    {
        return;
    }

    if (sphLightCount >= MAX_LIGHT_COUNT_SPHERICAL)
    {
        assert(0);
        return;
    }


    const SectorID sectorId = SectorID{ info.sectorID };
    const SectorArrayIndex sectorArrayIndex = lightListsForSpherical->SectorIDToArrayIndex(sectorId);


    const LightArrayIndex index = LightArrayIndex{ sphLightCount };
    sphLightCount++;

    auto *dst = (ShLightSpherical*)sphericalLights->GetMapped(frameIndex);
    FillInfoSpherical(info, &dst[index.GetArrayIndex()]);

    FillMatchPrev(sphericalUniqueIDToPrevIndex, sphericalLightMatchPrev, frameIndex, index, info.uniqueID);

    // must be unique
    assert(sphericalUniqueIDToPrevIndex[frameIndex].find(info.uniqueID) == sphericalUniqueIDToPrevIndex[frameIndex].end());

    // save index for the next frame
    sphericalUniqueIDToPrevIndex[frameIndex][info.uniqueID] = index;


    lightListsForSpherical->InsertLight(index, sectorArrayIndex,
                                        nullptr, nullptr);
}

void RTGL1::LightManager::AddPolygonalLight(uint32_t frameIndex, const RgPolygonalLightUploadInfo &info)
{
    if (IsColorTooDim(info.color))
    {
        return;
    }

    if (polyLightCount >= MAX_LIGHT_COUNT_POLYGONAL)
    {
        assert(0);
        return;
    }


    const SectorID sectorId = SectorID{ info.sectorID };
    const SectorArrayIndex sectorArrayIndex = lightListsForPolygonal->SectorIDToArrayIndex(sectorId);


    const LightArrayIndex index = LightArrayIndex{ polyLightCount };
    polyLightCount++;

    auto *dst = (ShLightPolygonal *)polygonalLights->GetMapped(frameIndex);
    FillInfoPolygonal(info, &dst[index.GetArrayIndex()]);

    FillMatchPrev(polygonalUniqueIDToPrevIndex, polygonalLightMatchPrev, frameIndex, index, info.uniqueID);

    // must be unique
    assert(polygonalUniqueIDToPrevIndex[frameIndex].find(info.uniqueID) == polygonalUniqueIDToPrevIndex[frameIndex].end());

    // save index for the next frame
    polygonalUniqueIDToPrevIndex[frameIndex][info.uniqueID] = index;


    lightListsForPolygonal->InsertLight(index, sectorArrayIndex,
                                        info.pfnIsLightVisibleFromSector, info.pUserDataForPfn);
}

void RTGL1::LightManager::AddSpotlight(uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, const RgSpotlightUploadInfo &info)
{
    if (IsColorTooDim(info.color) ||
        info.radius <= 0.0f ||
        info.falloffDistance <= 0.0f ||
        info.angleOuter <= 0.0f)
    {
        return;
    }

    if (spotLightCount >= MAX_LIGHT_COUNT_SPOT)
    {
        assert(0);
        throw RgException(RG_WRONG_ARGUMENT, "Only one spotlight can be added");
    }

    FillInfoSpotlight(info, uniform->GetData());
    spotLightCount++;
}

void RTGL1::LightManager::AddDirectionalLight(uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, const RgDirectionalLightUploadInfo &info)
{
    if (IsColorTooDim(info.color))
    {
        return;
    }

    if (dirLightCount >= MAX_LIGHT_COUNT_DIRECTIONAL)
    {
        assert(0);
        throw RgException(RG_WRONG_ARGUMENT, "Only one directional light can be added");
    }
    
    FillInfoDirectional(info, uniform->GetData());
    dirLightCount++;
}

void RTGL1::LightManager::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex)
{
    CmdLabel label(cmd, "Copying lights");

    sphericalLights->CopyFromStaging(cmd, frameIndex, sizeof(ShLightSpherical) * sphLightCount);
    polygonalLights->CopyFromStaging(cmd, frameIndex, sizeof(ShLightPolygonal) * polyLightCount);

    sphericalLightMatchPrev->CopyFromStaging(cmd, frameIndex, sizeof(uint32_t) * sphLightCountPrev);
    polygonalLightMatchPrev->CopyFromStaging(cmd, frameIndex, sizeof(uint32_t) * polyLightCountPrev);

    lightListsForSpherical->BuildAndCopyFromStaging(cmd, frameIndex);
    lightListsForPolygonal->BuildAndCopyFromStaging(cmd, frameIndex);

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
    const std::unordered_map<UniqueLightID, LightArrayIndex> *pUniqueToPrevIndex,
    const std::shared_ptr<AutoBuffer> &matchPrev,
    uint32_t curFrameIndex, LightArrayIndex lightIndexInCurFrame, UniqueLightID uniqueID)
{
    uint32_t prevFrame = (curFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    const std::unordered_map<UniqueLightID, LightArrayIndex> &uniqueToPrevIndex = pUniqueToPrevIndex[prevFrame];

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
    BINDING_LIGHT_SOURCES_SPHERICAL,
    BINDING_LIGHT_SOURCES_POLYGONAL,
    BINDING_LIGHT_SOURCES_SPH_MATCH_PREV,
    BINDING_LIGHT_SOURCES_POLY_MATCH_PREV,
    BINDING_PLAIN_LIGHT_LIST_POLY,
    BINDING_SECTOR_TO_LIGHT_LIST_REGION_POLY,
    BINDING_PLAIN_LIGHT_LIST_SPH,
    BINDING_SECTOR_TO_LIGHT_LIST_REGION_SPH,
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
        sphericalLights->GetDeviceLocal(),
        polygonalLights->GetDeviceLocal(),
        sphericalLightMatchPrev->GetDeviceLocal(),
        polygonalLightMatchPrev->GetDeviceLocal(),
        lightListsForPolygonal->GetPlainLightListDeviceLocalBuffer(),
        lightListsForPolygonal->GetSectorToLightListRegionDeviceLocalBuffer(),
        lightListsForSpherical->GetPlainLightListDeviceLocalBuffer(),
        lightListsForSpherical->GetSectorToLightListRegionDeviceLocalBuffer(),
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

uint32_t RTGL1::LightManager::GetSpotlightCount() const
{
    return spotLightCount;
}

uint32_t RTGL1::LightManager::GetSpotlightCountPrev() const
{
    return spotLightCountPrev;
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

uint32_t RTGL1::LightManager::GetPolygonalLightCount() const
{
    return polyLightCount;
}

uint32_t RTGL1::LightManager::GetPolygonalLightCountPrev() const
{
    return polyLightCountPrev;
}

static_assert(RTGL1::MAX_FRAMES_IN_FLIGHT == 2, "");