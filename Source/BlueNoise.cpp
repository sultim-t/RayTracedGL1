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

#include "BlueNoise.h"

#include <string>

#include "Generated/ShaderCommonC.h"
#include "Generated/BlueNoiseFileNames.h"
#include "ImageLoader.h"
#include "Utils.h"

static_assert(BLUE_NOISE_TEXTURE_COUNT == BlueNoiseFileNamesCount, "");

BlueNoise::BlueNoise(
    VkDevice _device,
    const char *_textureFolder,
    std::shared_ptr<MemoryAllocator> _allocator,
    const std::shared_ptr<CommandBufferManager> &_cmdManager,
    const std::shared_ptr<SamplerManager> &_samplerManager)
:
    device(_device),
    allocator(std::move(_allocator))
{
    assert(BlueNoiseFileNamesCount > 0);

    ImageLoader imageLoader;

    // load first file to get size info
    const std::string folderPath = _textureFolder;
    std::string curFileName = folderPath + BlueNoiseFileNames[0];

    uint32_t w, h;
    const uint32_t *data = imageLoader.LoadRGBA8(curFileName.c_str(), &w, &h);
    assert(w == BLUE_NOISE_TEXTURE_SIZE && h == BLUE_NOISE_TEXTURE_SIZE);

    const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const uint32_t bytesPerPixel = 4;

    // allocate buffer for all textures
    const VkDeviceSize oneTextureSize = bytesPerPixel * BLUE_NOISE_TEXTURE_SIZE * BLUE_NOISE_TEXTURE_SIZE;
    const VkDeviceSize dataSize = oneTextureSize * BlueNoiseFileNamesCount;

    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = dataSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    void *mappedData = nullptr;
    VkBuffer stagingBuffer = allocator->CreateStagingSrcTextureBuffer(&stagingInfo, &mappedData);

    assert(stagingBuffer != VK_NULL_HANDLE);

    // load each texture and place it in staging buffer
    memcpy(mappedData, data, oneTextureSize);
    imageLoader.FreeLoaded();

    for (uint32_t i = 1; i < BlueNoiseFileNamesCount; i++)
    {
        curFileName = folderPath + BlueNoiseFileNames[i];

        const uint32_t *src = imageLoader.LoadRGBA8(curFileName.c_str(), &w, &h);
        assert(w == BLUE_NOISE_TEXTURE_SIZE && h == BLUE_NOISE_TEXTURE_SIZE);

        void *dst = (uint8_t *)mappedData + oneTextureSize * i;
        
        memcpy(dst, src, oneTextureSize);
        imageLoader.FreeLoaded();
    }

    // create image that contains all blue noise textures as layers
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = imageFormat;
    info.extent = { BLUE_NOISE_TEXTURE_SIZE, BLUE_NOISE_TEXTURE_SIZE, 1 };
    info.mipLevels = 1;
    info.arrayLayers = BlueNoiseFileNamesCount;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    blueNoiseImages = allocator->CreateDstTextureImage(&info);

    SET_DEBUG_NAME(device, blueNoiseImages, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, "Blue noise Image");

    // copy from buffer to image
    VkCommandBuffer cmd = _cmdManager->StartGraphicsCmd();

    VkImageSubresourceRange allLayersRange = {};
    allLayersRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    allLayersRange.baseMipLevel = 0;
    allLayersRange.levelCount = 1;
    allLayersRange.baseArrayLayer = 0;
    allLayersRange.layerCount = BlueNoiseFileNamesCount;

    // to dst layout
    Utils::BarrierImage(
        cmd, blueNoiseImages,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        allLayersRange);

    VkBufferImageCopy copyInfo = {};
    copyInfo.bufferOffset = 0;
    // tigthly packed
    copyInfo.bufferRowLength = 0;
    copyInfo.bufferImageHeight = 0;
    copyInfo.imageOffset = { 0, 0, 0 };
    copyInfo.imageExtent = { BLUE_NOISE_TEXTURE_SIZE, BLUE_NOISE_TEXTURE_SIZE, 1 };
    copyInfo.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyInfo.imageSubresource.mipLevel = 0;
    copyInfo.imageSubresource.baseArrayLayer = 0;
    copyInfo.imageSubresource.layerCount = BlueNoiseFileNamesCount;

    vkCmdCopyBufferToImage(
        cmd, stagingBuffer, blueNoiseImages, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyInfo);

    // to read in shaders
    Utils::BarrierImage(
        cmd, blueNoiseImages,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        allLayersRange);

    // submit and wait
    _cmdManager->Submit(cmd);
    _cmdManager->WaitGraphicsIdle();

    allocator->DestroyStagingSrcTextureBuffer(stagingBuffer);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = blueNoiseImages;
    // multi-layer image
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = imageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = BlueNoiseFileNamesCount;

    VkResult r = vkCreateImageView(device, &viewInfo, nullptr, &blueNoiseImagesView);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, blueNoiseImagesView, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, "Blue noise View");

    VkSampler sampler = _samplerManager->GetSampler(
        RG_SAMPLER_FILTER_NEAREST, 
        RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_REPEAT);

    CreateDescriptors(sampler);
}

BlueNoise::~BlueNoise()
{
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);

    vkDestroyImageView(device, blueNoiseImagesView, nullptr);
    allocator->DestroyTextureImage(blueNoiseImages);
}

VkDescriptorSetLayout BlueNoise::GetDescSetLayout() const
{
    return descSetLayout;
}

VkDescriptorSet BlueNoise::GetDescSet() const
{
    return descSet;
}

void BlueNoise::CreateDescriptors(VkSampler sampler)
{
    VkResult r;

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = BINDING_BLUE_NOISE;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout);
    VK_CHECKERROR(r);

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descSetLayout;

    r = vkAllocateDescriptorSets(device, &allocInfo, &descSet);
    VK_CHECKERROR(r);

    VkDescriptorImageInfo imgInfo = {};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgInfo.imageView = blueNoiseImagesView;
    imgInfo.sampler = sampler;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descSet;
    write.dstBinding = BINDING_BLUE_NOISE;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    SET_DEBUG_NAME(device, descSetLayout, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT_EXT, "Blue noise Desc set layout");
    SET_DEBUG_NAME(device, descPool, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_POOL_EXT, "Blue noise Desc pool");
    SET_DEBUG_NAME(device, descSet, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT, "Blue noise Desc set");
}
