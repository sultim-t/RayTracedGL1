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

#include "Const.h"
#include "Utils.h"
#include "TextureOverrides.h"
#include "Generated/ShaderCommonC.h"
#include "RgException.h"


using namespace RTGL1;


namespace
{
    static_assert(TEXTURES_PER_MATERIAL_COUNT == sizeof(RgTextureSet) / sizeof(const void *), "TEXTURES_PER_MATERIAL_COUNT must be same as in RgTextureSet");

    constexpr MaterialTextures EmptyMaterialTextures = { EMPTY_TEXTURE_INDEX, EMPTY_TEXTURE_INDEX,EMPTY_TEXTURE_INDEX };

    constexpr RgSamplerFilter DefaultDynamicSamplerFilter = RG_SAMPLER_FILTER_LINEAR;

    template <typename T>
    constexpr const T *DefaultIfNull(const T *pData, const T *pDefault)
    {
        return pData != nullptr ? pData : pDefault;
    }

    TextureOverrides::Loader GetLoader(const std::shared_ptr<ImageLoader> &defaultLoader, const std::shared_ptr<ImageLoaderDev> devLoader)
    {
        return devLoader ? TextureOverrides::Loader(devLoader.get()) : TextureOverrides::Loader(defaultLoader.get());
    }
}


