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
#include "AutoBuffer.h"

namespace RTGL1
{

struct ShLightSpherical;
struct ShLightDirectional;

class LightManager
{
public:
    LightManager(VkDevice device, std::shared_ptr<MemoryAllocator> &allocator);
    ~LightManager();

    LightManager(const LightManager &other) = delete;
    LightManager(LightManager &&other) noexcept = delete;
    LightManager &operator=(const LightManager &other) = delete;
    LightManager &operator=(LightManager &&other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);
    void Reset();

    uint32_t GetSphericalLightCount() const;
    uint32_t GetDirectionalLightCount() const;
    uint32_t GetSphericalLightCountPrev() const;
    uint32_t GetDirectionalLightCountPrev() const;

    void AddSphericalLight(uint32_t frameIndex, const RgSphericalLightUploadInfo &info);
    void AddDirectionalLight(uint32_t frameIndex, const RgDirectionalLightUploadInfo &info);

    void CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex);

    VkDescriptorSetLayout GetDescSetLayout();
    VkDescriptorSet GetDescSet(uint32_t frameIndex);

private:
    void FillMatchPrev(
        const std::map<uint64_t, uint32_t> *pUniqueToPrevIndex,
        const std::shared_ptr<AutoBuffer> &matchPrev,
        uint32_t curFrameIndex, uint32_t curLightIndex, uint64_t uniqueID);

    void FillInfo(const RgSphericalLightUploadInfo &info, ShLightSpherical *dst);
    void FillInfo(const RgDirectionalLightUploadInfo &info, ShLightDirectional *dst);

    void CreateDescriptors();
    void UpdateDescriptors(uint32_t frameIndex);

private:
    VkDevice device;

    std::shared_ptr<AutoBuffer> sphericalLights;
    std::shared_ptr<AutoBuffer> directionalLights;

    std::shared_ptr<AutoBuffer> sphericalLightMatchPrev;
    std::shared_ptr<AutoBuffer> directionalLightMatchPrev;

    std::map<uint64_t, uint32_t> sphUniqueIDToPrevIndex[MAX_FRAMES_IN_FLIGHT];
    std::map<uint64_t, uint32_t> dirUniqueIDToPrevIndex[MAX_FRAMES_IN_FLIGHT];

    uint32_t sphLightCount;
    uint32_t dirLightCount;
    uint32_t sphLightCountPrev;
    uint32_t dirLightCountPrev;

    uint32_t maxSphericalLightCount;
    uint32_t maxDirectionalLightCount;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];

    bool needDescSetUpdate[MAX_FRAMES_IN_FLIGHT];
};

}