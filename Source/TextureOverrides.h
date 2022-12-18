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

#include <variant>

#include "Common.h"
#include "RTGL1/RTGL1.h"
#include "ImageLoader.h"
#include "ImageLoaderDev.h"

namespace RTGL1
{

constexpr uint32_t TEXTURE_DEBUG_NAME_MAX_LENGTH = 32;

// Struct for loading overriding texture files. Should be created on stack.
struct TextureOverrides
{
    using Loader = std::variant< ImageLoader*, ImageLoaderDev* >;

    explicit TextureOverrides( const std::filesystem::path& basePath,
                               std::string_view             relativePath,
                               std::string_view             postfix,
                               const void*                  defaultPixels,
                               const RgExtent2D&            defaultSize,
                               VkFormat                     defaultFormat,
                               Loader                       loader );
    ~TextureOverrides();

    TextureOverrides( const TextureOverrides& other )                = delete;
    TextureOverrides( TextureOverrides&& other ) noexcept            = delete;
    TextureOverrides& operator=( const TextureOverrides& other )     = delete;
    TextureOverrides& operator=( TextureOverrides&& other ) noexcept = delete;

    std::optional< ImageLoader::ResultInfo > result;
    char                                     debugname[ TEXTURE_DEBUG_NAME_MAX_LENGTH ];
    std::filesystem::path                    path;

private:
    Loader loader;
};

}