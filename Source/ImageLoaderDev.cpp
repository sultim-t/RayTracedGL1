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

#include "ImageLoaderDev.h"

#include <algorithm>
#include <cassert>

#include "Stb/stb_image.h"

using namespace RTGL1;

ImageLoaderDev::ImageLoaderDev(
    std::shared_ptr<ImageLoader> _fallback
)
    : fallback(std::move(_fallback))
{}

ImageLoaderDev::~ImageLoaderDev()
{
    assert(loadedImages.empty());
}

std::optional<ImageLoader::ResultInfo> ImageLoaderDev::Load(const char *pFilePath)
{
    // if null ptr or empty string
    if (pFilePath == nullptr || pFilePath[0] == '\0')
    {
        return fallback->Load(pFilePath);
    }

    int x = 0, y = 0;
    constexpr uint32_t Channels = 4;

    stbi_uc *pData = stbi_load(pFilePath, &x, &y, nullptr, Channels);
    if (pData == nullptr)
    {
        const char *reason = stbi_failure_reason();

        return fallback->Load(pFilePath);
    }

    const uint32_t width = x;
    const uint32_t height = y;

    assert(width > 0 && height > 0);

    ImageLoader::ResultInfo result =
    {
        .levelOffsets = {0},
        .levelSizes = {width * height * Channels},
        .levelCount = 1,
        .isPregenerated = false,
        .pData = pData,
        .dataSize = width * height * Channels,
        .baseSize = {width, height},
        .format = VK_FORMAT_R8G8B8A8_SRGB,
    };

    loadedImages.push_back(static_cast<void*>(pData));
    return result;
}

void ImageLoaderDev::FreeLoaded()
{
    for (void *pData : loadedImages)
    {
        stbi_image_free(pData);
    }
    loadedImages.clear();

    fallback->FreeLoaded();
}
