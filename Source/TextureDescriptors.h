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

#pragma once

#include <vector>

#include "Common.h"

class TextureDescriptors
{
public:
    explicit TextureDescriptors(VkDevice device);
    ~TextureDescriptors();

    TextureDescriptors(const TextureDescriptors &other) = delete;
    TextureDescriptors(TextureDescriptors &&other) noexcept = delete;
    TextureDescriptors &operator=(const TextureDescriptors &other) = delete;
    TextureDescriptors &operator=(TextureDescriptors &&other) noexcept = delete;

    void UpdateTextureDesc(uint32_t frameIndex, uint32_t textureIndex, VkImageView view, VkSampler sampler);
    void ResetTextureDesc(uint32_t frameIndex, uint32_t textureIndex);

    // Must be called after a series of UpdateTextureDesc and
    // ResetTextureDesc to make an actual desc write 
    void FlushDescWrites();

    VkDescriptorSet GetDescSet(uint32_t frameIndex) const;
    VkDescriptorSetLayout GetDescSetLayout() const;

    // Set texture info that should be used in ResetTextureDesc(..)
    void SetEmptyTextureInfo(VkImageView view, VkSampler sampler);

private:
    void CreateDescLayout();
    void CreateDescPool();
    void CreateDescSets();

    bool IsCached(uint32_t frameIndex, uint32_t textureIndex, VkImageView view, VkSampler sampler);
    void AddToCache(uint32_t frameIndex, uint32_t textureIndex, VkImageView view, VkSampler sampler);
    void ResetCache(uint32_t frameIndex, uint32_t textureIndex);

private:
    struct UpdatedDescCache
    {
        VkImageView view;
        VkSampler sampler;
    };

private:
    VkDevice device;

    VkDescriptorPool descPool;
    VkDescriptorSetLayout descLayout;
    VkDescriptorSet descSets[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorImageInfo emptyTextureInfo;

    uint32_t currentWriteCount;
    std::vector<VkDescriptorImageInfo> writeImageInfos;
    std::vector<VkWriteDescriptorSet> writeInfos;

    std::vector<UpdatedDescCache> writeCache[MAX_FRAMES_IN_FLIGHT];
};