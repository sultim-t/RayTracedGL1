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
#include "Const.h"
#include "SamplerManager.h"

#include <filesystem>

namespace RTGL1
{

struct Texture
{
    VkImage                             image         = VK_NULL_HANDLE;
    VkImageView                         view          = VK_NULL_HANDLE;
    RgExtent2D                          size          = {};
    VkFormat                            format        = VK_FORMAT_UNDEFINED;
    SamplerManager::Handle              samplerHandle = SamplerManager::Handle();
    std::optional< RgTextureSwizzling > swizzling     = std::nullopt;
    std::filesystem::path               filepath      = {};
};


struct MaterialTextures
{
    // Indices to use in shaders, each index represents a texture: albedo, roughness, etc
    uint32_t indices[ TEXTURES_PER_MATERIAL_COUNT ];
};

}