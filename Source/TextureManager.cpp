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

#include "TextureManager.h"

#include <cmath>

#include "Const.h"
#include "Utils.h"
#include "TextureOverrides.h"

TextureManager::TextureManager(
    VkDevice device,
    std::shared_ptr<MemoryAllocator> memAllocator,
    std::shared_ptr<CommandBufferManager> &cmdManager,
    const char *defaultTexturesPath,
    const char *albedoAlphaPostfix,
    const char *normalMetallicPostfix, 
    const char *emissionRoughnessPostfix)
{
    this->device = device;
    this->memAllocator = memAllocator;
    this->defaultTexturesPath = defaultTexturesPath;
    this->albedoAlphaPostfix = albedoAlphaPostfix;
    this->normalMetallicPostfix = normalMetallicPostfix;
    this->emissionRoughnessPostfix = emissionRoughnessPostfix;

    imageLoader = std::make_shared<ImageLoader>();
    samplerMgr = std::make_shared<SamplerManager>(device);

    textures.resize(MAX_TEXTURE_COUNT);

    // submit cmd to create empty texture
    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();
    CreateEmptyTexture(cmd, 0);
    cmdManager->Submit(cmd);
    cmdManager->WaitGraphicsIdle();
}

TextureManager::~TextureManager()
{
    for (uint32_t i = 0; i < textures.size(); i++)
    {
        if (textures[i].image != VK_NULL_HANDLE)
        {
            DestroyTexture(i);
        }
    }
}

uint32_t TextureManager::GetMipmapCount(const RgExtent2D &size)
{
    auto widthCount = static_cast<uint32_t>(log2(size.width));
    auto heightCount = static_cast<uint32_t>(log2(size.height));

    return std::min(widthCount, heightCount) + 1;
}

void TextureManager::PrepareForFrame(uint32_t frameIndex)
{
    for (VkBuffer staging : stagingToFree[frameIndex])
    {
        memAllocator->DestroyStagingSrcTextureBuffer(staging);
    }
}

uint32_t TextureManager::CreateStaticMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgStaticMaterialCreateInfo &createInfo)
{
    ParseInfo parseInfo = {};
    parseInfo.texturesPath = defaultTexturesPath.c_str();
    parseInfo.albedoAlphaPostfix = albedoAlphaPostfix.c_str();
    parseInfo.normalMetallicPostfix = normalMetallicPostfix.c_str();
    parseInfo.emissionRoughnessPostfix = emissionRoughnessPostfix.c_str();

    // load additional textures, they'll be freed after leaving the scope
    TextureOverrides ovrd(createInfo, parseInfo, imageLoader);

    Material material = {};

    material.sampler = samplerMgr->GetSampler(createInfo.filter, createInfo.addressModeU, createInfo.addressModeV);

    material.textures.albedoAlpha       = PrepareStaticTexture(cmd, frameIndex, ovrd.aa, ovrd.aaSize, ovrd.debugName);
    material.textures.normalMetallic    = PrepareStaticTexture(cmd, frameIndex, ovrd.nm, ovrd.nmSize, ovrd.debugName);
    material.textures.emissionRoughness = PrepareStaticTexture(cmd, frameIndex, ovrd.er, ovrd.erSize, ovrd.debugName);

    uint32_t matIndex = material.textures.indices[0] + material.textures.indices[1] + material.textures.indices[2];

    while (materials.find(matIndex) != materials.end())
    {
        matIndex++;
    }

    materials[matIndex] = material;

    return matIndex;
}

void TextureManager::CreateEmptyTexture(VkCommandBuffer cmd, uint32_t frameIndex)
{
    assert(textures[0].image == VK_NULL_HANDLE && textures[0].view == VK_NULL_HANDLE);

    uint32_t data[] = { 0xFFFFFFFF };
    RgExtent2D size = { 1,1 };

    uint32_t textureIndex = PrepareStaticTexture(cmd, frameIndex, data, size, "Empty texture");
    assert(textureIndex == RG_NO_MATERIAL);
}

