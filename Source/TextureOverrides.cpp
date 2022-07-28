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

#include "TextureOverrides.h"
#include "Const.h"
#include <cstdio>

#include "ImageLoader.h"

using namespace RTGL1;

namespace
{
    VkFormat ToUnorm(VkFormat f)
    {
        switch (f)
        {
            case VK_FORMAT_R8_SRGB: return VK_FORMAT_R8_UNORM;
            case VK_FORMAT_R8G8_SRGB: return VK_FORMAT_R8G8_UNORM;
            case VK_FORMAT_R8G8B8_SRGB: return VK_FORMAT_R8G8B8_UNORM;
            case VK_FORMAT_B8G8R8_SRGB: return VK_FORMAT_B8G8R8_UNORM;
            case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UNORM;
            case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UNORM;
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            case VK_FORMAT_BC2_SRGB_BLOCK: return VK_FORMAT_BC2_UNORM_BLOCK;
            case VK_FORMAT_BC3_SRGB_BLOCK: return VK_FORMAT_BC3_UNORM_BLOCK;
            case VK_FORMAT_BC7_SRGB_BLOCK: return VK_FORMAT_BC7_UNORM_BLOCK;
            default: return f;
        }
    }

    VkFormat ToSRGB(VkFormat f)
    {
        switch (f)
        {
            case VK_FORMAT_R8_UNORM: return VK_FORMAT_R8_SRGB;
            case VK_FORMAT_R8G8_UNORM: return VK_FORMAT_R8G8_SRGB;
            case VK_FORMAT_R8G8B8_UNORM: return VK_FORMAT_R8G8B8_SRGB;
            case VK_FORMAT_B8G8R8_UNORM: return VK_FORMAT_B8G8R8_SRGB;
            case VK_FORMAT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_SRGB;
            case VK_FORMAT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_SRGB;
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
            case VK_FORMAT_BC2_UNORM_BLOCK: return VK_FORMAT_BC2_SRGB_BLOCK;
            case VK_FORMAT_BC3_UNORM_BLOCK: return VK_FORMAT_BC3_SRGB_BLOCK;
            case VK_FORMAT_BC7_UNORM_BLOCK: return VK_FORMAT_BC7_SRGB_BLOCK;
            default: return f;
        }
    }

    auto Loader_Load(TextureOverrides::Loader loader, const char *pFilePath)
    {
        if (std::holds_alternative<ImageLoaderDev *>(loader))
        {
            return std::get<ImageLoaderDev *>(loader)->Load(pFilePath);
        }
        else
        {
            return std::get<ImageLoader *>(loader)->Load(pFilePath);
        }
    }

    void Loader_FreeLoaded(TextureOverrides::Loader loader)
    {
        if (std::holds_alternative<ImageLoaderDev *>(loader))
        {
            std::get<ImageLoaderDev *>(loader)->FreeLoaded();
        }
        else
        {
            std::get<ImageLoader *>(loader)->FreeLoaded();
        }
    }

    constexpr const char *Loader_GetExtension(TextureOverrides::Loader loader)
    {
        if (std::holds_alternative<ImageLoaderDev *>(loader))
        {
            return ".png";
        }
        else
        {
            return ".ktx2";
        }
    }
}

TextureOverrides::TextureOverrides(
    const char *_relativePath,
    const RgTextureSet &_defaultTextures,
    const RgExtent2D &_defaultSize,
    const OverrideInfo &_overrideInfo,
    Loader _loader
)
    : results{}
    , debugName{}
    , loader(_loader)
{
    const void *defaultData[TEXTURES_PER_MATERIAL_COUNT] =
    {
        _defaultTextures.pDataAlbedoAlpha,
        _defaultTextures.pDataRoughnessMetallicEmission,
        _defaultTextures.pDataNormal,
    };

    constexpr VkFormat defaultSRGBFormat = VK_FORMAT_R8G8B8A8_SRGB;
    constexpr VkFormat defaultLinearFormat = VK_FORMAT_R8G8B8A8_UNORM;
    constexpr uint32_t defaultBytesPerPixel = 4;


    if (!_overrideInfo.disableOverride)
    {
        char paths[TEXTURES_PER_MATERIAL_COUNT][TEXTURE_FILE_PATH_MAX_LENGTH];

        // TODO: try to load with different loaders with their own extensions
        if (ParseOverrideTexturePaths(paths, _relativePath, _overrideInfo))
        {
            for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
            {
                results[i] = Loader_Load(loader, paths[i]);
                
                if (results[i])
                {
                    results[i]->format =
                        _overrideInfo.overridenIsSRGB[i] ? ToSRGB(results[i]->format) : ToUnorm(results[i]->format);
                }
            }

            // don't check if wasn't loaded from file, pData might be provided by a user
        }
    }


    const uint32_t defaultDataSize = defaultBytesPerPixel * _defaultSize.width * _defaultSize.height;

    for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
    {
        // if file wasn't found, use default data instead
        if ( !results[i] && defaultData[i] )
        {
            results[i] = ImageLoader::ResultInfo
            {
                .levelOffsets = {0},
                .levelSizes = {defaultDataSize},
                .levelCount = 1,
                .isPregenerated = false,
                .pData = static_cast<const uint8_t *>(defaultData[i]) ,
                .dataSize = defaultDataSize,
                .baseSize = _defaultSize,
                .format = _overrideInfo.originalIsSRGB[i] ? defaultSRGBFormat : defaultLinearFormat,
            };
        }
    }
}

