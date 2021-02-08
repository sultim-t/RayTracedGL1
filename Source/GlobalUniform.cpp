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

#include "GlobalUniform.h"

#include "Generated/ShaderCommonC.h"

using namespace RTGL1;

GlobalUniform::GlobalUniform(VkDevice _device, const std::shared_ptr<MemoryAllocator> &_allocator, bool _deviceLocal)
:
    device(_device),
    uniformData(std::make_shared<ShGlobalUniform>()),
    descPool(VK_NULL_HANDLE),
    descSetLayout(VK_NULL_HANDLE),
    descSets{}
{
    const VkDeviceSize size = sizeof(ShGlobalUniform);

    VkMemoryPropertyFlags properties = _deviceLocal ?
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT :
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        uniformBuffers[i].Init(_allocator, size,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, properties,
                               "Uniform buffer");
    }

    CreateDescriptors();
}

void GlobalUniform::CreateDescriptors()
{
    VkResult r;

    // create descriptor layout for uniform buffer
    VkDescriptorSetLayoutBinding uniformBinding = {};
    uniformBinding.binding = BINDING_GLOBAL_UNIFORM;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1;
    uniformBinding.stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uniformBinding;

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout);
    VK_CHECKERROR(r);

    // create descriptor pool
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    // allocate sets
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descSetLayout;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkAllocateDescriptorSets(device, &allocInfo, &descSets[i]);
        VK_CHECKERROR(r);
    }

    // bind buffers to sets
    VkDescriptorBufferInfo bufferInfos[MAX_FRAMES_IN_FLIGHT] = {};
    VkWriteDescriptorSet writes[MAX_FRAMES_IN_FLIGHT] = {};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        bufferInfos[i].buffer = uniformBuffers[i].GetBuffer();
        bufferInfos[i].offset = 0;
        bufferInfos[i].range = VK_WHOLE_SIZE;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descSets[i];
        writes[i].dstBinding = BINDING_GLOBAL_UNIFORM;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(device, MAX_FRAMES_IN_FLIGHT, writes, 0, nullptr);
}

GlobalUniform::~GlobalUniform()
{
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
}

void GlobalUniform::Upload(uint32_t frameIndex)
{
    SetData(frameIndex, uniformData.get(), sizeof(ShGlobalUniform));
}

ShGlobalUniform *GlobalUniform::GetData()
{
    return uniformData.get();
}

const ShGlobalUniform *GlobalUniform::GetData() const
{
    return uniformData.get();
}

VkDescriptorSet GlobalUniform::GetDescSet(uint32_t frameIndex) const
{
    return descSets[frameIndex];
}

VkDescriptorSetLayout GlobalUniform::GetDescSetLayout() const
{
    return descSetLayout;
}

void GlobalUniform::SetData(uint32_t frameIndex, const void *data, VkDeviceSize dataSize)
{
    assert(frameIndex >= 0 && frameIndex < MAX_FRAMES_IN_FLIGHT);
    assert(uniformBuffers[frameIndex].GetSize() <= dataSize);

    void *mapped = uniformBuffers[frameIndex].Map();
    memcpy(mapped, data, dataSize);

    uniformBuffers[frameIndex].Unmap();
}

