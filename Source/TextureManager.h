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

#include <string>

#include "Common.h"
#include "ImageLoader.h"
#include "MemoryAllocator.h"
#include "RTGL1/RTGL1.h"

#define TEXTURE_DEBUG_NAME_MAX_LENGTH 32

class TextureManager
{
public:
    explicit TextureManager(
        std::shared_ptr<MemoryAllocator> memAllocator,
        const char *defaultTexturesPath,
        const char *albedoAlphaPostfix,
        const char *normalMetallicPostfix,
        const char *emissionRoughnessPostfix);
    ~TextureManager();

    TextureManager(const TextureManager &other) = delete;
    TextureManager(TextureManager &&other) noexcept = delete;
    TextureManager &operator=(const TextureManager &other) = delete;
    TextureManager &operator=(TextureManager &&other) noexcept = delete;

    uint32_t CreateStaticTexture(const RgStaticTextureCreateInfo *createInfo);
    uint32_t CreateAnimatedTexture(const RgAnimatedTextureCreateInfo *createInfo);
    uint32_t CreateDynamicTexture(const RgDynamicTextureCreateInfo *createInfo);

private:
    struct TextureOverrides
    {
        // albedo-alpha
        const uint32_t      *aa;
        // normal-metallic
        const uint32_t      *nm;
        // emission-roughness
        const uint32_t      *er;
        RgExtent2D          aaSize;
        RgExtent2D          nmSize;
        RgExtent2D          erSize;
        char                debugName[TEXTURE_DEBUG_NAME_MAX_LENGTH];
    };

private:
    bool ParseOverrideTexturePaths(
        const RgStaticTextureCreateInfo *createInfo,
        char *albedoAlphaPath,
        char *normalMetallic,
        char *emissionRoughness,
        char *debugName) const;

    // Load texture overrides if they exist.
    // Memory must freed by ClearOverrides(..) after using data.
    void GetOverrides(const RgStaticTextureCreateInfo *createInfo, TextureOverrides *result);
    // Free texture overrides memory.
    void ClearOverrides();

private:
    std::shared_ptr<ImageLoader> imageLoader;
    std::shared_ptr<MemoryAllocator> memAllocator;

    std::string defaultTexturesPath;
    std::string albedoAlphaPostfix;
    std::string normalMetallicPostfix;
    std::string emissionRoughnessPostfix;
};