TextureOverrides::~TextureOverrides()
{
    Loader_FreeLoaded(loader);
}

static void ParseFilePath(const char *filePath, char *folderPath, char *name, char *extension)
{
    uint32_t folderPathEnd = 0;
    uint32_t nameStart = 0;
    uint32_t nameEnd = 0;
    uint32_t extStart = 0;

    const char *str = filePath;
    uint32_t len = 0;

    while (str[len] != '\0')
    {
        const char c = str[len];

        // find last folder delimiter
        if (c == '\\' || c == '/')
        {
            folderPathEnd = len;
            nameStart = len + 1;
            // reset, find new dot
            nameEnd = nameStart;
            extStart = nameStart;
        }
        // find last dot that was after a delimiter
        else if (c == '.')
        {
            nameEnd = len - 1;
            // extension start with a dot
            extStart = len;
        }

        ++len;
    }

    assert(folderPathEnd > 0 || folderPathEnd == nameStart);

    // no dot
    if (nameStart == nameEnd)
    {
        nameEnd = len - 1;
        extStart = len;
    }

    const uint32_t folderPathLen = nameStart > 0 ? folderPathEnd + 1 : 0;
    const uint32_t nameLen = nameEnd - nameStart + 1;
    const uint32_t extLen = extStart < len ? len - extStart : 0;

    memcpy(folderPath, str, folderPathLen);
    folderPath[folderPathLen] = '\0';

    memcpy(name, str + nameStart, nameLen);
    name[nameLen] = '\0';

    if (extLen > 0)
    {
        memcpy(extension, str + extStart, extLen);
    }
    extension[extLen] = '\0';
}

static void SPrintfIfNotNull(
    char dst[TEXTURE_FILE_PATH_MAX_LENGTH],
    const char *postfix,
    const char *texturesPath,
    const char *folderPath,
    const char *name,
    const char *extension)
{
    if (postfix != nullptr)
    {
        snprintf(dst, TEXTURE_FILE_PATH_MAX_LENGTH, "%s%s%s%s%s", texturesPath, folderPath, name, postfix, extension);
    }
    else
    {
        dst[0] = '\0';
    }
}

const std::optional<ImageLoader::ResultInfo> &RTGL1::TextureOverrides::GetResult(uint32_t index) const
{
    assert(index < TEXTURES_PER_MATERIAL_COUNT);
    return results[index];
}

const char *RTGL1::TextureOverrides::GetDebugName() const
{
    return debugName;
}

bool TextureOverrides::ParseOverrideTexturePaths(
    char paths[TEXTURES_PER_MATERIAL_COUNT][TEXTURE_FILE_PATH_MAX_LENGTH],
    const char *relativePath,
    const OverrideInfo &overrideInfo)
{
    char folderPath[TEXTURE_FILE_PATH_MAX_LENGTH];
    char name[TEXTURE_FILE_NAME_MAX_LENGTH];
    char extension[TEXTURE_FILE_EXTENSION_MAX_LENGTH];

    if (relativePath != nullptr)
    {
        ParseFilePath(relativePath, folderPath, name, extension);
    }
    else
    {
        return false;
    }

    // ignore original extension, and force KTX2
    const char *newExtension = Loader_GetExtension(loader);

    SPrintfIfNotNull(paths[0], overrideInfo.postfixes[0], overrideInfo.texturesPath, folderPath, name, newExtension);
    SPrintfIfNotNull(paths[1], overrideInfo.postfixes[1], overrideInfo.texturesPath, folderPath, name, newExtension);
    SPrintfIfNotNull(paths[2], overrideInfo.postfixes[2], overrideInfo.texturesPath, folderPath, name, newExtension);

    static_assert(TEXTURE_DEBUG_NAME_MAX_LENGTH < TEXTURE_FILE_PATH_MAX_LENGTH, "TEXTURE_DEBUG_NAME_MAX_LENGTH must be less than TEXTURE_FILE_PATH_MAX_LENGTH");

    memcpy(debugName, name, TEXTURE_DEBUG_NAME_MAX_LENGTH);
    debugName[TEXTURE_DEBUG_NAME_MAX_LENGTH - 1] = '\0';

    return true;
}
