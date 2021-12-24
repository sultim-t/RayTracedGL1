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
#include "ImageLoader.h"
#include "Utils.h"
#include "RgException.h"

using namespace RTGL1;

BlueNoise::BlueNoise(
    VkDevice _device,
    const char *_blueNoiseFilePath,
    std::shared_ptr<MemoryAllocator> _allocator,
    const std::shared_ptr<CommandBufferManager> &_cmdManager,
    std::shared_ptr<UserFileLoad> _userFileLoad)
:
    device(_device),
    allocator(std::move(_allocator)),
    blueNoiseImages(VK_NULL_HANDLE),
    blueNoiseImagesView(VK_NULL_HANDLE),
    descSetLayout(VK_NULL_HANDLE),
    descPool(VK_NULL_HANDLE),
    descSet(VK_NULL_HANDLE)
{
    using namespace std::string_literals;

    // no compression
    const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const uint32_t bytesPerPixel = 4;

    const VkDeviceSize oneLayerSize = bytesPerPixel * BLUE_NOISE_TEXTURE_SIZE * BLUE_NOISE_TEXTURE_SIZE;
    const VkDeviceSize dataSize = oneLayerSize * BLUE_NOISE_TEXTURE_COUNT;


    ImageLoader imageLoader(std::move(_userFileLoad));
    ImageLoader::LayeredResultInfo resultInfo = {};
    bool loaded = imageLoader.LoadLayered(_blueNoiseFilePath, &resultInfo);

    if (!loaded)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Can't find blue noise file: "s + _blueNoiseFilePath);
    }

    if (resultInfo.baseSize.width != BLUE_NOISE_TEXTURE_SIZE || resultInfo.baseSize.height != BLUE_NOISE_TEXTURE_SIZE)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Blue noise image size must be " + std::to_string(BLUE_NOISE_TEXTURE_SIZE));
    }

    if (resultInfo.layerData.size() != BLUE_NOISE_TEXTURE_COUNT)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Blue noise image must have " + std::to_string(BLUE_NOISE_TEXTURE_COUNT) + " layers");
    }

    if (resultInfo.format != imageFormat)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Blue noise image must have R8G8B8A8_UNORM format");
    }

    // allocate buffer for all textures
    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = dataSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;


    void *mappedData = nullptr;
    VkBuffer stagingBuffer = allocator->CreateStagingSrcTextureBuffer(&stagingInfo, "Blue noise image VMA staging alloc", &mappedData);

    assert(stagingBuffer != VK_NULL_HANDLE);

    // load each texture and place it in staging buffer
    for (uint32_t i = 0; i < BLUE_NOISE_TEXTURE_COUNT; i++)
    {
        void *dst = static_cast<uint8_t*>(mappedData) + oneLayerSize * i;
        
        memcpy(dst, resultInfo.layerData[i], oneLayerSize);
    }

    imageLoader.FreeLoaded();


    // create image that contains all blue noise textures as layers
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = imageFormat;
    info.extent = { BLUE_NOISE_TEXTURE_SIZE, BLUE_NOISE_TEXTURE_SIZE, 1 };
    info.mipLevels = 1;
    info.arrayLayers = BLUE_NOISE_TEXTURE_COUNT;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    blueNoiseImages = allocator->CreateDstTextureImage(&info, "Blue noise image VMA alloc");

    SET_DEBUG_NAME(device, blueNoiseImages, VK_OBJECT_TYPE_IMAGE, "Blue noise Image");

    // copy from buffer to image
    VkCommandBuffer cmd = _cmdManager->StartGraphicsCmd();

    VkImageSubresourceRange allLayersRange = {};
    allLayersRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    allLayersRange.baseMipLevel = 0;
    allLayersRange.levelCount = 1;
    allLayersRange.baseArrayLayer = 0;
    allLayersRange.layerCount = BLUE_NOISE_TEXTURE_COUNT;

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
    copyInfo.imageSubresource.layerCount = BLUE_NOISE_TEXTURE_COUNT;

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
    viewInfo.subresourceRange.layerCount = BLUE_NOISE_TEXTURE_COUNT;

    VkResult r = vkCreateImageView(device, &viewInfo, nullptr, &blueNoiseImagesView);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, blueNoiseImagesView, VK_OBJECT_TYPE_IMAGE_VIEW, "Blue noise View");
    
    CreateDescriptors();
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

void BlueNoise::CreateDescriptors()
{
    VkResult r;

    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = BINDING_BLUE_NOISE;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout);
    VK_CHECKERROR(r);

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
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

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descSet;
    write.dstBinding = BINDING_BLUE_NOISE;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    SET_DEBUG_NAME(device, descSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Blue noise Desc set layout");
    SET_DEBUG_NAME(device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Blue noise Desc pool");
    SET_DEBUG_NAME(device, descSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Blue noise Desc set");
}
