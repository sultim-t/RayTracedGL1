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

namespace RTGL1
{

constexpr uint32_t      ALLOCATOR_BLOCK_SIZE_STAGING_TEXTURES   = 64 * 512 * 512 * 4;
constexpr uint32_t      ALLOCATOR_BLOCK_SIZE_TEXTURES           = 64 * 512 * 512 * 4;

constexpr uint32_t      TEXTURE_FILE_PATH_MAX_LENGTH            = 512;
constexpr uint32_t      TEXTURE_FILE_NAME_MAX_LENGTH            = 256;
constexpr uint32_t      TEXTURE_FILE_EXTENSION_MAX_LENGTH       = 16;

constexpr uint32_t      TEXTURE_COUNT_MIN                       = 1024;
constexpr uint32_t      TEXTURE_COUNT_MAX                       = 4096;
constexpr uint32_t      EMPTY_TEXTURE_INDEX                     = 0;
constexpr uint32_t      MATERIALS_MAX_LAYER_COUNT               = 3;
constexpr uint32_t      TEXTURES_PER_MATERIAL_COUNT             = 3;

constexpr const char    *DEFAULT_TEXTURES_PATH                  = "";
constexpr const char    *DEFAULT_TEXTURES_POSTFIXES[TEXTURES_PER_MATERIAL_COUNT] = { "", "_rme", "_n" };

constexpr uint32_t      MAX_PREGENERATED_MIPMAP_LEVELS          = 20;

// Use WORLD2 mask bit as SKY
#define RAYCULLMASK_SKY_IS_WORLD2 1

}