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

#include "Stb/stb_image.h"

#include <ktx.h>
#include <ktxvulkan.h>

using namespace RTGL1;

ImageLoader::~ImageLoader()
{
    assert(loadedImages.empty());
}

ImageLoader::ResultInfo ImageLoader::Load(const char *pFilePath)
{
    if (pFilePath == nullptr)
    {
        return {};
    }

    ktxTexture *pTexture = nullptr;

    KTX_error_code r = ktxTexture_CreateFromNamedFile(
        pFilePath,
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &pTexture);

    if (r != KTX_SUCCESS)
    {
        return {};
    }

    // TODO: KTX 
    assert(pTexture->numDimensions == 2);
    assert(pTexture->numLevels == 1);
    assert(pTexture->numLayers == 1);
    assert(pTexture->numFaces == 1);

    ResultInfo result = {};
    result.isLoaded = true;
    result.width = pTexture->baseWidth;
    result.height = pTexture->baseHeight;
    result.format = ktxTexture_GetVkFormat(pTexture);
    result.pData = ktxTexture_GetData(pTexture);
    result.dataSize = ktxTexture_GetDataSize(pTexture);

    loadedImages.push_back(static_cast<void*>(pTexture));
    return result;
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
