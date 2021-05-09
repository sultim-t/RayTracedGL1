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

#include "ImageLoader.h"

#include <cassert>

#include <ktx.h>
#include <ktxvulkan.h>

using namespace RTGL1;

ImageLoader::ImageLoader(std::shared_ptr<UserFileLoad> _userFileLoad) : userFileLoad(std::move( _userFileLoad))
{}

ImageLoader::~ImageLoader()
{
    assert(loadedImages.empty());
}

bool ImageLoader::LoadTextureFile(const char *pFilePath, ktxTexture **ppTexture)
{
    KTX_error_code r;

    if (userFileLoad->Exists())
    {
        auto fileHandle = userFileLoad->Open(pFilePath);

        if (!fileHandle.Contains())
        {
            return false;
        }

        r = ktxTexture_CreateFromMemory(
            static_cast<const uint8_t *>(fileHandle.pData), fileHandle.dataSize,
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            ppTexture
        );
    }
    else
    {
        r = ktxTexture_CreateFromNamedFile(
            pFilePath,
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            ppTexture);
       
    }

    return r == KTX_SUCCESS;
}

bool ImageLoader::Load(const char *pFilePath, ResultInfo *pResultInfo)
{
    assert(pResultInfo != nullptr);
    *pResultInfo = {};

    if (pFilePath == nullptr)
    {
        return false;
    }

    ktxTexture *pTexture = nullptr;
    bool loaded = LoadTextureFile(pFilePath, &pTexture);

    if (!loaded)
    {
        return false;
    }

    assert(pTexture->numDimensions == 2);
    assert(pTexture->numLevels <= MAX_PREGENERATED_MIPMAP_LEVELS);
    assert(pTexture->numLayers == 1);
    assert(pTexture->numFaces == 1);


    pResultInfo->baseSize = { pTexture->baseWidth, pTexture->baseHeight };
    pResultInfo->format = ktxTexture_GetVkFormat(pTexture);
    pResultInfo->pData = ktxTexture_GetData(pTexture);
    pResultInfo->dataSize = static_cast<uint32_t>(ktxTexture_GetDataSize(pTexture));

    
    // get mipmap offsets
    pResultInfo->levelCount = std::min(pTexture->numLevels, MAX_PREGENERATED_MIPMAP_LEVELS);

    for (uint32_t level = 0; level < pResultInfo->levelCount; level++)
    {
        ktx_size_t offset = 0;
        auto r = ktxTexture_GetImageOffset(pTexture, level, 0, 0, &offset);

        ktx_size_t size = ktxTexture_GetImageSize(pTexture, level);

        if (r != KTX_SUCCESS || size == 0)
        {
            pResultInfo->levelCount = level + 1;
            break;
        }

        pResultInfo->levelOffsets[level] = static_cast<uint32_t>(offset);
        pResultInfo->levelSizes[level] = static_cast<uint32_t>(size);
    }


    loadedImages.push_back(static_cast<void*>(pTexture));
    return true;
}

bool ImageLoader::LoadLayered(const char *pFilePath, LayeredResultInfo *pResultInfo)
{
    assert(pResultInfo != nullptr);

    if (pFilePath == nullptr)
    {
        return false;
    }

    ktxTexture *pTexture = nullptr;
    bool loaded = LoadTextureFile(pFilePath, &pTexture);

    if (!loaded)
    {
        return false;
    }

    assert(pTexture->numDimensions == 2);
    assert(pTexture->numLevels == 1);
    assert(pTexture->numFaces == 1);


    pResultInfo->baseSize = { pTexture->baseWidth, pTexture->baseHeight };
    pResultInfo->format = ktxTexture_GetVkFormat(pTexture);
    pResultInfo->dataSize = static_cast<uint32_t>(ktxTexture_GetDataSize(pTexture));

    pResultInfo->layerData.clear();

    uint8_t *pData = ktxTexture_GetData(pTexture);

    for (uint32_t i = 0; i < pTexture->numLayers; i++)
    {
        ktx_size_t offset;
        ktx_error_code_e r = ktxTexture_GetImageOffset(pTexture, 0, i, 0, &offset);

        if (r != KTX_SUCCESS)
        {
            continue;
        }

        pResultInfo->layerData.push_back(pData + offset);
    }

    loadedImages.push_back(static_cast<void *>(pTexture));
    return true;
}

void ImageLoader::FreeLoaded()
{
    for (void *pp : loadedImages)
    {
        ktxTexture *p = static_cast<ktxTexture*>(pp);

        ktxTexture_Destroy(p);
    }

    loadedImages.clear();
}
