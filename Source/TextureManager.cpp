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

#define EMPTY_TEXTURE_INDEX 0

#define DEFAULT_TEXTURES_PATH               ""
#define DEFAULT_ALBEDO_ALPHA_POSTFIX        ""
#define DEFAULT_NORMAL_METALLIC_POSTFIX     "_n"
#define DEFAULT_EMISSION_ROUGHNESS_POSTFIX  "_e"

TextureManager::TextureManager(
    VkDevice _device,
    std::shared_ptr<MemoryAllocator> _memAllocator,
    std::shared_ptr<CommandBufferManager> &_cmdManager,
    const char *_defaultTexturesPath,
    const char *_albedoAlphaPostfix,
    const char *_normalMetallicPostfix,
    const char *_emissionRoughnessPostfix)
:
    device(_device),
    memAllocator(_memAllocator)
{
    this->defaultTexturesPath = _defaultTexturesPath != nullptr ? _defaultTexturesPath : DEFAULT_TEXTURES_PATH;
    this->albedoAlphaPostfix = _albedoAlphaPostfix != nullptr ? _albedoAlphaPostfix : DEFAULT_ALBEDO_ALPHA_POSTFIX;
    this->normalMetallicPostfix = _normalMetallicPostfix != nullptr ? _normalMetallicPostfix : DEFAULT_NORMAL_METALLIC_POSTFIX;
    this->emissionRoughnessPostfix = _emissionRoughnessPostfix != nullptr ? _emissionRoughnessPostfix : DEFAULT_EMISSION_ROUGHNESS_POSTFIX;

    imageLoader = std::make_shared<ImageLoader>();
    samplerMgr = std::make_shared<SamplerManager>(device);
    textureDesc = std::make_shared<TextureDescriptors>(device);

    textures.resize(MAX_TEXTURE_COUNT);

    // submit cmd to create empty texture
    VkCommandBuffer cmd = _cmdManager->StartGraphicsCmd();
    CreateEmptyTexture(cmd, 0);
    _cmdManager->Submit(cmd);
    _cmdManager->WaitGraphicsIdle();
}

TextureManager::~TextureManager()
{
    imageLoader.reset();
    samplerMgr.reset();
    textureDesc.reset();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        for (VkBuffer staging : stagingToFree[i])
        {
            memAllocator->DestroyStagingSrcTextureBuffer(staging);
        }
    }

    for (auto &texture : textures)
    {
        assert((texture.image == VK_NULL_HANDLE && texture.view == VK_NULL_HANDLE) ||
               (texture.image != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE));

        if (texture.image != VK_NULL_HANDLE)
        {
            DestroyTexture(texture);
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
    // clear unused staging
    for (VkBuffer staging : stagingToFree[frameIndex])
    {
        memAllocator->DestroyStagingSrcTextureBuffer(staging);
    }

    stagingToFree[frameIndex].clear();

    // update desc set with current values
    UpdateDescSet(frameIndex);
}

void TextureManager::UpdateDescSet(uint32_t frameIndex)
{
    for (uint32_t i = 0; i < textures.size(); i++)
    {
        if (textures[i].image != VK_NULL_HANDLE)
        {
            textureDesc->UpdateTextureDesc(frameIndex, i, textures[i].view, textures[i].sampler);
        }
        else
        {
            // reset descriptor to empty texture
            textureDesc->ResetTextureDesc(frameIndex, i);
        }
    }

    textureDesc->FlushDescWrites();
}

uint32_t TextureManager::CreateStaticMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgStaticMaterialCreateInfo &createInfo)
{
    MaterialTextures material = {};
    VkSampler sampler = samplerMgr->GetSampler(createInfo.filter, createInfo.addressModeU, createInfo.addressModeV);

    if (!createInfo.disableOverride)
    {
        ParseInfo parseInfo = {};
        parseInfo.texturesPath = defaultTexturesPath.c_str();
        parseInfo.albedoAlphaPostfix = albedoAlphaPostfix.c_str();
        parseInfo.normalMetallicPostfix = normalMetallicPostfix.c_str();
        parseInfo.emissionRoughnessPostfix = emissionRoughnessPostfix.c_str();

        // load additional textures, they'll be freed after leaving the scope
        TextureOverrides ovrd(createInfo, parseInfo, imageLoader);

        material.albedoAlpha = PrepareStaticTexture(cmd, frameIndex, ovrd.aa, ovrd.aaSize, sampler, ovrd.debugName);
        material.normalMetallic = PrepareStaticTexture(cmd, frameIndex, ovrd.nm, ovrd.nmSize, sampler, ovrd.debugName);
        material.emissionRoughness = PrepareStaticTexture(cmd, frameIndex, ovrd.er, ovrd.erSize, sampler, ovrd.debugName);
    }
    else
    {
        material.albedoAlpha = PrepareStaticTexture(cmd, frameIndex, createInfo.data, createInfo.size, sampler, createInfo.relativePath);
        material.normalMetallic = EMPTY_TEXTURE_INDEX;
        material.emissionRoughness = EMPTY_TEXTURE_INDEX;
    }

    return InsertMaterial(material);
}

