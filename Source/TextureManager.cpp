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

#include <numeric>
#include <algorithm>

#include "Const.h"
#include "Utils.h"
#include "TextureOverrides.h"
#include "Generated/ShaderCommonC.h"
#include "RgException.h"


using namespace RTGL1;
static_assert(TEXTURES_PER_MATERIAL_COUNT == sizeof(RgTextureSet) / sizeof(RgTextureData), "TEXTURES_PER_MATERIAL_COUNT must be same as in RgTextureSet");

constexpr MaterialTextures EmptyMaterialTextures = { EMPTY_TEXTURE_INDEX, EMPTY_TEXTURE_INDEX,EMPTY_TEXTURE_INDEX };

constexpr RgSamplerFilter DefaultDynamicSamplerFilter = RG_SAMPLER_FILTER_LINEAR;


TextureManager::TextureManager(
    VkDevice _device,
    std::shared_ptr<MemoryAllocator> _memAllocator,
    std::shared_ptr<SamplerManager> _samplerMgr,
    const std::shared_ptr<CommandBufferManager> &_cmdManager,
    std::shared_ptr<UserFileLoad> _userFileLoad,
    const RgInstanceCreateInfo &_info)
:
    device(_device),
    samplerMgr(std::move(_samplerMgr)),
    currentDynamicSamplerFilter(DefaultDynamicSamplerFilter)
{
    this->defaultTexturesPath = _info.pOverridenTexturesFolderPath != nullptr ? _info.pOverridenTexturesFolderPath : DEFAULT_TEXTURES_PATH;

    const char *userPostfixes[TEXTURES_PER_MATERIAL_COUNT] =
    {
        _info.pOverridenAlbedoAlphaTexturePostfix,
        _info.pOverridenRoughnessMetallicEmissionTexturePostfix,
        _info.pOverridenNormalTexturePostfix,
    };

    bool userOverridenIsSRGB[TEXTURES_PER_MATERIAL_COUNT] =
    {
        !!_info.overridenAlbedoAlphaTextureIsSRGB,
        !!_info.overridenRoughnessMetallicEmissionTextureIsSRGB,
        !!_info.overridenNormalTextureIsSRGB
    };

    for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
    {
        this->postfixes[i] = userPostfixes[i] != nullptr ? userPostfixes[i] : DEFAULT_TEXTURES_POSTFIXES[i];
        this->overridenIsSRGB[i] = userOverridenIsSRGB[i];
    }

    const uint32_t maxTextureCount = std::max<uint32_t>(TEXTURE_COUNT_MIN, std::min<uint32_t>(_info.maxTextureCount, TEXTURE_COUNT_MAX));

    imageLoader = std::make_shared<ImageLoader>(std::move(_userFileLoad));
    textureDesc = std::make_shared<TextureDescriptors>(device, samplerMgr, maxTextureCount, BINDING_TEXTURES);
    textureUploader = std::make_shared<TextureUploader>(device, std::move(_memAllocator));

    textures.resize(maxTextureCount);

    // submit cmd to create empty texture
    VkCommandBuffer cmd = _cmdManager->StartGraphicsCmd();
    CreateEmptyTexture(cmd, 0);
    CreateWaterNormalTexture(cmd, 0, _info.pWaterNormalTexturePath);
    _cmdManager->Submit(cmd);
    _cmdManager->WaitGraphicsIdle();

    if (this->waterNormalTextureIndex == EMPTY_TEXTURE_INDEX)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Couldn't create water normal texture with path: " + std::string(_info.pWaterNormalTexturePath));
    }
}

