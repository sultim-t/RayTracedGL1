#pragma once

#include <map>

#include "Common.h"
#include "RTGL1/RTGL1.h"

class SamplerManager
{
public:
    SamplerManager(VkDevice device);
    ~SamplerManager();

    SamplerManager(const SamplerManager &other) = delete;
    SamplerManager(SamplerManager &&other) noexcept = delete;
    SamplerManager &operator=(const SamplerManager &other) = delete;
    SamplerManager &operator=(SamplerManager &&other) noexcept = delete;

    VkSampler GetSampler(
        RgSamplerFilter filter, 
        RgSamplerAddressMode addressModeU, 
        RgSamplerAddressMode addressModeV) const;

private:
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
    std::map<uint32_t, VkSampler> samplers;
};
