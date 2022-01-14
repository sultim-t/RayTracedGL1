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

#include "SamplerManager.h"

#include <string>

#include "RgException.h"

using namespace RTGL1;


static VkFilter RgFilterToVk(RgSamplerFilter r)
{
    switch (r)
    {
        case RG_SAMPLER_FILTER_LINEAR:  return VK_FILTER_LINEAR;
        case RG_SAMPLER_FILTER_NEAREST: return VK_FILTER_NEAREST;
        default: assert(0); return VK_FILTER_NEAREST;
    }
}

static VkSamplerAddressMode RgAddressModeToVk(RgSamplerAddressMode r)
{
    switch (r)
    {
        case RG_SAMPLER_ADDRESS_MODE_REPEAT:                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case RG_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:       return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:         return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:       return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case RG_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:  return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
        default: assert(0); return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}


SamplerManager::SamplerManager(VkDevice _device, uint32_t _anisotropy) : device(_device), mipLodBias(0.0f), anisotropy(_anisotropy)
{
    CreateAllSamplers(anisotropy, mipLodBias);
}

SamplerManager::~SamplerManager()
{
    for (auto &p : samplers)
    {
        vkDestroySampler(device, p.second, nullptr);
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        for (VkSampler s : samplersToDelete[i])
        {
            vkDestroySampler(device, s, nullptr);
        }

        samplersToDelete[i].clear();
    }
;
    samplers.clear();
}

void RTGL1::SamplerManager::CreateAllSamplers(uint32_t _anisotropy, float _mipLodBias)
{
    assert(samplers.empty());
    assert(_anisotropy == 0 || _anisotropy == 2 || _anisotropy == 4 || _anisotropy == 8 || _anisotropy == 16);

    VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.mipLodBias = _mipLodBias;
    info.anisotropyEnable = _anisotropy > 0 ? VK_TRUE : VK_FALSE;
    info.maxAnisotropy = _anisotropy;
    info.compareEnable = VK_FALSE;
    info.minLod = 0.0f;
    info.maxLod = VK_LOD_CLAMP_NONE;
    info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;

    VkFilter filters[] = 
    {
        VK_FILTER_NEAREST,
        VK_FILTER_LINEAR
    };

    VkSamplerAddressMode modes[] =
    {
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ,
        VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
    };

    for (auto filter : filters)
    {
        for (auto modeU : modes)
        {
            for (auto modeV : modes)
            {
                info.minFilter = info.magFilter = filter;
                info.addressModeU = modeU;
                info.addressModeV = modeV;

                uint32_t index = ToIndex(filter, modeU, modeV, false);
                VkSampler sampler;

                VkResult r = vkCreateSampler(device, &info, nullptr, &sampler);
                VK_CHECKERROR(r);

                assert(samplers.find(index) == samplers.end());

                samplers[index] = sampler;
            }
        }
    }

    // corner case: create only 1 sampler with 'forceLowestMip'
    {
        info.minFilter = info.magFilter = VK_FILTER_LINEAR;
        info.addressModeU = info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.minLod = VK_LOD_CLAMP_NONE - 1;

        uint32_t index = ToIndex(info.minFilter, info.addressModeU, info.addressModeV, true);
        VkSampler sampler;

        VkResult r = vkCreateSampler(device, &info, nullptr, &sampler);
        VK_CHECKERROR(r);

        assert(samplers.find(index) == samplers.end());

        samplers[index] = sampler;
    }
}

void RTGL1::SamplerManager::AddAllSamplersToDestroy(uint32_t frameIndex)
{
    for (auto &p : samplers)
    {
        samplersToDelete[frameIndex].push_back(p.second);
    }

    samplers.clear();
}

void RTGL1::SamplerManager::PrepareForFrame(uint32_t frameIndex)
{
    for (VkSampler s : samplersToDelete[frameIndex])
    {
        vkDestroySampler(device, s, nullptr);
    }

    samplersToDelete[frameIndex].clear();
}

VkSampler SamplerManager::GetSampler(
    RgSamplerFilter filter, RgSamplerAddressMode addressModeU, RgSamplerAddressMode addressModeV, bool forceLowestMip) const
{
    uint32_t index = ToIndex(filter, addressModeU, addressModeV, forceLowestMip);

    auto f = samplers.find(index);

    if (f != samplers.end())
    {
        return f->second;
    }
    else
    {
        throw RgException(RG_WRONG_MATERIAL_PARAMETER, 
                          "Wrong RgSamplerFilter(" + std::to_string(filter) + 
                          ") or RgSamplerAddressMode (U: " + std::to_string(addressModeU) +
                          ", V: " + std::to_string(addressModeV) + ") value");
        return VK_NULL_HANDLE;
    }
}

VkSampler RTGL1::SamplerManager::GetSampler(const Handle &handle) const
{
    assert(handle.internalIndex != 0);
    auto f = samplers.find(handle.internalIndex);

    if (f == samplers.end())
    {
        // pHandle->internalIndex is incorrect
        assert(0);
        return VK_NULL_HANDLE;
    }

    return f->second;
}

bool RTGL1::SamplerManager::TryChangeMipLodBias(uint32_t frameIndex, float newMipLodBias)
{
    constexpr float delta = 0.025f;

    if (std::abs(newMipLodBias - mipLodBias) < delta)
    {
        return false;
    }
    
    AddAllSamplersToDestroy(frameIndex);
    CreateAllSamplers(anisotropy, newMipLodBias);

    mipLodBias = newMipLodBias;
    return true;
}



// sampler flags

#define FILTER_LINEAR                           (1u << 0)
#define FILTER_NEAREST                          (2u << 0)
#define FILTER_MASK                             (3u << 0)

#define ADDRESS_MODE_U_REPEAT                   (1u << 2)
#define ADDRESS_MODE_U_MIRRORED_REPEAT          (2u << 2)
#define ADDRESS_MODE_U_CLAMP_TO_EDGE            (3u << 2)
#define ADDRESS_MODE_U_CLAMP_TO_BORDER          (4u << 2)
#define ADDRESS_MODE_U_MIRROR_CLAMP_TO_EDGE     (5u << 2)
#define ADDRESS_MODE_U_MASK                     (7u << 2)

#define ADDRESS_MODE_V_REPEAT                   (1u << 5)
#define ADDRESS_MODE_V_MIRRORED_REPEAT          (2u << 5)
#define ADDRESS_MODE_V_CLAMP_TO_EDGE            (3u << 5)
#define ADDRESS_MODE_V_CLAMP_TO_BORDER          (4u << 5)
#define ADDRESS_MODE_V_MIRROR_CLAMP_TO_EDGE     (5u << 5)
#define ADDRESS_MODE_V_MASK                     (7u << 5)

#define FORCE_LOWEST_MIP_BOOL                   (1u << 8)

uint32_t SamplerManager::ToIndex(
    RgSamplerFilter filter, RgSamplerAddressMode addressModeU, RgSamplerAddressMode addressModeV, bool forceLowestMip)
{
    return ToIndex(RgFilterToVk(filter), RgAddressModeToVk(addressModeU), RgAddressModeToVk(addressModeV), forceLowestMip);
}

uint32_t SamplerManager::ToIndex(
    VkFilter filter, VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV, bool forceLowestMip)
{
    uint32_t index = 0;

    if (forceLowestMip)
    {
        return FORCE_LOWEST_MIP_BOOL;
    }

    switch (filter)
    {
        case VK_FILTER_NEAREST: index |= FILTER_NEAREST;    break;
        case VK_FILTER_LINEAR:  index |= FILTER_LINEAR;     break;
        default: assert(0); break;
    }

    switch (addressModeU)
    {
        case VK_SAMPLER_ADDRESS_MODE_REPEAT:                index |= ADDRESS_MODE_U_REPEAT;                 break;
        case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:       index |= ADDRESS_MODE_U_MIRRORED_REPEAT;        break;
        case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:         index |= ADDRESS_MODE_U_CLAMP_TO_EDGE;          break;
        case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:       index |= ADDRESS_MODE_U_CLAMP_TO_BORDER;        break;
        case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:  index |= ADDRESS_MODE_U_MIRROR_CLAMP_TO_EDGE;   break;
        default: assert(0); break;
    }

    switch (addressModeV)
    {
        case VK_SAMPLER_ADDRESS_MODE_REPEAT:                index |= ADDRESS_MODE_V_REPEAT;                 break;
        case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:       index |= ADDRESS_MODE_V_MIRRORED_REPEAT;        break;
        case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:         index |= ADDRESS_MODE_V_CLAMP_TO_EDGE;          break;
        case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:       index |= ADDRESS_MODE_V_CLAMP_TO_BORDER;        break;
        case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:  index |= ADDRESS_MODE_V_MIRROR_CLAMP_TO_EDGE;   break;
        default: assert(0); break;
    }

    assert(index != 0);
    return index;
}

static uint32_t SwapFilterInIndex(uint32_t srcIndex, RgSamplerFilter newFilter)
{
    if (srcIndex & FORCE_LOWEST_MIP_BOOL)
    {
        return FORCE_LOWEST_MIP_BOOL;
    }

    // clear previous filter type
    uint32_t i = srcIndex & (~FILTER_MASK);

    switch (newFilter)
    {
        case RG_SAMPLER_FILTER_NEAREST: i |= FILTER_NEAREST;    break;
        case RG_SAMPLER_FILTER_LINEAR:  i |= FILTER_LINEAR;     break;
        default: assert(0); break;
    }

    assert(i != 0);
    return i;
}

RTGL1::SamplerManager::Handle::Handle() :
    internalIndex(0), hasDynamicSamplerFilter(false)
{}

RTGL1::SamplerManager::Handle::Handle(RgSamplerFilter filter, RgSamplerAddressMode addressModeU, RgSamplerAddressMode addressModeV, RgMaterialCreateFlags flags) :
    internalIndex(ToIndex(filter, addressModeU, addressModeV, flags & RG_MATERIAL_CREATE_FORCE_LOWEST_MIP_BIT)), hasDynamicSamplerFilter(flags &RG_MATERIAL_CREATE_DYNAMIC_SAMPLER_FILTER_BIT)
{}

bool RTGL1::SamplerManager::Handle::SetIfHasDynamicSamplerFilter(RgSamplerFilter newDynamicSamplerFilter)
{
    if (hasDynamicSamplerFilter)
    {
        internalIndex = SwapFilterInIndex(internalIndex, newDynamicSamplerFilter);
        return true;
    }

    return false;
}
