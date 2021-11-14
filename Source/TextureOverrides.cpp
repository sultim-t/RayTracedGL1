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
#include <stdio.h>

using namespace RTGL1;


static VkFormat ConvertToUnorm(VkFormat f)
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

static VkFormat ConvertToSRGB(VkFormat f)
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


TextureOverrides::TextureOverrides(
    const char *_relativePath,
    const void *_defaultData,     
    bool _isSRGB,
    const RgExtent2D &_defaultSize,
    const OverrideInfo &_overrideInfo,
    std::shared_ptr<ImageLoader> _imageLoader)
:
    TextureOverrides(_relativePath, 
                     RgTextureSet{ { _defaultData, _isSRGB }, {}, {} }, _defaultSize, 
                     _overrideInfo, std::move(_imageLoader))
{}

TextureOverrides::TextureOverrides(
    const char *_relativePath, 
    const RgTextureSet &_defaultTextures,
    const RgExtent2D &_defaultSize,
    const OverrideInfo &_overrideInfo, 
    std::shared_ptr<ImageLoader> _imageLoader) 
:
    results{},
    debugName{},
    imageLoader(_imageLoader)
{
    const RgTextureData *defaultData[TEXTURES_PER_MATERIAL_COUNT] =
    {
        &_defaultTextures.albedoAlpha,
        &_defaultTextures.roughnessMetallicEmission,
        &_defaultTextures.normal,
    };

    const VkFormat defaultSRGBFormat = VK_FORMAT_R8G8B8A8_SRGB;
    const VkFormat defaultLinearFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const uint32_t defaultBytesPerPixel = 4;


    if (!_overrideInfo.disableOverride)
    {
        char paths[TEXTURES_PER_MATERIAL_COUNT][TEXTURE_FILE_PATH_MAX_LENGTH];
        const bool hasOverrides = ParseOverrideTexturePaths(paths, _relativePath, _overrideInfo);

        if (hasOverrides)
        {
            for (uint32_t i = 0; i < TEXTURES_PER_MATERIAL_COUNT; i++)
            {
                _imageLoader->Load(paths[i], &results[i]);

                // fix format, if needed
                results[i].format = _overrideInfo.overridenIsSRGB[i] ?
                    ConvertToSRGB(results[i].format) :
                    ConvertToUnorm(results[i].format);
            }

            // don't check if wasn't loaded from file, pData might be provided by a user
        }
    }


    const uint32_t defaultDataSize = defaultBytesPerPixel * _defaultSize.width * _defaultSize.height;

    for (uint32_t i = 0; i < 3; i++)
    {
        // if file wasn't found, use default data instead
        if (defaultData[i]->pData != nullptr && results[i].pData == nullptr)
        {
            results[i].pData = static_cast<const uint8_t *>(defaultData[i]->pData);
            results[i].dataSize = defaultDataSize;
            results[i].levelCount = 1;
            results[i].isPregenerated = false;
            results[i].levelOffsets[0] = 0;
            results[i].levelSizes[0] = defaultDataSize;
            results[i].baseSize = _defaultSize;
            results[i].format = defaultData[i]->isSRGB ? defaultSRGBFormat : defaultLinearFormat;
        }
    }
}

TextureOverrides::~TextureOverrides()
{
    if (auto p = imageLoader.lock())
    {
        p->FreeLoaded();
    }
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

    const uint32_t folderPathLen = folderPathEnd + 1;
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
#if _WINDLL
        sprintf_s(dst, TEXTURE_FILE_PATH_MAX_LENGTH, "%s%s%s%s%s", texturesPath, folderPath, name, postfix, extension);
#else
        snprintf(dst, TEXTURE_FILE_PATH_MAX_LENGTH, "%s%s%s%s%s", texturesPath, folderPath, name, postfix, extension);
#endif
    }
    else
    {
        dst[0] = '\0';
    }
}

const ImageLoader::ResultInfo &RTGL1::TextureOverrides::GetResult(uint32_t index) const
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

    SPrintfIfNotNull(paths[0], overrideInfo.postfixes[0], overrideInfo.texturesPath, folderPath, name, extension);
    SPrintfIfNotNull(paths[1], overrideInfo.postfixes[1], overrideInfo.texturesPath, folderPath, name, extension);
    SPrintfIfNotNull(paths[2], overrideInfo.postfixes[2], overrideInfo.texturesPath, folderPath, name, extension);

    static_assert(TEXTURE_DEBUG_NAME_MAX_LENGTH < TEXTURE_FILE_PATH_MAX_LENGTH, "TEXTURE_DEBUG_NAME_MAX_LENGTH must be less than TEXTURE_FILE_PATH_MAX_LENGTH");

    memcpy(debugName, name, TEXTURE_DEBUG_NAME_MAX_LENGTH);
    debugName[TEXTURE_DEBUG_NAME_MAX_LENGTH - 1] = '\0';

    return true;
}
