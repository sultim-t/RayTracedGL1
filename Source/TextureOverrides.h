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

#include "Common.h"
#include "RTGL1/RTGL1.h"
#include "ImageLoader.h"

namespace RTGL1
{

#define TEXTURE_DEBUG_NAME_MAX_LENGTH 32

struct ParseInfo
{
    const char *texturesPath;
    const char *albedoAlphaPostfix;
    const char *normalMetallicPostfix;
    const char *emissionRoughnessPostfix;
};

// Struct for loading overriding texture files. Should be created on stack.
struct TextureOverrides
{
public:
    explicit TextureOverrides(
        const char *relativePath,
        const RgTextureData &defaultData,
        const RgExtent2D &defaultSize,
        const ParseInfo &parseInfo,
        std::shared_ptr<ImageLoader> imageLoader);
    explicit TextureOverrides(
        const char *relativePath,
        const void *defaultData,
        const RgExtent2D &defaultSize,
        const ParseInfo &parseInfo,
        std::shared_ptr<ImageLoader> imageLoader);
    ~TextureOverrides();

    TextureOverrides(const TextureOverrides &other) = delete;
    TextureOverrides(TextureOverrides &&other) noexcept = delete;
    TextureOverrides &operator=(const TextureOverrides &other) = delete;
    TextureOverrides &operator=(TextureOverrides &&other) noexcept = delete;

private:
    bool ParseOverrideTexturePaths(
        char *albedoAlphaPath,
        char *normalMetallicPath,
        char *emissionRoughnessPath,
        const char *relativePath,
        const ParseInfo &parseInfo);

public:
    // Albedo-Alpha
    const uint32_t      *aa;
    RgExtent2D          aaSize;

    // Normal-Metallic
    const uint32_t      *nm;
    RgExtent2D          nmSize;

    // Emission-Roughness
    const uint32_t      *er;
    RgExtent2D          erSize;

    char                debugName[TEXTURE_DEBUG_NAME_MAX_LENGTH];

private:
    std::weak_ptr<ImageLoader> imageLoader;
};

}