uint32_t TextureManager::PrepareStaticTexture(VkCommandBuffer cmd, uint32_t frameIndex, const void *data, const RgExtent2D &size, const char *debugName)
{
    VkResult r;

    // 1. Allocate and fill buffer

    const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
    const VkDeviceSize bytesPerPixel = 4;

    VkDeviceSize dataSize = bytesPerPixel * size.width * size.height;

    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = dataSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkDeviceMemory stagingMemory, imageMemory;
    void *mappedData;

    VkBuffer stagingBuffer = memAllocator->CreateStagingSrcTextureBuffer(&stagingInfo, &stagingMemory, &mappedData);
    if (stagingBuffer == VK_NULL_HANDLE)
    {
        return RG_NO_MATERIAL;
    }

    // copy image data to buffer
    memcpy(mappedData, data, dataSize);

    uint32_t mipmapCount = GetMipmapCount(size);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = imageFormat;
    imageInfo.extent = { size.width, size.height, 1 };
    imageInfo.mipLevels = mipmapCount;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkImage finalImage = memAllocator->CreateDstTextureImage(&imageInfo, &imageMemory);
    if (finalImage == VK_NULL_HANDLE)
    {
        memAllocator->DestroyStagingSrcTextureBuffer(stagingBuffer);

        return RG_NO_MATERIAL;
    }

    if (debugName != nullptr)
    {
        SET_DEBUG_NAME(device, finalImage, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, debugName);
    }

    // 2. Copy buffer data to the first mipmap

    VkImageSubresourceRange firstMipmap = {};
    firstMipmap.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    firstMipmap.baseMipLevel = 0;
    firstMipmap.levelCount = 1;
    firstMipmap.baseArrayLayer = 0;
    firstMipmap.layerCount = 1;

    // set layout for copying
    Utils::BarrierImage(
        cmd, finalImage,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        firstMipmap);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    // tigthly packed
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageExtent = { size.width, size.height, 1 };
    copyRegion.imageOffset = { 0,0,0 };
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;

    vkCmdCopyBufferToImage(
        cmd, stagingBuffer, finalImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // 3. Create mipmaps

    // first mipmap to TRANSFER_SRC to create mipmaps using blit
    Utils::BarrierImage(
        cmd, finalImage,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        firstMipmap);

    uint32_t mipWidth = size.width;
    uint32_t mipHeight = size.height;

    for (uint32_t mipLevel = 1; mipLevel < mipmapCount; mipLevel++)
    {
        uint32_t prevMipWidth = mipWidth;
        uint32_t prevMipHeight = mipHeight;

        mipWidth >>= 1;
        mipHeight >>= 1;

        assert(mipWidth > 0 && mipHeight > 0);
        assert(mipLevel != mipmapCount - 1 || (mipWidth == 1 || mipHeight == 1));

        VkImageSubresourceRange curMipmap = {};
        curMipmap.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        curMipmap.baseMipLevel = mipLevel;
        curMipmap.levelCount = 1;
        curMipmap.baseArrayLayer = 0;
        curMipmap.layerCount = 1;

        // current mip to TRANSFER_DST
        Utils::BarrierImage(
            cmd, finalImage,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            curMipmap);

        // blit from previous mip level
        VkImageBlit curBlit = {};

        curBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        curBlit.srcSubresource.mipLevel = mipLevel - 1;
        curBlit.srcSubresource.baseArrayLayer = 0;
        curBlit.srcSubresource.layerCount = 1;
        curBlit.srcOffsets[0] = { 0,0,0 };
        curBlit.srcOffsets[1] = { (int32_t)prevMipWidth, (int32_t)prevMipHeight, 1 };

        curBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        curBlit.dstSubresource.mipLevel = mipLevel;
        curBlit.dstSubresource.baseArrayLayer = 0;
        curBlit.dstSubresource.layerCount = 1;
        curBlit.dstOffsets[0] = { 0,0,0 };
        curBlit.dstOffsets[1] = { (int32_t)mipWidth, (int32_t)mipHeight, 1 };

        vkCmdBlitImage(
            cmd,
            finalImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            finalImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &curBlit, VK_FILTER_LINEAR);

        // current mip to TRANSFER_SRC for the next one
        Utils::BarrierImage(
            cmd, finalImage,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            curMipmap);
    }

    // 4. Prepare all mipmaps for reading in ray tracing and fragment shaders

    VkImageSubresourceRange allMipmaps = {};
    allMipmaps.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    allMipmaps.baseMipLevel = 0;
    allMipmaps.levelCount = mipmapCount;
    allMipmaps.baseArrayLayer = 0;
    allMipmaps.layerCount = 1;

    Utils::BarrierImage(
        cmd, finalImage,
        VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        allMipmaps);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = finalImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageFormat;
    viewInfo.components = {};
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipmapCount;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView finalImageView;
    r = vkCreateImageView(device, &viewInfo, nullptr, &finalImageView);
    VK_CHECKERROR(r);

    if (debugName != nullptr)
    {
        SET_DEBUG_NAME(device, finalImageView, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, debugName);
    }

    // push staging buffer to be deleted when it won't be in use
    stagingToFree[frameIndex].push_back(stagingBuffer);

    return InsertTexture(finalImage, finalImageView);
}

uint32_t TextureManager::CreateDynamicMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgDynamicMaterialCreateInfo &createInfo)
{

}

uint32_t TextureManager::CreateAnimatedMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgAnimatedMaterialCreateInfo &createInfo)
{

}

void TextureManager::DestroyMaterial(uint32_t materialIndex)
{
    auto it = materials.find(materialIndex);

    if (it == materials.end())
    {
        return;
    }

    Material &material = it->second;

    for (auto t : material.textures.indices)
    {
        DestroyTexture(t);
    }

    material.sampler = VK_NULL_HANDLE;
    material.textures = {};
}

uint32_t TextureManager::InsertTexture(VkImage image, VkImageView view)
{
    auto texture = std::find_if(textures.begin(), textures.end(), [] (const Texture &t)
    {
        return t.image == VK_NULL_HANDLE && t.view == VK_NULL_HANDLE;
    });

    texture->image = image;
    texture->view = view;

    uint32_t textureIndex = (uint32_t)std::distance(textures.begin(), texture);
    return textureIndex;
}

void TextureManager::DestroyTexture(uint32_t textureIndex)
{
    Texture &texture = textures[textureIndex];

    vkDestroyImage(device, texture.image, nullptr);
    vkDestroyImageView(device, texture.view, nullptr);

    texture.image = VK_NULL_HANDLE;
    texture.view = VK_NULL_HANDLE;
}

bool TextureManager::GetMaterialTextures(
    uint32_t materialIndex, uint32_t *outAATexture,
    uint32_t *outNMTexture, uint32_t *outERTexture) const
{
    auto it = materials.find(materialIndex);

    if (it == materials.end())
    {
        return false;
    }

    const Material &material = it->second;

    *outAATexture = material.textures.albedoAlpha;
    *outNMTexture = material.textures.normalMetallic;
    *outERTexture = material.textures.emissionRoughness;

    return true;
}
