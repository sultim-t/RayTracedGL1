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

#include "TextureDescriptors.h"

#include <array>

#include "Generated/ShaderCommonC.h"
#include "Const.h"

TextureDescriptors::TextureDescriptors(VkDevice _device) :
    device(_device),
    descPool(VK_NULL_HANDLE),
    descLayout(VK_NULL_HANDLE),
    descSets{},
    emptyTextureInfo{},
    currentWriteCount(0)
{
    writeImageInfos.resize(MAX_TEXTURE_COUNT);
    writeInfos.resize(MAX_TEXTURE_COUNT);

    CreateDescLayout();
    CreateDescPool();
    CreateDescSets();
}

TextureDescriptors::~TextureDescriptors()
{
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descLayout, nullptr);
}

VkDescriptorSet TextureDescriptors::GetDescSet(uint32_t frameIndex) const
{
    return descSets[frameIndex];
}

VkDescriptorSetLayout TextureDescriptors::GetDescSetLayout() const
{
    return descLayout;
}

void TextureDescriptors::SetEmptyTextureInfo(VkImageView view, VkSampler sampler)
{
    emptyTextureInfo.imageView = view;
    emptyTextureInfo.sampler = sampler;
    emptyTextureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void TextureDescriptors::CreateDescLayout()
{
    VkDescriptorSetLayoutBinding binding = {};

    binding.binding = BINDING_TEXTURES;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = MAX_TEXTURE_COUNT;
    binding.stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT |
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    VkResult r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descLayout);
    VK_CHECKERROR(r);
}

void TextureDescriptors::CreateDescPool()
{
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSize.descriptorCount = MAX_TEXTURE_COUNT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    VkResult r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);
}

void TextureDescriptors::CreateDescSets()
{
    VkDescriptorSetAllocateInfo setInfo = {};
    setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setInfo.descriptorPool = descPool;
    setInfo.descriptorSetCount = 1;
    setInfo.pSetLayouts = &descLayout;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkResult r = vkAllocateDescriptorSets(device, &setInfo, &descSets[i]);
        VK_CHECKERROR(r);
    }
}

void TextureDescriptors::UpdateTextureDesc(uint32_t frameIndex, uint32_t textureIndex, VkImageView view, VkSampler sampler)
{
    assert(view != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE);

    if  (currentWriteCount >= MAX_TEXTURE_COUNT)
    {
        assert(0);
        return;
    }

    VkDescriptorImageInfo &imageInfo = writeImageInfos[currentWriteCount];
    imageInfo.sampler = sampler;
    imageInfo.imageView = view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet &write = writeInfos[currentWriteCount];
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descSets[frameIndex];
    write.dstBinding = BINDING_TEXTURES;
    write.dstArrayElement = textureIndex;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;

    currentWriteCount++;
}

void TextureDescriptors::ResetTextureDesc(uint32_t frameIndex, uint32_t textureIndex)
{
    assert(emptyTextureInfo.imageView != VK_NULL_HANDLE &&
           emptyTextureInfo.imageLayout != VK_NULL_HANDLE &&
           emptyTextureInfo.sampler != VK_NULL_HANDLE);

    if (currentWriteCount >= MAX_TEXTURE_COUNT)
    {
        assert(0);
        return;
    }

    VkWriteDescriptorSet &write = writeInfos[currentWriteCount];
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descSets[frameIndex];
    write.dstBinding = BINDING_TEXTURES;
    write.dstArrayElement = textureIndex;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &emptyTextureInfo;

    currentWriteCount++;
}

void TextureDescriptors::FlushDescWrites()
{
    // must have constant size
    assert(writeInfos.size() == MAX_TEXTURE_COUNT);

    vkUpdateDescriptorSets(device, currentWriteCount, writeInfos.data(), 0, nullptr);
    currentWriteCount = 0;
}
