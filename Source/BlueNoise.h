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

#include "Common.h"
#include "CommandBufferManager.h"
#include "MemoryAllocator.h"
#include "SamplerManager.h"

namespace RTGL1
{

class BlueNoise
{
public:
    explicit BlueNoise(
        VkDevice device, 
        const char *blueNoiseFilePath,
        std::shared_ptr<MemoryAllocator> allocator,
        const std::shared_ptr<CommandBufferManager> &cmdManager,
        const std::shared_ptr<SamplerManager> &samplerManager);
    ~BlueNoise();

    BlueNoise(const BlueNoise &other) = delete;
    BlueNoise(BlueNoise &&other) noexcept = delete;
    BlueNoise &operator=(const BlueNoise &other) = delete;
    BlueNoise &operator=(BlueNoise &&other) noexcept = delete;

    VkDescriptorSetLayout GetDescSetLayout() const;
    VkDescriptorSet GetDescSet() const;

private:
    void CreateDescriptors(VkSampler sampler);

private:
    VkDevice device;
    std::shared_ptr<MemoryAllocator> allocator;

    VkImage blueNoiseImages;
    VkImageView blueNoiseImagesView;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool descPool;
    VkDescriptorSet descSet;
};

}