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

using namespace RTGL1;

ImageLoader::~ImageLoader()
{
    assert(loadedImages.empty());
}

const uint32_t *ImageLoader::LoadRGBA8(const char *path, uint32_t *outWidth, uint32_t *outHeight)
{
    int width, height, channels;
    uint8_t *img = stbi_load(path, &width, &height, &channels, 4);

    if (img == nullptr)
    {
        *outWidth = 0;
        *outHeight = 0;
        return nullptr;
    }

    *outWidth = width;
    *outHeight = height;

    loadedImages.push_back(static_cast<void*>(img));

    return reinterpret_cast<const uint32_t*>(img);
}

void ImageLoader::FreeLoaded()
{
    for (auto *p : loadedImages)
    {
        stbi_image_free(p);
    }

    loadedImages.clear();
}