void TextureManager::CreateEmptyTexture(VkCommandBuffer cmd, uint32_t frameIndex)
{
    assert(textures[EMPTY_TEXTURE_INDEX].image == VK_NULL_HANDLE && textures[EMPTY_TEXTURE_INDEX].view == VK_NULL_HANDLE);

    const uint32_t data[] = { 0xFFFFFFFF };
    const RgExtent2D size = { 1,1 };

    ImageLoader::ResultInfo info = {};
    info.pData = reinterpret_cast<const uint8_t*>(data);
    info.dataSize = sizeof(data);
    info.baseSize = size;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.levelCount = 1;
    info.isPregenerated = false;
    info.levelSizes[0] = sizeof(data);

    SamplerManager::Handle samplerHandle(RG_SAMPLER_FILTER_NEAREST, RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_REPEAT, 0);

    uint32_t textureIndex = PrepareStaticTexture(cmd, frameIndex, info, samplerHandle, false, "Empty texture");

    // must have specific index
    assert(textureIndex == EMPTY_TEXTURE_INDEX);

    VkImage emptyImage = textures[textureIndex].image;
    VkImageView emptyView = textures[textureIndex].view;

    assert(emptyImage != VK_NULL_HANDLE && emptyView != VK_NULL_HANDLE);

    // if texture will be reset, it will use empty texture's info
    textureDesc->SetEmptyTextureInfo(emptyView);
}

// Check CreateStaticMaterial for notes
void RTGL1::TextureManager::CreateWaterNormalTexture(VkCommandBuffer cmd, uint32_t frameIndex, const char *pFilePath)
{
    SamplerManager::Handle samplerHandle(RG_SAMPLER_FILTER_LINEAR, RG_SAMPLER_ADDRESS_MODE_REPEAT, RG_SAMPLER_ADDRESS_MODE_REPEAT, 0);

    TextureOverrides::OverrideInfo parseInfo = {};
    parseInfo.disableOverride = false;
    // use absolute path
    parseInfo.texturesPath = "";
    for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
    {
        parseInfo.postfixes[i] = "";
        parseInfo.overridenIsSRGB[i] = false;
    }

    const uint32_t defaultData[] = { 0x7F7FFFFF };
    const RgExtent2D defaultSize = { 1, 1 };
    // try to load image file
    TextureOverrides ovrd(pFilePath, defaultData, false, defaultSize, parseInfo, imageLoader);

    this->waterNormalTextureIndex = PrepareStaticTexture(cmd, frameIndex, ovrd.GetResult(0), samplerHandle, true, "Water normal");
}

TextureManager::~TextureManager()
{
    for (auto &texture : textures)
    {
        assert((texture.image == VK_NULL_HANDLE && texture.view == VK_NULL_HANDLE) ||
               (texture.image != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE));

        if (texture.image != VK_NULL_HANDLE)
        {
            DestroyTexture(texture);
        }
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        for (auto &texture : texturesToDestroy[i])
        {
            DestroyTexture(texture);
        }
    }
}

void TextureManager::PrepareForFrame(uint32_t frameIndex)
{
    // destroy delayed textures
    for (auto &texture : texturesToDestroy[frameIndex])
    {
        DestroyTexture(texture);
    }
    texturesToDestroy[frameIndex].clear();

    // clear staging buffer that are not in use
    textureUploader->ClearStaging(frameIndex);
}

