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

// Struct for loading overriding texture files. Should be created on stack.
struct TextureOverrides
{
public:
    struct OverrideInfo
    {
        bool disableOverride = false;

        const char *texturesPath             = nullptr;
        const char *albedoAlphaPostfix       = nullptr;
        const char *normalMetallicPostfix    = nullptr;
        const char *emissionRoughnessPostfix = nullptr;
        // Default params for overriden textures. If texture isn't overriden,
        // RgTextureData::isSRGB value is used
        bool aaIsSRGBDefault = false;
        bool nmIsSRGBDefault = false;
        bool erIsSRGBDefault = false;
    };

public:
    explicit TextureOverrides(
        const char *relativePath,
        const RgTextureSet &defaultTextures,
        const RgExtent2D &defaultSize,
        const OverrideInfo &overrideInfo,
        std::shared_ptr<ImageLoader> imageLoader);
    explicit TextureOverrides(
        const char *relativePath,
        const void *defaultData,
        bool isSRGB,
        const RgExtent2D &defaultSize,
        const OverrideInfo &overrideInfo,
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
        const OverrideInfo &overrideInfo);

public:
    // Albedo-Alpha
    ImageLoader::ResultInfo aa;

    // Normal-Metallic
    ImageLoader::ResultInfo nm;

    // Emission-Roughness
    ImageLoader::ResultInfo er;

    char    debugName[TEXTURE_DEBUG_NAME_MAX_LENGTH];

private:
    std::weak_ptr<ImageLoader> imageLoader;
};

}