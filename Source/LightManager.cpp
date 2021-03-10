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

namespace RTGL1
{
constexpr double RG_PI = 3.141592653589793238462643383279502884197169399375105820974944592307816406;

constexpr uint32_t START_MAX_LIGHT_COUNT_SPHERICAL = 1024;
constexpr uint32_t START_MAX_LIGHT_COUNT_DIRECTIONAL = 32;

constexpr uint32_t STEP_LIGHT_COUNT_SPHERICAL = 1024;
constexpr uint32_t STEP_LIGHT_COUNT_DIRECTIONAL = 32;
}

RTGL1::LightManager::LightManager(
    VkDevice _device, 
    std::shared_ptr<MemoryAllocator> &_allocator)
:
    device(_device),
    sphericalLightCount(0),
    directionalLightCount(0),
    maxSphericalLightCount(START_MAX_LIGHT_COUNT_SPHERICAL),
    maxDirectionalLightCount(START_MAX_LIGHT_COUNT_DIRECTIONAL),
    descSetLayout(VK_NULL_HANDLE),
    descPool(VK_NULL_HANDLE),
    descSets{},
    needDescSetUpdate{}
{
    sphericalLights = std::make_shared<AutoBuffer>(device, _allocator, "Lights spherical staging", "Lights spherical");
    directionalLights = std::make_shared<AutoBuffer>(device, _allocator, "Lights directional staging", "Lights directional");

    sphericalLights->Create(sizeof(ShLightSpherical) * maxSphericalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    directionalLights->Create(sizeof(ShLightDirectional) * maxDirectionalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    CreateDescriptors();
}

RTGL1::LightManager::~LightManager()
{
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);
}

uint32_t RTGL1::LightManager::GetSphericalLightCount() const
{
    return sphericalLightCount;
}

uint32_t RTGL1::LightManager::GetDirectionalLightCount() const
{
    return directionalLightCount;
}

void RTGL1::LightManager::AddSphericalLight(uint32_t frameIndex, const RgSphericalLightUploadInfo &info)
{
    ShLightSpherical light = {};
    memcpy(light.color, info.color.data, sizeof(float) * 3);
    memcpy(light.position, info.position.data, sizeof(float) * 3);
    light.radius = std::max(0.0f, info.radius);
    light.falloff = std::max(light.radius, std::max(0.0f, info.falloffDistance));

    if (sphericalLightCount + 1 >= maxSphericalLightCount)
    {
        // TODO: schedule buffer to be destroyed in the next frame
        return;

        /*uint32_t oldCount = maxSphericalLightCount;
        maxSphericalLightCount += STEP_LIGHT_COUNT_SPHERICAL;

        ShLightSpherical *copy = new ShLightSpherical[oldCount];

        sphericalLights->Destroy();
        sphericalLights->Create(sizeof(ShLightSpherical) * maxSphericalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        memcpy(sphericalLights->GetMapped(frameIndex), copy, sizeof(ShLightSpherical) * oldCount);
        delete[] copy;

        // mark descriptors to be updated, in all frames
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            needDescSetUpdate[i] = true;
        }*/
    }

    ShLightSpherical *dst = (ShLightSpherical*)sphericalLights->GetMapped(frameIndex);
    dst += sphericalLightCount;

    memcpy(dst, &light, sizeof(ShLightSpherical));

    sphericalLightCount++;
}

void RTGL1::LightManager::AddDirectionalLight(uint32_t frameIndex, const RgDirectionalLightUploadInfo &info)
{
    ShLightDirectional light = {};
    memcpy(light.color, info.color.data, sizeof(float) * 3);
    light.direction[0] = -info.direction.data[0];
    light.direction[1] = -info.direction.data[1];
    light.direction[2] = -info.direction.data[2];
    light.tanAngularRadius = tanf(std::max(0.0, info.angularDiameterDegrees * 0.5) * RG_PI / 180.0);

    if (directionalLightCount + 1 >= maxDirectionalLightCount)
    {
        // TODO: schedule buffer to be destroyed in the next frame
        return;

        /*uint32_t oldCount = maxDirectionalLightCount;
        maxDirectionalLightCount += STEP_LIGHT_COUNT_DIRECTIONAL;

        ShLightDirectional *copy = new ShLightDirectional[oldCount];

        directionalLights->Destroy();
        directionalLights->Create(sizeof(ShLightDirectional) * maxDirectionalLightCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        memcpy(directionalLights->GetMapped(frameIndex), copy, sizeof(ShLightDirectional) * oldCount);
        delete[] copy;

        // mark descriptors to be updated, in all frames
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            needDescSetUpdate[i] = true;
        }*/
    }

    ShLightDirectional *dst = (ShLightDirectional*)directionalLights->GetMapped(frameIndex);
    dst += directionalLightCount;

    memcpy(dst, &light, sizeof(ShLightDirectional));

    directionalLightCount++;
}

void RTGL1::LightManager::Clear()
{
    sphericalLightCount = 0;
    directionalLightCount = 0;
}

void RTGL1::LightManager::CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex)
{
    sphericalLights->CopyFromStaging(cmd, frameIndex, sizeof(ShLightSpherical) * sphericalLightCount);
    directionalLights->CopyFromStaging(cmd, frameIndex, sizeof(ShLightDirectional) * directionalLightCount);

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

void RTGL1::LightManager::CreateDescriptors()
{
    VkResult r;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {};

    auto &bndSph = bindings[0];
    bndSph.binding = BINDING_LIGHT_SOURCES_SPHERICAL;
    bndSph.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bndSph.descriptorCount = 1;
    bndSph.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    auto &bndDir = bindings[1];
    bndDir.binding = BINDING_LIGHT_SOURCES_DIRECTIONAL;
    bndDir.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bndDir.descriptorCount = 1;
    bndDir.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, "Light buffers Desc set layout");

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = bindings.size();

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descPool, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT, "Light buffers Desc set pool");

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descSetLayout;
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkAllocateDescriptorSets(device, &allocInfo, &descSets[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, descSets[i], VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, "Light buffers Desc set");
    }
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        UpdateDescriptors(i);
    }
}

void RTGL1::LightManager::UpdateDescriptors(uint32_t frameIndex)
{
    VkDescriptorBufferInfo bfSphInfo = {};
    bfSphInfo.buffer = sphericalLights->GetDeviceLocal();
    bfSphInfo.offset = 0;
    bfSphInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo bfDirInfo = {};
    bfDirInfo.buffer = directionalLights->GetDeviceLocal();
    bfDirInfo.offset = 0;
    bfDirInfo.range = VK_WHOLE_SIZE;

    std::array<VkWriteDescriptorSet, 2> wrts = {};

    auto &wrtSph = wrts[0];
    wrtSph.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wrtSph.dstSet = descSets[frameIndex];
    wrtSph.dstBinding = BINDING_LIGHT_SOURCES_SPHERICAL;
    wrtSph.dstArrayElement = 0;
    wrtSph.descriptorCount = 1;
    wrtSph.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wrtSph.pBufferInfo = &bfSphInfo;

    auto &wrtDir = wrts[1];
    wrtDir.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wrtDir.dstSet = descSets[frameIndex];
    wrtDir.dstBinding = BINDING_LIGHT_SOURCES_DIRECTIONAL;
    wrtDir.dstArrayElement = 0;
    wrtDir.descriptorCount = 1;
    wrtDir.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wrtDir.pBufferInfo = &bfDirInfo;

    vkUpdateDescriptorSets(device, wrts.size(), wrts.data(), 0, nullptr);
}