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

#include "PhysicalDevice.h"

#include <string>
#include <vector>

#include "RgException.h"

using namespace RTGL1;

PhysicalDevice::PhysicalDevice(VkInstance instance)
    : physDevice(VK_NULL_HANDLE), memoryProperties{}, rtPipelineProperties{}, asProperties{}
{
    VkResult r;

    uint32_t physCount = 0;
    r = vkEnumeratePhysicalDevices(instance, &physCount, nullptr);

    if (physCount == 0)
    {
        throw RgException(RG_CANT_FIND_PHYSICAL_DEVICE, "Can't find physical devices");
    }

    std::vector<VkPhysicalDevice> physicalDevices;
    physicalDevices.resize(physCount);
    r = vkEnumeratePhysicalDevices(instance, &physCount, physicalDevices.data());
    VK_CHECKERROR(r);

    for (VkPhysicalDevice p : physicalDevices)
    {
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures = {};
        rtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

        VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &rtFeatures;
        vkGetPhysicalDeviceFeatures2(p, &deviceFeatures2);

        if (rtFeatures.rayTracingPipeline)
        {
            physDevice = p;

            rtPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            asProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
            VkPhysicalDeviceProperties2 deviceProp2 = {};
            deviceProp2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProp2.pNext = &rtPipelineProperties;
            rtPipelineProperties.pNext = &asProperties;

            vkGetPhysicalDeviceProperties2(physDevice, &deviceProp2);
            vkGetPhysicalDeviceMemoryProperties(physDevice, &memoryProperties);

            break;
        }
    }

    if (physDevice == VK_NULL_HANDLE)
    {
        throw RgException(RG_CANT_FIND_PHYSICAL_DEVICE, "Can't find physical device with ray tracing support");
    }
}

VkPhysicalDevice PhysicalDevice::Get() const
{
    return physDevice;
}

uint32_t PhysicalDevice::GetMemoryTypeIndex(uint32_t memoryTypeBits, VkFlags requirementsMask) const
{
    VkMemoryPropertyFlags flagsToIgnore = 0;

    if (requirementsMask & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {        
        // device-local memory must not be host visible
        flagsToIgnore = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    else
    {
        // host visible memory must not be device-local
        flagsToIgnore = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
    

    // for each memory type available for this device
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        // if type is available
        if ((memoryTypeBits & 1u) == 1)
        {
            VkMemoryPropertyFlags flags = memoryProperties.memoryTypes[i].propertyFlags;

            bool isSuitable = (flags & requirementsMask) == requirementsMask;
            bool isIgnored = (flags & flagsToIgnore) == flagsToIgnore;
            
            if (isSuitable && !isIgnored)
            {
                return i;
            }
        }

        memoryTypeBits >>= 1u;
    }

    throw RgException(RG_GRAPHICS_API_ERROR, "Can't find memory type for given memory property flags (" + std::to_string(requirementsMask) + ")");
    return 0;
}

const VkPhysicalDeviceMemoryProperties &PhysicalDevice::GetMemoryProperties() const
{
    return memoryProperties;
}

const VkPhysicalDeviceRayTracingPipelinePropertiesKHR &PhysicalDevice::GetRTPipelineProperties() const
{
    return rtPipelineProperties;
}

const VkPhysicalDeviceAccelerationStructurePropertiesKHR& PhysicalDevice::GetASProperties() const
{
    return asProperties;
}
