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

#pragma once

#include <cstdint>
#include <vector>

// Loading images from files.
class ImageLoader
{
public:
    ImageLoader() = default;
    ~ImageLoader();

    ImageLoader(const ImageLoader &other) = delete;
    ImageLoader(ImageLoader &&other) noexcept = delete;
    ImageLoader &operator=(const ImageLoader &other) = delete;
    ImageLoader &operator=(ImageLoader &&other) noexcept = delete;

    // Read the file and get array of R8G8B8A8 values.
    // Returns null if image wasn't loaded.
    const uint32_t *LoadRGBA8(const char *path, uint32_t *outWidth, uint32_t *outHeight);
    // Must be called after using the loaded data to free the allocated memory
    void FreeLoaded();

private:
    std::vector<void*> loadedImages;
};