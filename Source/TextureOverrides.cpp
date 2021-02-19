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
    const RgExtent2D &_defaultSize,
    const ParseInfo &_parseInfo, 
    std::shared_ptr<ImageLoader> _imageLoader) 
:
    aa(nullptr), aaSize({}), 
    nm(nullptr), nmSize({}), 
    er(nullptr), erSize({}), 
    imageLoader(_imageLoader)
{
    char albedoAlphaPath[TEXTURE_FILE_PATH_MAX_LENGTH];
    char normalMetallic[TEXTURE_FILE_PATH_MAX_LENGTH];
    char emissionRoughness[TEXTURE_FILE_PATH_MAX_LENGTH];

    const bool hasOverrides = ParseOverrideTexturePaths(
        albedoAlphaPath, normalMetallic, emissionRoughness,
        _relativePath, _parseInfo);

    if (hasOverrides)
    {
        aa = _imageLoader->LoadRGBA8(albedoAlphaPath,   &aaSize.width, &aaSize.height);
        nm = _imageLoader->LoadRGBA8(normalMetallic,    &nmSize.width, &nmSize.height);
        er = _imageLoader->LoadRGBA8(emissionRoughness, &erSize.width, &erSize.height);
    }

    // if file wasn't found, use data instead
    if (_defaultData != nullptr && aa == nullptr)
    {
        aa = (uint32_t*)_defaultData;
        aaSize = _defaultSize;
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

bool TextureOverrides::ParseOverrideTexturePaths(
    char *albedoAlphaPath,
    char *normalMetallicPath,
    char *emissionRoughnessPath,
    const char *relativePath,
    const ParseInfo &parseInfo)
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

    const char *albedoAlphaPostfix       = parseInfo.albedoAlphaPostfix       != nullptr ? parseInfo.albedoAlphaPostfix       : "";
    const char *normalMetallicPostfix    = parseInfo.normalMetallicPostfix    != nullptr ? parseInfo.normalMetallicPostfix    : "";
    const char *emissionRoughnessPostfix = parseInfo.emissionRoughnessPostfix != nullptr ? parseInfo.emissionRoughnessPostfix : "";

    sprintf_s(albedoAlphaPath,       TEXTURE_FILE_PATH_MAX_LENGTH, "%s%s%s%s%s", parseInfo.texturesPath, folderPath, name, albedoAlphaPostfix, extension);
    sprintf_s(normalMetallicPath,    TEXTURE_FILE_PATH_MAX_LENGTH, "%s%s%s%s%s", parseInfo.texturesPath, folderPath, name, normalMetallicPostfix, extension);
    sprintf_s(emissionRoughnessPath, TEXTURE_FILE_PATH_MAX_LENGTH, "%s%s%s%s%s", parseInfo.texturesPath, folderPath, name, emissionRoughnessPostfix, extension);

    static_assert(TEXTURE_DEBUG_NAME_MAX_LENGTH < TEXTURE_FILE_PATH_MAX_LENGTH, "TEXTURE_DEBUG_NAME_MAX_LENGTH must be less than TEXTURE_FILE_PATH_MAX_LENGTH");

    memcpy(debugName, name, TEXTURE_DEBUG_NAME_MAX_LENGTH);
    debugName[TEXTURE_DEBUG_NAME_MAX_LENGTH - 1] = '\0';

    return true;
}
