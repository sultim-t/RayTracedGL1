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

#include <string>

#include "Common.h"
#include "CommandBufferManager.h"
#include "Material.h"
#include "ImageLoader.h"
#include "MemoryAllocator.h"
#include "SamplerManager.h"
#include "TextureDescriptors.h"
#include "TextureUploader.h"

class TextureManager
{
public:
    explicit TextureManager(
        VkDevice device,
        std::shared_ptr<MemoryAllocator> memAllocator,
        const std::shared_ptr<CommandBufferManager> &cmdManager,
        const char *defaultTexturesPath,
        const char *albedoAlphaPostfix,
        const char *normalMetallicPostfix,
        const char *emissionRoughnessPostfix);
    ~TextureManager();

    TextureManager(const TextureManager &other) = delete;
    TextureManager(TextureManager &&other) noexcept = delete;
    TextureManager &operator=(const TextureManager &other) = delete;
    TextureManager &operator=(TextureManager &&other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);

    uint32_t CreateStaticMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgStaticMaterialCreateInfo &createInfo);
    uint32_t CreateAnimatedMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgAnimatedMaterialCreateInfo &createInfo);
    uint32_t CreateDynamicMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgDynamicMaterialCreateInfo &createInfo);
    void ChangeAnimatedMaterialFrame(RgMaterial animMaterial, uint32_t materialFrame);

    void DestroyMaterial(uint32_t materialIndex);

    MaterialTextures GetMaterialTextures(uint32_t materialIndex) const;
    static uint32_t GetEmptyTextureIndex();

    VkDescriptorSet GetDescSet(uint32_t frameIndex) const;
    VkDescriptorSetLayout GetDescSetLayout() const;

private:
    void CreateEmptyTexture(VkCommandBuffer cmd, uint32_t frameIndex);

    uint32_t PrepareStaticTexture(
        VkCommandBuffer cmd, uint32_t frameIndex,
        const void *data, const RgExtent2D &size,
        VkSampler sampler,
        const char *debugName = nullptr);

    uint32_t InsertTexture(VkImage image, VkImageView view, VkSampler sampler);
    void DestroyTexture(uint32_t textureIndex);
    void DestroyTexture(Texture &texture);

    uint32_t GenerateMaterialIndex(const MaterialTextures &materialTextures);
    uint32_t GenerateMaterialIndex(const std::vector<uint32_t> &materialIndices);

    uint32_t InsertMaterial(const MaterialTextures &materialTextures);
    uint32_t InsertAnimatedMaterial(std::vector<uint32_t> &materialIndices);

    void DestroyMaterialTextures(uint32_t materialIndex);
    void DestroyMaterialTextures(const Material &material);

    void UpdateDescSet(uint32_t frameIndex);

private:
    VkDevice device;

    std::shared_ptr<ImageLoader> imageLoader;

    std::shared_ptr<SamplerManager> samplerMgr;
    std::shared_ptr<TextureDescriptors> textureDesc;
    std::shared_ptr<TextureUploader> textureUploader;

    std::vector<Texture> textures;
    std::map<uint32_t, AnimatedMaterial> animatedMaterials;
    std::map<uint32_t, Material> materials;

    std::string defaultTexturesPath;
    std::string albedoAlphaPostfix;
    std::string normalMetallicPostfix;
    std::string emissionRoughnessPostfix;
};