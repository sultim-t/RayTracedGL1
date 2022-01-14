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

#include <list>
#include <string>

#include "Common.h"
#include "CommandBufferManager.h"
#include "Material.h"
#include "ImageLoader.h"
#include "IMaterialDependency.h"
#include "MemoryAllocator.h"
#include "SamplerManager.h"
#include "TextureDescriptors.h"
#include "TextureUploader.h"

namespace RTGL1
{

class TextureManager
{
public:
    explicit TextureManager(
        VkDevice device,
        std::shared_ptr<MemoryAllocator> memAllocator,
        std::shared_ptr<SamplerManager> samplerManager,
        const std::shared_ptr<CommandBufferManager> &cmdManager,
        std::shared_ptr<UserFileLoad> userFileLoad,
        const RgInstanceCreateInfo &info);
    ~TextureManager();

    TextureManager(const TextureManager &other) = delete;
    TextureManager(TextureManager &&other) noexcept = delete;
    TextureManager &operator=(const TextureManager &other) = delete;
    TextureManager &operator=(TextureManager &&other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);
    void SubmitDescriptors(uint32_t frameIndex,
                           const RgDrawFrameTexturesParams *pTexturesParams,
                           bool forceUpdateAllDescriptors = false); // true, if mip lod bias was changed, for example

    uint32_t CreateStaticMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgStaticMaterialCreateInfo &createInfo);

    uint32_t CreateAnimatedMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgAnimatedMaterialCreateInfo &createInfo);
    bool ChangeAnimatedMaterialFrame(uint32_t animMaterial, uint32_t materialFrame);

    uint32_t CreateDynamicMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgDynamicMaterialCreateInfo &createInfo);
    bool UpdateDynamicMaterial(VkCommandBuffer cmd, const RgDynamicMaterialUpdateInfo &updateInfo);

    void DestroyMaterial(uint32_t currentFrameIndex, uint32_t materialIndex);

    MaterialTextures GetMaterialTextures(uint32_t materialIndex) const;

    static constexpr uint32_t GetEmptyTextureIndex();
    uint32_t GetWaterNormalTextureIndex() const;

    VkDescriptorSet GetDescSet(uint32_t frameIndex) const;
    VkDescriptorSetLayout GetDescSetLayout() const;

    // Subscribe to material change event.
    // shared_ptr will be transformed to weak_ptr
    void Subscribe(std::shared_ptr<IMaterialDependency> subscriber);
    void Unsubscribe(const IMaterialDependency *subscriber);

private:
    void CreateEmptyTexture(VkCommandBuffer cmd, uint32_t frameIndex);
    void CreateWaterNormalTexture(VkCommandBuffer cmd, uint32_t frameIndex, const char *pFilePath);

    uint32_t PrepareStaticTexture(
        VkCommandBuffer cmd, uint32_t frameIndex, const ImageLoader::ResultInfo &info,
        SamplerManager::Handle samplerHandle, bool useMipmaps, const char *debugName);

    uint32_t PrepareDynamicTexture(
        VkCommandBuffer cmd, uint32_t frameIndex, const void *data, uint32_t dataSize, const RgExtent2D &size,
        SamplerManager::Handle samplerHandle, VkFormat format, bool generateMipmaps, const char *debugName);

    uint32_t InsertTexture(uint32_t frameIndex, VkImage image, VkImageView view, SamplerManager::Handle samplerHandle);
    void DestroyTexture(const Texture &texture);
    void AddToBeDestroyed(uint32_t frameIndex, const Texture &texture);

    uint32_t GenerateMaterialIndex(const MaterialTextures &materialTextures);
    uint32_t GenerateMaterialIndex(const std::vector<uint32_t> &materialIndices);

    uint32_t InsertMaterial(const MaterialTextures &materialTextures, bool isDynamic);
    uint32_t InsertAnimatedMaterial(std::vector<uint32_t> &materialIndices);

    void DestroyMaterialTextures(uint32_t frameIndex, uint32_t materialIndex);
    void DestroyMaterialTextures(uint32_t frameIndex, const Material &material);

private:
    VkDevice device;

    std::shared_ptr<ImageLoader> imageLoader;

    std::shared_ptr<SamplerManager> samplerMgr;
    std::shared_ptr<TextureDescriptors> textureDesc;
    std::shared_ptr<TextureUploader> textureUploader;

    std::vector<Texture> textures;
    // Textures are not destroyed immediately, but when
    // they won't be in use
    std::vector<Texture> texturesToDestroy[MAX_FRAMES_IN_FLIGHT];

    std::unordered_map<uint32_t, AnimatedMaterial> animatedMaterials;
    std::unordered_map<uint32_t, Material> materials;

    uint32_t waterNormalTextureIndex;

    RgSamplerFilter currentDynamicSamplerFilter;

    std::string defaultTexturesPath;
    std::string postfixes[TEXTURES_PER_MATERIAL_COUNT];
    bool overridenIsSRGB[TEXTURES_PER_MATERIAL_COUNT];

    std::list<std::weak_ptr<IMaterialDependency>> subscribers;
};

inline constexpr uint32_t TextureManager::GetEmptyTextureIndex()
{
    return EMPTY_TEXTURE_INDEX;
}

}