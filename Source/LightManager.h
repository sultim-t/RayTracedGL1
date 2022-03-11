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

#pragma once

#include "RTGL1/RTGL1.h"
#include "Common.h"
#include "Containers.h"
#include "AutoBuffer.h"
#include "GlobalUniform.h"
#include "LightLists.h"

namespace RTGL1
{

class LightManager
{
public:
    LightManager(VkDevice device, std::shared_ptr<MemoryAllocator> &allocator, std::shared_ptr<SectorVisibility> &sectorVisibility);
    ~LightManager();

    LightManager(const LightManager &other) = delete;
    LightManager(LightManager &&other) noexcept = delete;
    LightManager &operator=(const LightManager &other) = delete;
    LightManager &operator=(LightManager &&other) noexcept = delete;

    void PrepareForFrame(VkCommandBuffer cmd, uint32_t frameIndex);
    void Reset();

    uint32_t GetSpotlightCount() const;
    uint32_t GetSpotlightCountPrev() const;
    uint32_t GetSphericalLightCount() const;
    uint32_t GetSphericalLightCountPrev() const;
    uint32_t GetDirectionalLightCount() const;
    uint32_t GetDirectionalLightCountPrev() const;
    uint32_t GetPolygonalLightCount() const;
    uint32_t GetPolygonalLightCountPrev() const;

    void AddSphericalLight(uint32_t frameIndex, const RgSphericalLightUploadInfo &info);
    void AddPolygonalLight(uint32_t frameIndex, const RgPolygonalLightUploadInfo &info);
    void AddDirectionalLight(uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, const RgDirectionalLightUploadInfo &info);
    void AddSpotlight(uint32_t frameIndex, const std::shared_ptr<GlobalUniform> &uniform, const RgSpotlightUploadInfo &info);

    void CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex);

    VkDescriptorSetLayout GetDescSetLayout();
    VkDescriptorSet GetDescSet(uint32_t frameIndex);

private:
    void FillMatchPrev(
        const rgl::unordered_map<UniqueLightID, LightArrayIndex> *pUniqueToPrevIndex,
        const std::shared_ptr<AutoBuffer> &matchPrev,
        uint32_t curFrameIndex, LightArrayIndex lightIndexInCurFrame, UniqueLightID uniqueID);

    void CreateDescriptors();
    void UpdateDescriptors(uint32_t frameIndex);

private:
    VkDevice device;

    std::shared_ptr<LightLists> lightListsForPolygonal;
    std::shared_ptr<LightLists> lightListsForSpherical;

    std::shared_ptr<AutoBuffer> sphericalLights;
    std::shared_ptr<AutoBuffer> polygonalLights;
    Buffer sphericalLightsPrev;
    Buffer polygonalLightsPrev;

    // The light was uploaded in previous frame with LightArrayIndex==i.
    // We need to access the same light, but in current frame it has other LightArrayIndex==k.
    // These arrays are used to access 'k' by 'i'.
    std::shared_ptr<AutoBuffer> sphericalLightMatchPrev;
    std::shared_ptr<AutoBuffer> polygonalLightMatchPrev;

    rgl::unordered_map<UniqueLightID, LightArrayIndex> sphericalUniqueIDToPrevIndex[MAX_FRAMES_IN_FLIGHT];
    rgl::unordered_map<UniqueLightID, LightArrayIndex> polygonalUniqueIDToPrevIndex[MAX_FRAMES_IN_FLIGHT];

    uint32_t sphLightCount;
    uint32_t sphLightCountPrev;

    uint32_t dirLightCount;
    uint32_t dirLightCountPrev;
    RgFloat3D dirLightDirectionPrev;

    uint32_t spotLightCount;
    uint32_t spotLightCountPrev;
    RgFloat3D spotLightPositionPrev;
    RgFloat3D spotLightDirectionPrev;
    RgFloat3D spotLightUpVectorPrev;

    uint32_t polyLightCount;
    uint32_t polyLightCountPrev;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];

    bool needDescSetUpdate[MAX_FRAMES_IN_FLIGHT];
};

}