void TextureManager::CreateEmptyTexture(VkCommandBuffer cmd, uint32_t frameIndex)
{
    assert(textures[0].image == VK_NULL_HANDLE && textures[0].view == VK_NULL_HANDLE);

    uint32_t data[] = { 0xFFFFFFFF };
    RgExtent2D size = { 1,1 };
    VkSampler sampler = samplerMgr->GetSampler(RG_SAMPLER_FILTER_NEAREST, RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_REPEAT);

    uint32_t textureIndex = PrepareStaticTexture(cmd, frameIndex, data, size, sampler, "Empty texture");

    // must have specific index
    assert(textureIndex == EMPTY_TEXTURE_INDEX);

    VkImage emptyImage = textures[textureIndex].image;
    VkImageView emptyView = textures[textureIndex].view;

    assert(emptyImage != VK_NULL_HANDLE && emptyView != VK_NULL_HANDLE);

    // if texture will be reset, it will use empty texture's info
    textureDesc->SetEmptyTextureInfo(emptyView, sampler);
}

uint32_t TextureManager::PrepareStaticTexture(VkCommandBuffer cmd, uint32_t frameIndex, const void *data, const RgExtent2D &size, VkSampler sampler, const char *debugName)
{
    if (data == nullptr)
    {
        return EMPTY_TEXTURE_INDEX;
    }

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
        return EMPTY_TEXTURE_INDEX;
    }

    if (debugName != nullptr)
    {
        SET_DEBUG_NAME(device, stagingBuffer, VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT, debugName);
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

        return EMPTY_TEXTURE_INDEX;
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

    return InsertTexture(finalImage, finalImageView, sampler);
}

uint32_t TextureManager::CreateDynamicMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgDynamicMaterialCreateInfo &createInfo)
{
    assert(0);
    return RG_NO_MATERIAL;
}

uint32_t TextureManager::CreateAnimatedMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgAnimatedMaterialCreateInfo &createInfo)
{
    assert(0);
    return RG_NO_MATERIAL;
}

uint32_t TextureManager::InsertMaterial(const MaterialTextures &materialTextures)
{
    bool isEmpty = true;

    for (uint32_t t : materialTextures.indices)
    {
        if (t != EMPTY_TEXTURE_INDEX)
        {
            isEmpty = false;
        }
    }

    if (isEmpty)
    {
        return RG_NO_MATERIAL;
    }

    uint32_t matIndex = materialTextures.indices[0] + materialTextures.indices[1] + materialTextures.indices[2];

    while (materials.find(matIndex) != materials.end())
    {
        matIndex++;
    }

    Material material = {};
    material.index = matIndex;
    material.textures = materialTextures;

    materials[matIndex] = material;
    return matIndex;
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
        if (t != EMPTY_TEXTURE_INDEX)
        {
            DestroyTexture(t);
        }
    }

    material.index = 0;
    material.textures = {};
}

uint32_t TextureManager::InsertTexture(VkImage image, VkImageView view, VkSampler sampler)
{
    auto texture = std::find_if(textures.begin(), textures.end(), [] (const Texture &t)
    {
        return t.image == VK_NULL_HANDLE && t.view == VK_NULL_HANDLE;
    });

    texture->image = image;
    texture->view = view;
    texture->sampler = sampler;

    return (uint32_t)std::distance(textures.begin(), texture);
}

void TextureManager::DestroyTexture(uint32_t textureIndex)
{
    DestroyTexture(textures[textureIndex]);
}

void TextureManager::DestroyTexture(Texture &texture)
{
    assert(texture.image != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE);

    memAllocator->DestroyTextureImage(texture.image);
    vkDestroyImageView(device, texture.view, nullptr);
 
    texture.image = VK_NULL_HANDLE;
    texture.view = VK_NULL_HANDLE;
    texture.sampler = VK_NULL_HANDLE;
}

MaterialTextures TextureManager::GetMaterialTextures(uint32_t materialIndex) const
{
    const auto it = materials.find(materialIndex);

    if (it == materials.end())
    {
        MaterialTextures empty = {};
        empty.albedoAlpha = EMPTY_TEXTURE_INDEX;
        empty.normalMetallic = EMPTY_TEXTURE_INDEX;
        empty.emissionRoughness = EMPTY_TEXTURE_INDEX;

        return empty;
    }

    const Material &material = it->second;

    return material.textures;
}

uint32_t TextureManager::GetEmptyTextureIndex()
{
    return EMPTY_TEXTURE_INDEX;
}

VkDescriptorSet TextureManager::GetDescSet(uint32_t frameIndex) const
{
    return textureDesc->GetDescSet(frameIndex);
}

VkDescriptorSetLayout TextureManager::GetDescSetLayout() const
{
    return textureDesc->GetDescSetLayout();
}