void TextureManager::SubmitDescriptors(uint32_t frameIndex, 
                                       const RgDrawFrameTexturesParams *pTexturesParams,
                                       bool forceUpdateAllDescriptors)
{
    // check if dynamic sampler filter was changed
    RgSamplerFilter newDynamicSamplerFilter = pTexturesParams != nullptr ?
        pTexturesParams->dynamicSamplerFilter : DefaultDynamicSamplerFilter;

    if (currentDynamicSamplerFilter != newDynamicSamplerFilter)
    {
        currentDynamicSamplerFilter = newDynamicSamplerFilter;
        forceUpdateAllDescriptors = true;
    }


    if (forceUpdateAllDescriptors)
    {
        textureDesc->ResetAllCache(frameIndex);
    }

    // update desc set with current values
    for (uint32_t i = 0; i < textures.size(); i++)
    {
        textures[i].samplerHandle.SetIfHasDynamicSamplerFilter(newDynamicSamplerFilter);


        if (textures[i].image != VK_NULL_HANDLE)
        {
            textureDesc->UpdateTextureDesc(frameIndex, i, textures[i].view, textures[i].samplerHandle);
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
    if (createInfo.pRelativePath == nullptr && 
        createInfo.textures.albedoAlpha.pData == nullptr &&
        createInfo.textures.roughnessMetallicEmission.pData == nullptr &&
        createInfo.textures.normal.pData == nullptr)
    {
        throw RgException(RG_WRONG_MATERIAL_PARAMETER, "At least one of \'pRelativePath\' or \'textures\' members must be not null");
    }

    SamplerManager::Handle samplerHandle(createInfo.filter, createInfo.addressModeU, createInfo.addressModeV, createInfo.flags);

    TextureOverrides::OverrideInfo parseInfo = {};
    parseInfo.disableOverride = createInfo.flags & RG_MATERIAL_CREATE_DISABLE_OVERRIDE_BIT;
    parseInfo.texturesPath = defaultTexturesPath.c_str();
    for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
    {
        parseInfo.postfixes[i] = postfixes[i].c_str();
        parseInfo.overridenIsSRGB[i] = overridenIsSRGB[i];
    }

    // load additional textures, they'll be freed after leaving the scope
    TextureOverrides ovrd(createInfo.pRelativePath, createInfo.textures, createInfo.size, parseInfo, imageLoader);


    MaterialTextures mtextures = {};

    for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
    {
        mtextures.indices[i] = PrepareStaticTexture(cmd, frameIndex, ovrd.GetResult(i), samplerHandle,
                                                   !(createInfo.flags & RG_MATERIAL_CREATE_DONT_GENERATE_MIPMAPS_BIT), ovrd.GetDebugName());
    }


    return InsertMaterial(mtextures, false);
}

uint32_t TextureManager::CreateDynamicMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgDynamicMaterialCreateInfo &createInfo)
{
    SamplerManager::Handle samplerHandle(createInfo.filter, createInfo.addressModeU, createInfo.addressModeV, createInfo.flags);

    const RgTextureData *tds[TEXTURES_PER_MATERIAL_COUNT] =
    { 
        &createInfo.textures.albedoAlpha,
        &createInfo.textures.roughnessMetallicEmission,
        &createInfo.textures.normal,
    };

    const uint32_t bytesPerPixel = 4;
    const uint32_t dataSize = createInfo.size.width * createInfo.size.height * bytesPerPixel;
    assert(dataSize > 0);


    MaterialTextures mtextures = {};

    for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
    {
        VkFormat format = tds[i]->isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        mtextures.indices[i] = PrepareDynamicTexture(cmd, frameIndex, tds[i]->pData, dataSize, createInfo.size, samplerHandle, format,
                                                    !(createInfo.flags & RG_MATERIAL_CREATE_DONT_GENERATE_MIPMAPS_BIT), nullptr);
    }


    return InsertMaterial(mtextures, true);
}

bool TextureManager::UpdateDynamicMaterial(VkCommandBuffer cmd, const RgDynamicMaterialUpdateInfo &updateInfo)
{
    const auto it = materials.find(updateInfo.dynamicMaterial);

    // if exist and dynamic
    if (it != materials.end())
    {
        if (!it->second.isDynamic)
        {
            throw RgException(RG_CANT_UPDATE_DYNAMIC_MATERIAL,
                              "Material with ID=" + std::to_string(updateInfo.dynamicMaterial) + " is not dynamic");
        }

        const void *updateData[TEXTURES_PER_MATERIAL_COUNT] = 
        {
            updateInfo.textures.albedoAlpha.pData,
            updateInfo.textures.roughnessMetallicEmission.pData,
            updateInfo.textures.normal.pData,
        };

        auto &textureIndices = it->second.textures.indices;
        static_assert(sizeof(textureIndices) / sizeof(textureIndices[0]) == TEXTURES_PER_MATERIAL_COUNT, "");

        bool wasUpdated = false;

        for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
        {
            uint32_t textureIndex = textureIndices[i];

            if (textureIndex == EMPTY_TEXTURE_INDEX)
            {
                continue;
            }

            VkImage img = textures[textureIndex].image;

            if (img == VK_NULL_HANDLE)
            {
                continue;
            }

            textureUploader->UpdateDynamicImage(cmd, img, updateData[i]);
            wasUpdated = true;
        }

        return wasUpdated;
    }
    else
    {
        throw RgException(RG_CANT_UPDATE_DYNAMIC_MATERIAL, 
                          "Material with ID=" + std::to_string(updateInfo.dynamicMaterial) +  " was not created");
    }

    return false;
}

uint32_t TextureManager::PrepareStaticTexture(
    VkCommandBuffer cmd, uint32_t frameIndex, 
    const ImageLoader::ResultInfo &imageInfo,
    SamplerManager::Handle samplerHandle, bool useMipmaps,
    const char *debugName)
{
    // only dynamic textures can have null data
    if (imageInfo.pData == nullptr)
    {
        return EMPTY_TEXTURE_INDEX;
    }

    if (imageInfo.baseSize.width == 0 || imageInfo.baseSize.height == 0)
    {
        using namespace std::string_literals;

        throw RgException(RG_WRONG_MATERIAL_PARAMETER, "Incorrect size (" + 
                          std::to_string(imageInfo.baseSize.width) + ", " + 
                          std::to_string(imageInfo.baseSize.height) + ") of one of images in a material" +
                          (debugName != nullptr ? " with name: "s + debugName : ""s));
    }

    assert(imageInfo.dataSize > 0);
    assert(imageInfo.levelCount > 0 && imageInfo.levelSizes[0] > 0);

    TextureUploader::UploadInfo info = {};
    info.cmd = cmd;
    info.frameIndex = frameIndex;
    info.pData = imageInfo.pData;
    info.dataSize = imageInfo.dataSize;
    info.baseSize = imageInfo.baseSize;
    info.format = imageInfo.format;
    info.isDynamic = false;
    info.useMipmaps = useMipmaps;
    info.pDebugName = debugName;
    info.isCubemap = false;
    info.pregeneratedLevelCount = imageInfo.isPregenerated ? imageInfo.levelCount : 0;
    info.pLevelDataOffsets = imageInfo.levelOffsets;
    info.pLevelDataSizes = imageInfo.levelSizes;

    auto result = textureUploader->UploadImage(info);

    if (!result.wasUploaded)
    {
        return EMPTY_TEXTURE_INDEX;
    }

    return InsertTexture(frameIndex, result.image, result.view, samplerHandle);
}

uint32_t TextureManager::PrepareDynamicTexture(
    VkCommandBuffer cmd, uint32_t frameIndex,
    const void *data, uint32_t dataSize, const RgExtent2D &size,
    SamplerManager::Handle samplerHandle, VkFormat format, bool generateMipmaps,
    const char *debugName)
{
    if (data == nullptr)
    {
        return EMPTY_TEXTURE_INDEX;
    }

    assert(size.width > 0 && size.height > 0);

    TextureUploader::UploadInfo info = {};
    info.cmd = cmd;
    info.frameIndex = frameIndex;
    info.pData = data;
    info.dataSize = dataSize;
    info.baseSize = size;
    info.format = format;
    info.isDynamic = true;
    info.useMipmaps = generateMipmaps;
    info.pDebugName = debugName;
    info.isCubemap = false;

    auto result = textureUploader->UploadImage(info);

    if (!result.wasUploaded)
    {
        return EMPTY_TEXTURE_INDEX;
    }

    return InsertTexture(frameIndex, result.image, result.view, samplerHandle);
}

uint32_t TextureManager::CreateAnimatedMaterial(VkCommandBuffer cmd, uint32_t frameIndex, const RgAnimatedMaterialCreateInfo &createInfo)
{
    if (createInfo.frameCount == 0)
    {
        return RG_NO_MATERIAL;
    }

    std::vector<uint32_t> materialIndices(createInfo.frameCount);

    // animated material is a series of static materials
    for (uint32_t i = 0; i < createInfo.frameCount; i++)
    {
        materialIndices[i] = CreateStaticMaterial(cmd, frameIndex, createInfo.pFrames[i]);
    }

    return InsertAnimatedMaterial(materialIndices);
}

bool TextureManager::ChangeAnimatedMaterialFrame(uint32_t animMaterial, uint32_t materialFrame)
{
    const auto animIt = animatedMaterials.find(animMaterial);

    if (animIt != animatedMaterials.end())
    {
        AnimatedMaterial &anim = animIt->second;

        uint32_t maxFrameCount = (uint32_t)anim.materialIndices.size();
        if (materialFrame >= maxFrameCount)
        {
            throw RgException(RG_CANT_UPDATE_ANIMATED_MATERIAL, 
                              "Animated material with ID=" + std::to_string(animMaterial) + " has only " +
                              std::to_string(maxFrameCount) + " frames, but frame with index " 
                              + std::to_string(materialFrame) + " was requested");
        }

        anim.currentFrame = materialFrame;

        // notify subscribers
        for (auto &ws : subscribers)
        {
            // if subscriber still exist
            if (auto s = ws.lock())
            {
                uint32_t frameMatIndex = anim.materialIndices[anim.currentFrame];

                // find MaterialTextures
                auto it = materials.find(frameMatIndex);

                if (it != materials.end())
                {
                    s->OnMaterialChange(animMaterial, it->second.textures);
                }
            }
        }

        return true;
    }
    else
    {
        throw RgException(RG_CANT_UPDATE_ANIMATED_MATERIAL, "Material with ID=" + std::to_string(animMaterial) + " is not animated");
    }

    return false;
}

uint32_t TextureManager::GenerateMaterialIndex(const MaterialTextures &materialTextures)
{
    uint32_t matIndex = materialTextures.indices[0] + materialTextures.indices[1] + materialTextures.indices[2];

    while (materials.find(matIndex) != materials.end())
    {
        matIndex++;
    }

    return matIndex;
}

uint32_t TextureManager::GenerateMaterialIndex(const std::vector<uint32_t> &materialIndices)
{
    uint32_t matIndex = std::accumulate(materialIndices.begin(), materialIndices.end(), 0u);

    // all materials share the same pool of indices
    while (materials.find(matIndex) != materials.end())
    {
        matIndex++;
    }

    return matIndex;
}

uint32_t TextureManager::InsertMaterial(const MaterialTextures &materialTextures, bool isDynamic)
{
    bool isEmpty = true;

    for (uint32_t t : materialTextures.indices)
    {
        if (t != EMPTY_TEXTURE_INDEX)
        {
            isEmpty = false;
            break;
        }
    }

    if (isEmpty)
    {
        return RG_NO_MATERIAL;
    }

    uint32_t matIndex = GenerateMaterialIndex(materialTextures);

    Material material = {};
    material.isDynamic = isDynamic;
    material.textures = materialTextures;

    materials[matIndex] = material;
    return matIndex;
}

uint32_t TextureManager::InsertAnimatedMaterial(std::vector<uint32_t> &materialIndices)
{
    bool isEmpty = true;

    for (uint32_t m : materialIndices)
    {
        if (m != RG_NO_MATERIAL)
        {
            isEmpty = false;
            break;
        }    
    }

    if (isEmpty)
    {
        return RG_NO_MATERIAL;
    }

    uint32_t animMatIndex = GenerateMaterialIndex(materialIndices);

    animatedMaterials[animMatIndex] = {};

    AnimatedMaterial &animMat = animatedMaterials[animMatIndex];
    animMat.currentFrame = 0;
    animMat.materialIndices = std::move(materialIndices);

    return animMatIndex;
}

void TextureManager::DestroyMaterialTextures(uint32_t frameIndex, uint32_t materialIndex)
{
    auto it = materials.find(materialIndex);

    if (it != materials.end())
    {
        DestroyMaterialTextures(frameIndex, it->second);
    }
}

void TextureManager::DestroyMaterialTextures(uint32_t frameIndex, const Material &material)
{
    for (auto t : material.textures.indices)
    {
        if (t != EMPTY_TEXTURE_INDEX)
        {
            Texture &texture = textures[t];

            AddToBeDestroyed(frameIndex, texture);

            // null data
            texture.image = VK_NULL_HANDLE;
            texture.view = VK_NULL_HANDLE;
            texture.samplerHandle = SamplerManager::Handle();
        }
    }
}

void TextureManager::DestroyMaterial(uint32_t currentFrameIndex, uint32_t materialIndex)
{
    if (materialIndex == RG_NO_MATERIAL)
    {
        return;
    }

    const auto animIt = animatedMaterials.find(materialIndex);

    // if it's an animated material
    if (animIt != animatedMaterials.end())
    {
        AnimatedMaterial &anim = animIt->second;

        // destroy each material
        for (auto &mat : anim.materialIndices)
        {
            DestroyMaterialTextures(currentFrameIndex, mat);
        }

        animatedMaterials.erase(animIt);
    }
    else
    {
        auto it = materials.find(materialIndex);

        if (it != materials.end())
        {
            DestroyMaterialTextures(currentFrameIndex, it->second);
            materials.erase(it);
        }
    }

    // notify subscribers
    for (auto &ws : subscribers)
    {
        if (auto s = ws.lock())
        {
            // send them empty texture indices as material is destroyed
            s->OnMaterialChange(materialIndex, EmptyMaterialTextures);
        }
    }
}

uint32_t TextureManager::InsertTexture(uint32_t frameIndex, VkImage image, VkImageView view, SamplerManager::Handle samplerHandle)
{
    auto texture = std::find_if(textures.begin(), textures.end(), [] (const Texture &t)
    {
        return t.image == VK_NULL_HANDLE && t.view == VK_NULL_HANDLE;
    });

    // if coudn't find empty space, use empty texture
    if (texture == textures.end())
    {
        // clean created data
        Texture t = {};
        t.image = image;
        t.view = view;
        AddToBeDestroyed(frameIndex, t);

        // TODO: properly warn user, add severity to print
        assert(false && "Too many textures");

        return EMPTY_TEXTURE_INDEX;
    }

    texture->image = image;
    texture->view = view;
    texture->samplerHandle = samplerHandle;

    return (uint32_t)std::distance(textures.begin(), texture);
}

void TextureManager::DestroyTexture(const Texture &texture)
{
    assert(texture.image != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE);
    textureUploader->DestroyImage(texture.image, texture.view);
}

void TextureManager::AddToBeDestroyed(uint32_t frameIndex, const Texture &texture)
{
    assert(texture.image != VK_NULL_HANDLE && texture.view != VK_NULL_HANDLE);

    texturesToDestroy[frameIndex].push_back(texture);
}

MaterialTextures TextureManager::GetMaterialTextures(uint32_t materialIndex) const
{
    if (materialIndex == RG_NO_MATERIAL)
    {
        return EmptyMaterialTextures;
    }

    const auto animIt = animatedMaterials.find(materialIndex);

    if (animIt != animatedMaterials.end())
    {
        const AnimatedMaterial &anim = animIt->second;

        // return material textures of the current frame
        return GetMaterialTextures(anim.materialIndices[anim.currentFrame]);
    }

    const auto it = materials.find(materialIndex);

    if (it == materials.end())
    {
        return EmptyMaterialTextures;
    }

    return it->second.textures;
}

VkDescriptorSet TextureManager::GetDescSet(uint32_t frameIndex) const
{
    return textureDesc->GetDescSet(frameIndex);
}

VkDescriptorSetLayout TextureManager::GetDescSetLayout() const
{
    return textureDesc->GetDescSetLayout();
}

void TextureManager::Subscribe(std::shared_ptr<IMaterialDependency> subscriber)
{
    subscribers.emplace_back(subscriber);
}

void TextureManager::Unsubscribe(const IMaterialDependency *subscriber)
{
    subscribers.remove_if([subscriber] (const std::weak_ptr<IMaterialDependency> &ws)
    {
        if (const auto s = ws.lock())
        {
            return s.get() == subscriber;
        }

        return true;
    });
}

uint32_t TextureManager::GetWaterNormalTextureIndex() const
{
    return waterNormalTextureIndex;
}
