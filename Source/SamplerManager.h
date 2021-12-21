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
#include <unordered_map>

#include "Common.h"
#include "RTGL1/RTGL1.h"

namespace RTGL1
{

// TODO: separate classes for fixed / updateable(with SamplerHandle) samplers
class SamplerManager
{
public:
    class Handle
    {
        friend class SamplerManager;

    public:
        explicit Handle();
        explicit Handle(RgSamplerFilter filter, RgSamplerAddressMode addressModeU, RgSamplerAddressMode addressModeV);

        bool operator==(const Handle &other) const
        {
            return other.internalIndex == internalIndex;
        }

    private:
        uint32_t internalIndex;
    };

public:
    SamplerManager(VkDevice device, uint32_t anisotropy);
    ~SamplerManager();

    SamplerManager(const SamplerManager &other) = delete;
    SamplerManager(SamplerManager &&other) noexcept = delete;
    SamplerManager &operator=(const SamplerManager &other) = delete;
    SamplerManager &operator=(SamplerManager &&other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);

    VkSampler GetSampler(
        RgSamplerFilter filter, 
        RgSamplerAddressMode addressModeU, 
        RgSamplerAddressMode addressModeV) const;

    // In case, if mip load bias was updated and a fresh sampler is required
    VkSampler GetSampler(const Handle &handle) const;

    // Wait idle and recreate all the samplers with new lod bias
    bool TryChangeMipLodBias(uint32_t frameIndex, float newMipLodBias);

private:
    void CreateAllSamplers(uint32_t anisotropy, float mipLodBias);
    void AddAllSamplersToDestroy(uint32_t frameIndex);

    static uint32_t ToIndex(
        RgSamplerFilter filter,
        RgSamplerAddressMode addressModeU,
        RgSamplerAddressMode addressModeV);

    static uint32_t ToIndex(
        VkFilter filter,
        VkSamplerAddressMode addressModeU,
        VkSamplerAddressMode addressModeV);

private:
    VkDevice device;

    std::unordered_map<uint32_t, VkSampler> samplers;
    std::vector<VkSampler> samplersToDelete[MAX_FRAMES_IN_FLIGHT];
    float mipLodBias;
    uint32_t anisotropy;
};

}