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

using namespace RTGL1;


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
    aa{}, nm{}, er{},
    debugName{},
    imageLoader(_imageLoader)
{
    if (!_overrideInfo.disableOverride)
    {
        char paths[3][TEXTURE_FILE_PATH_MAX_LENGTH];

        const bool hasOverrides = ParseOverrideTexturePaths(
            paths[0], paths[1], paths[2],
            _relativePath, _overrideInfo);

        if (hasOverrides)
        {
            _imageLoader->Load(paths[0], &aa);
            _imageLoader->Load(paths[1], &nm);
            _imageLoader->Load(paths[2], &er);

            // don't check if wasn't loaded from file, pData might be provided by a user
        }
    }

    const VkFormat defaultSRGBFormat = VK_FORMAT_R8G8B8A8_SRGB;
    const VkFormat defaultLinearFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const uint32_t defaultBytesPerPixel = 4;

    const uint32_t defaultDataSize = defaultBytesPerPixel * _defaultSize.width * _defaultSize.height;

    // if file wasn't found, use default data instead
    if (_defaultTextures.albedoAlpha.pData != nullptr && aa.pData == nullptr)
    {
        aa.pData = static_cast<const uint8_t*>(_defaultTextures.albedoAlpha.pData);
        aa.dataSize = defaultDataSize;
        aa.levelCount = 1;
        aa.levelOffsets[0] = 0;
        aa.levelSizes[0] = defaultDataSize;
        aa.baseSize = _defaultSize;
        aa.format = _defaultTextures.albedoAlpha.isSRGB ? defaultSRGBFormat : defaultLinearFormat;
    }

    if (_defaultTextures.normalsMetallicity.pData != nullptr && nm.pData == nullptr)
    {
        nm.pData = static_cast<const uint8_t*>(_defaultTextures.normalsMetallicity.pData);
        nm.dataSize = defaultDataSize;
        nm.levelCount = 1;
        nm.levelOffsets[0] = 0;
        nm.levelSizes[0] = defaultDataSize;
        nm.baseSize = _defaultSize;
        nm.format = _defaultTextures.normalsMetallicity.isSRGB ? defaultSRGBFormat : defaultLinearFormat;
    }

    if (_defaultTextures.emissionRoughness.pData != nullptr && er.pData == nullptr)
    {
        er.pData = static_cast<const uint8_t*>(_defaultTextures.emissionRoughness.pData);
        er.dataSize = defaultDataSize;
        er.levelCount = 1;
        er.levelOffsets[0] = 0;
        er.levelSizes[0] = defaultDataSize;
        er.baseSize = _defaultSize;
        er.format = _defaultTextures.emissionRoughness.isSRGB ? defaultSRGBFormat : defaultLinearFormat;
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
    char **dst, 
    const char *postfix,
    const char *texturesPath,
    const char *folderPath,
    const char *name,
    const char *extension)
{
    if (postfix != nullptr)
    {
        sprintf_s(*dst, TEXTURE_FILE_PATH_MAX_LENGTH, "%s%s%s%s%s", texturesPath, folderPath, name, postfix, extension);
    }
    else
    {
        *dst = nullptr;
    }
}

bool TextureOverrides::ParseOverrideTexturePaths(
    char *albedoAlphaPath,
    char *normalMetallicPath,
    char *emissionRoughnessPath,
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

    SPrintfIfNotNull(&albedoAlphaPath,       overrideInfo.albedoAlphaPostfix,       overrideInfo.texturesPath, folderPath, name, extension);
    SPrintfIfNotNull(&normalMetallicPath,    overrideInfo.normalMetallicPostfix,    overrideInfo.texturesPath, folderPath, name, extension);
    SPrintfIfNotNull(&emissionRoughnessPath, overrideInfo.emissionRoughnessPostfix, overrideInfo.texturesPath, folderPath, name, extension);

    static_assert(TEXTURE_DEBUG_NAME_MAX_LENGTH < TEXTURE_FILE_PATH_MAX_LENGTH, "TEXTURE_DEBUG_NAME_MAX_LENGTH must be less than TEXTURE_FILE_PATH_MAX_LENGTH");

    memcpy(debugName, name, TEXTURE_DEBUG_NAME_MAX_LENGTH);
    debugName[TEXTURE_DEBUG_NAME_MAX_LENGTH - 1] = '\0';

    return true;
}