TextureManager::TextureManager( VkDevice                                       _device,
                                std::shared_ptr< MemoryAllocator >             _memAllocator,
                                std::shared_ptr< SamplerManager >              _samplerMgr,
                                const std::shared_ptr< CommandBufferManager >& _cmdManager,
                                std::shared_ptr< UserFileLoad >                _userFileLoad,
                                const RgInstanceCreateInfo&                    _info,
                                const LibraryConfig::Config&                   _config )
    : device( _device )
    , pbrSwizzling( _info.pbrTextureSwizzling )
    , samplerMgr( std::move( _samplerMgr ) )
    , waterNormalTextureIndex( 0 )
    , currentDynamicSamplerFilter( DefaultDynamicSamplerFilter )
    , defaultTexturesPath(
        DefaultIfNull(_info.pOverridenTexturesFolderPath, DEFAULT_TEXTURES_PATH)
        )
    , postfixes
        {
            DefaultIfNull(_info.pOverridenAlbedoAlphaTexturePostfix, DEFAULT_TEXTURE_POSTFIX_ALBEDO_ALPHA),
            DefaultIfNull(_info.pOverridenRoughnessMetallicEmissionTexturePostfix, DEFAULT_TEXTURE_POSTFIX_ROUGNESS_METALLIC_EMISSION),
            DefaultIfNull(_info.pOverridenNormalTexturePostfix, DEFAULT_TEXTURE_POSTFIX_NORMAL),
        }
    , overridenIsSRGB
        {
            !!_info.overridenAlbedoAlphaTextureIsSRGB,
            !!_info.overridenRoughnessMetallicEmissionTextureIsSRGB,
            !!_info.overridenNormalTextureIsSRGB,
        }
    , originalIsSRGB
        {
            !!_info.originalAlbedoAlphaTextureIsSRGB,
            !!_info.originalRoughnessMetallicEmissionTextureIsSRGB,
            !!_info.originalNormalTextureIsSRGB,
        }
    , forceNormalMapFilterLinear( !!_info.textureSamplerForceNormalMapFilterLinear )
{
    const uint32_t maxTextureCount =
        std::clamp( _info.maxTextureCount, TEXTURE_COUNT_MIN, TEXTURE_COUNT_MAX );

    imageLoader = std::make_shared< ImageLoader >( std::move( _userFileLoad ) );

    if( _config.developerMode )
    {
        imageLoaderDev = std::make_shared< ImageLoaderDev >( imageLoader );
        observer       = std::make_shared< TextureObserver >();

        if( _info.pOverridenTexturesFolderPathDeveloper != nullptr )
        {
            defaultTexturesPath = _info.pOverridenTexturesFolderPathDeveloper;
        }
    }


    textureDesc = std::make_shared< TextureDescriptors >(
        device, samplerMgr, maxTextureCount, BINDING_TEXTURES );
    textureUploader = std::make_shared< TextureUploader >( device, std::move( _memAllocator ) );

    textures.resize( maxTextureCount );

    // submit cmd to create empty texture
    VkCommandBuffer cmd = _cmdManager->StartGraphicsCmd();
    CreateEmptyTexture( cmd, 0 );
    CreateWaterNormalTexture( cmd, 0, _info.pWaterNormalTexturePath );
    _cmdManager->Submit( cmd );
    _cmdManager->WaitGraphicsIdle();

    if( this->waterNormalTextureIndex == EMPTY_TEXTURE_INDEX )
    {
        throw RgException( RG_WRONG_ARGUMENT,
                           "Couldn't create water normal texture with path: " +
                               std::string( _info.pWaterNormalTexturePath ) );
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

    uint32_t textureIndex = PrepareTexture(cmd, frameIndex, info, samplerHandle, false, "Empty texture", false, std::nullopt);

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

    TextureOverrides::OverrideInfo parseInfo = 
    {
        // use absolute path
        .commonFolderPath = ""
    };
    for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
    {
        parseInfo.postfixes[i] = "";
        parseInfo.originalIsSRGB[i] = false;
        parseInfo.overridenIsSRGB[i] = false;
    }

    constexpr uint32_t defaultData[] = { 0x7F7FFFFF };
    constexpr RgExtent2D defaultSize = { 1, 1 };
    // try to load image file
    TextureOverrides ovrd(pFilePath, RgTextureSet{ .pDataAlbedoAlpha = defaultData }, defaultSize, parseInfo, imageLoader.get());

    this->waterNormalTextureIndex = PrepareTexture( cmd,
                                                    frameIndex,
                                                    ovrd.GetResult( 0 ),
                                                    samplerHandle,
                                                    true,
                                                    "Water normal",
                                                    false,
                                                    std::nullopt );
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

uint32_t TextureManager::CreateMaterial( VkCommandBuffer             cmd,
                                         uint32_t                    frameIndex,
                                         const RgMaterialCreateInfo& createInfo )
{
    if( createInfo.pRelativePath == nullptr && createInfo.textures.pDataAlbedoAlpha == nullptr &&
        createInfo.textures.pDataRoughnessMetallicEmission == nullptr &&
        createInfo.textures.pDataNormal == nullptr )
    {
        throw RgException(
            RG_WRONG_MATERIAL_PARAMETER,
            R"(At least one of 'pRelativePath' or 'textures' members must be not null)" );
    }

    auto samplerHandle = SamplerManager::Handle(
        createInfo.filter, createInfo.addressModeU, createInfo.addressModeV, createInfo.flags );

    auto normalMapSamplerHandle = SamplerManager::Handle(
        forceNormalMapFilterLinear ? RG_SAMPLER_FILTER_LINEAR : createInfo.filter,
        createInfo.addressModeU,
        createInfo.addressModeV,
        createInfo.flags & ( ~RG_MATERIAL_CREATE_DYNAMIC_SAMPLER_FILTER_BIT ) );

    TextureOverrides::OverrideInfo parseInfo = {
        .commonFolderPath = defaultTexturesPath.c_str(),
    };
    for( uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++ )
    {
        parseInfo.postfixes[ i ]       = postfixes[ i ].c_str();
        parseInfo.overridenIsSRGB[ i ] = overridenIsSRGB[ i ];
        parseInfo.originalIsSRGB[ i ]  = originalIsSRGB[ i ];
    }

    // load additional textures, they'll be freed after leaving the scope
    TextureOverrides ovrd( createInfo.pRelativePath,
                           createInfo.textures,
                           createInfo.size,
                           parseInfo,
                           GetLoader( imageLoader, imageLoaderDev ) );


    bool isUpdateable = createInfo.flags & RG_MATERIAL_CREATE_UPDATEABLE_BIT;
    if( observer )
    {
        // treat everything as updateable
        isUpdateable = true;
    }


    MaterialTextures mtextures = {};
    for( uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++ )
    {
        const auto& texSampler =
            i != MATERIAL_NORMAL_INDEX ? samplerHandle : normalMapSamplerHandle;

        mtextures.indices[ i ] = PrepareTexture(
            cmd,
            frameIndex,
            ovrd.GetResult( i ),
            texSampler,
            !( createInfo.flags & RG_MATERIAL_CREATE_DONT_GENERATE_MIPMAPS_BIT ),
            ovrd.GetDebugName(),
            isUpdateable,
            i == MATERIAL_ROUGHNESS_METALLIC_EMISSION_INDEX ? std::optional( pbrSwizzling )
                                                            : std::nullopt );
    }

    uint32_t materialIndex = InsertMaterial( mtextures, isUpdateable );


    if( observer )
    {
        for( uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++ )
        {
            observer->RegisterPath(
                materialIndex, ovrd.GetPathAndRemove( i ), ovrd.GetResult( i ), i );
        }
    }


    return materialIndex;
}

bool TextureManager::UpdateMaterial(VkCommandBuffer cmd, const RgMaterialUpdateInfo &updateInfo)
{
    const auto it = materials.find(updateInfo.target);

    // must exist
    if (it == materials.end())
    {
        throw RgException(RG_CANT_UPDATE_MATERIAL,
            "Material with ID=" + std::to_string(updateInfo.target) + " was not created");
    }

    // must be updateable
    if (!it->second.isUpdateable)
    {
        throw RgException(RG_CANT_UPDATE_MATERIAL,
            "Material with ID=" + std::to_string(updateInfo.target) + " was not marked as updateable");
    }

    const void *updateData[TEXTURES_PER_MATERIAL_COUNT] = 
    {
        updateInfo.textures.pDataAlbedoAlpha,
        updateInfo.textures.pDataRoughnessMetallicEmission,
        updateInfo.textures.pDataNormal,
    };

    auto &textureIndices = it->second.textures.indices;
    static_assert(sizeof(textureIndices) / sizeof(textureIndices[0]) == TEXTURES_PER_MATERIAL_COUNT);

    bool wasUpdated = false;

    for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
    {
        uint32_t textureIndex = textureIndices[i];

        if (textureIndex == EMPTY_TEXTURE_INDEX)
        {
            continue;
        }

        VkImage img = textures[textureIndex].image;

        if (img == VK_NULL_HANDLE || updateData[i] == nullptr)
        {
            continue;
        }

        textureUploader->UpdateImage(cmd, img, updateData[i]);
        wasUpdated = true;
    }

    return wasUpdated;
}

uint32_t TextureManager::PrepareTexture(
    VkCommandBuffer                                 cmd,
    uint32_t                                        frameIndex,
    const std::optional< ImageLoader::ResultInfo >& optImageInfo,
    SamplerManager::Handle                          samplerHandle,
    bool                                            useMipmaps,
    const char*                                     debugName,
    bool                                            isUpdateable,
    std::optional< RgTextureSwizzling >             swizzling )
{
    if( !optImageInfo.has_value() )
    {
        return EMPTY_TEXTURE_INDEX;
    }
    const auto& imageInfo = optImageInfo.value();

    if( imageInfo.baseSize.width == 0 || imageInfo.baseSize.height == 0 )
    {
        using namespace std::string_literals;

        throw RgException( RG_WRONG_MATERIAL_PARAMETER,
                           "Incorrect size (" + std::to_string( imageInfo.baseSize.width ) + ", " +
                               std::to_string( imageInfo.baseSize.height ) +
                               ") of one of images in a material" +
                               ( debugName != nullptr ? " with name: "s + debugName : ""s ) );
    }

    assert( imageInfo.dataSize > 0 );
    assert( imageInfo.levelCount > 0 && imageInfo.levelSizes[ 0 ] > 0 );

    TextureUploader::UploadInfo info = {
        .cmd                    = cmd,
        .frameIndex             = frameIndex,
        .pData                  = imageInfo.pData,
        .dataSize               = imageInfo.dataSize,
        .cubemap                = {},
        .baseSize               = imageInfo.baseSize,
        .format                 = imageInfo.format,
        .useMipmaps             = useMipmaps,
        .pregeneratedLevelCount = imageInfo.isPregenerated ? imageInfo.levelCount : 0,
        .pLevelDataOffsets      = imageInfo.levelOffsets,
        .pLevelDataSizes        = imageInfo.levelSizes,
        .isUpdateable           = isUpdateable,
        .pDebugName             = debugName,
        .isCubemap              = false,
        .swizzling              = swizzling,
    };

    auto [ wasUploaded, image, view ] = textureUploader->UploadImage( info );

    if( !wasUploaded )
    {
        return EMPTY_TEXTURE_INDEX;
    }

    return InsertTexture( frameIndex, image, view, samplerHandle );
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
        materialIndices[i] = CreateMaterial(cmd, frameIndex, createInfo.pFrames[i]);
    }

    return InsertAnimatedMaterial(materialIndices);
}

bool TextureManager::ChangeAnimatedMaterialFrame(uint32_t animMaterial, uint32_t materialFrame)
{
    const auto animIt = animatedMaterials.find(animMaterial);

    if (animIt == animatedMaterials.end())
    {
        throw RgException(RG_CANT_UPDATE_ANIMATED_MATERIAL, "Material with ID=" + std::to_string(animMaterial) + " is not animated");
    }

    AnimatedMaterial &anim = animIt->second;

    {
        auto maxFrameCount = static_cast<uint32_t>(anim.materialIndices.size());
        if (materialFrame >= maxFrameCount)
        {
            throw RgException(RG_CANT_UPDATE_ANIMATED_MATERIAL,
                "Animated material with ID=" + std::to_string(animMaterial) + " has only " +
                std::to_string(maxFrameCount) + " frames, but frame with index "
                + std::to_string(materialFrame) + " was requested");
        }
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

uint32_t TextureManager::InsertMaterial(const MaterialTextures &materialTextures, bool isUpdateable)
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
    materials[matIndex] = Material
    {
        .textures = materialTextures,
        .isUpdateable = isUpdateable,
    };

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
    animatedMaterials[animMatIndex] = AnimatedMaterial
    {
        .materialIndices = std::move(materialIndices),
        .currentFrame = 0,
    };

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


    if (observer)
    {
        observer->Remove(materialIndex);
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

void TextureManager::CheckForHotReload(VkCommandBuffer cmd)
{
    if (observer && imageLoaderDev)
    {
        observer->CheckPathsAndReupload(cmd, *this, imageLoaderDev.get());
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

MaterialTextures TextureManager::GetMaterialTextures( const char* pTextureName ) const
{
    if( pTextureName == nullptr )
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
