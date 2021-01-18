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
#include "Const.h"

//static const char *SupportedImageExtensions[] = {
//    ".png",
//    ".tga",
//};

TextureManager::TextureManager(
    std::shared_ptr<MemoryAllocator> memAllocator, 
    const char *defaultTexturesPath,
    const char *albedoAlphaPostfix,
    const char *normalMetallicPostfix, 
    const char *emissionRoughnessPostfix)
{
    this->memAllocator = memAllocator;
    this->defaultTexturesPath = defaultTexturesPath;
    this->albedoAlphaPostfix = albedoAlphaPostfix;
    this->normalMetallicPostfix = normalMetallicPostfix;
    this->emissionRoughnessPostfix = emissionRoughnessPostfix;

    imageLoader = std::make_shared<ImageLoader>();
}

TextureManager::~TextureManager()
{
}

uint32_t TextureManager::CreateStaticTexture(const RgStaticTextureCreateInfo *createInfo)
{
    TextureOverrides overrides; 
    GetOverrides(createInfo, &overrides);

    const uint32_t *data = nullptr;
    const RgExtent2D size = {};

    //

    const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    const VkDeviceSize bytesPerPixel = 4;

    VkDeviceSize dataSize = bytesPerPixel * size.width * size.height;

    VkBufferCreateInfo stagingInfo = {};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = dataSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkDeviceMemory stagingMemory, imageMemory;
    void *mappedData;

    // allocate and fill buffer
    memAllocator->CreateStagingSrcTextureBuffer(&stagingInfo, &stagingMemory, &mappedData);
    memcpy(mappedData, data, dataSize);

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = size.width;
    imageInfo.extent.height = size.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = createInfo->mipmapCount;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    memAllocator->CreateDstTextureImage(&imageInfo, &imageMemory);



    ClearOverrides();
}

uint32_t TextureManager::CreateAnimatedTexture(const RgAnimatedTextureCreateInfo *createInfo)
{

}

uint32_t TextureManager::CreateDynamicTexture(const RgDynamicTextureCreateInfo *createInfo)
{

}

#pragma region Texture overrides
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

        if (c == '\\' || c == '/')
        {
            folderPathEnd = len - 1;
            nameStart = len + 1;
            // reset
            nameEnd = nameStart;
            extStart = nameStart;
        }
        else if (c == '.')
        {
            nameEnd = len - 1;
            extStart = len + 1;
        }

        ++len;
    }

    assert(0 < folderPathEnd && folderPathEnd < nameStart);

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

static void ParseFileName(const char *fileName, char *name, char *extension)
{
    uint32_t nameEnd = 0;
    uint32_t extStart = 0;

    const char *str = fileName;
    uint32_t len = 0;

    while (str[len] != '\0')
    {
        const char c = str[len];

        if (c == '.')
        {
            nameEnd = len - 1;
            extStart = len + 1;
        }

        ++len;
    }

    if (nameEnd != 0)
    {
        const uint32_t nameLen = nameEnd + 1;
        const uint32_t extLen = extStart < len ? len - extStart : 0;

        memcpy(name, str, nameLen);
        name[nameLen] = '\0';

        if (extLen > 0)
        {
            memcpy(extension, str + extStart, extLen);
        }
        extension[extLen] = '\0';
    }
    else
    {
        // if dot wasn't found
        memcpy(name, str, len);
        name[len] = '\0';

        extension[0] = '\0';
    }
}

bool TextureManager::ParseOverrideTexturePaths(
    const RgStaticTextureCreateInfo *createInfo,
    char *albedoAlphaPath,
    char *normalMetallicPath,
    char *emissionRoughnessPath,
    char *debugName) const
{
    char folderPath[TEXTURE_FILE_PATH_MAX_LENGTH];
    char name[TEXTURE_FILE_NAME_MAX_LENGTH];
    char extension[TEXTURE_FILE_EXTENSION_MAX_LENGTH];

    const char *pFolderPathSrc = nullptr;

    if (createInfo->filePath != nullptr)
    {
        ParseFilePath(createInfo->filePath, folderPath, name, extension);
        pFolderPathSrc = folderPath;
    }
    else if (createInfo->fileName != nullptr)
    {
        ParseFileName(createInfo->fileName, name, extension);
        pFolderPathSrc = defaultTexturesPath.c_str();
    }
    else
    {
        return false;
    }

    assert(pFolderPathSrc != nullptr);

    sprintf_s(albedoAlphaPath,       TEXTURE_FILE_PATH_MAX_LENGTH, "%s/%s%s.%s", pFolderPathSrc, name, albedoAlphaPostfix.c_str(), extension);
    sprintf_s(normalMetallicPath,    TEXTURE_FILE_PATH_MAX_LENGTH, "%s/%s%s.%s", pFolderPathSrc, name, normalMetallicPostfix.c_str(), extension);
    sprintf_s(emissionRoughnessPath, TEXTURE_FILE_PATH_MAX_LENGTH, "%s/%s%s.%s", pFolderPathSrc, name, emissionRoughnessPostfix.c_str(), extension);

    static_assert(TEXTURE_DEBUG_NAME_MAX_LENGTH < TEXTURE_FILE_PATH_MAX_LENGTH, "TEXTURE_DEBUG_NAME_MAX_LENGTH must be less than TEXTURE_FILE_PATH_MAX_LENGTH");

    memcpy(debugName, name, TEXTURE_DEBUG_NAME_MAX_LENGTH);
    debugName[TEXTURE_DEBUG_NAME_MAX_LENGTH - 1] = '\0';

    return true;
}

void TextureManager::GetOverrides(const RgStaticTextureCreateInfo *createInfo, TextureOverrides *result)
{
    char albedoAlphaPath[TEXTURE_FILE_PATH_MAX_LENGTH];
    char normalMetallic[TEXTURE_FILE_PATH_MAX_LENGTH];
    char emissionRoughness[TEXTURE_FILE_PATH_MAX_LENGTH];

    const bool hasOverrides = ParseOverrideTexturePaths(
        createInfo, albedoAlphaPath, normalMetallic, emissionRoughness, result->debugName);

    if (hasOverrides)
    {
        result->aa = imageLoader->LoadRGBA8(albedoAlphaPath,   &result->aaSize.width, &result->aaSize.height);
        result->nm = imageLoader->LoadRGBA8(normalMetallic,    &result->nmSize.width, &result->nmSize.height);
        result->er = imageLoader->LoadRGBA8(emissionRoughness, &result->erSize.width, &result->erSize.height);
    }

    // if file wasn't found, use data instead
    if (createInfo->data != nullptr && result->aa == nullptr)
    {
        result->aa = createInfo->data;
        result->aaSize = createInfo->size;
    }
}

void TextureManager::ClearOverrides()
{
    imageLoader->FreeLoaded();
}
#pragma endregion 

