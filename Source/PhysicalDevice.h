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

namespace RTGL1
{

class PhysicalDevice
{
public:
    explicit PhysicalDevice(VkInstance instance, uint32_t selectedPhysDevice);
    ~PhysicalDevice() = default;

    PhysicalDevice(const PhysicalDevice& other) = delete;
    PhysicalDevice(PhysicalDevice&& other) noexcept = delete;
    PhysicalDevice& operator=(const PhysicalDevice& other) = delete;
    PhysicalDevice& operator=(PhysicalDevice&& other) noexcept = delete;

    VkPhysicalDevice Get() const;
    uint32_t GetMemoryTypeIndex(uint32_t memoryTypeBits, VkFlags requirementsMask) const;
    const VkPhysicalDeviceMemoryProperties &GetMemoryProperties() const;
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR &GetRTPipelineProperties() const;

private:
    // selected physical device
    VkPhysicalDevice physDevice;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelineProperties;
};

}