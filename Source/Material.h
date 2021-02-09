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

namespace RTGL1
{

constexpr uint32_t EMPTY_TEXTURE_INDEX          = 0;
constexpr uint32_t MATERIALS_MAX_LAYER_COUNT    = 3;
constexpr uint32_t TEXTURES_PER_MATERIAL_COUNT  = 3;

struct Texture
{
    VkImage             image;
    VkImageView         view;
    VkSampler           sampler;
};

union MaterialTextures
{
    uint32_t            indices[TEXTURES_PER_MATERIAL_COUNT];
    struct
    {
        uint32_t        albedoAlpha;
        uint32_t        normalMetallic;
        uint32_t        emissionRoughness;
    };
};

struct Material
{
    MaterialTextures    textures;
    uint32_t            isDynamic;
};

struct AnimatedMaterial
{
    // Indices of static materials.
    std::vector<uint32_t>   materialIndices;
    uint32_t                currentFrame;
};

}