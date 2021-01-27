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

enum class VertexCollectorFilterTypeFlagBits
{
    NONE                    = 0,

    STATIC_NON_MOVABLE      = 1 << 0,
    STATIC_MOVABLE          = 2 << 0,
    DYNAMIC                 = 4 << 0,
    MASK_CHANGE_FREQUENCY   = STATIC_NON_MOVABLE | STATIC_MOVABLE | DYNAMIC,

    OPAQUE                  = 1 << 3,
    TRANSPARENT             = 2 << 3,
    MASK_PASS_THROUGH       = OPAQUE | TRANSPARENT,
};
typedef uint32_t VertexCollectorFilterTypeFlags;

inline VertexCollectorFilterTypeFlags operator|(VertexCollectorFilterTypeFlagBits a, VertexCollectorFilterTypeFlagBits b)
{
    typedef VertexCollectorFilterTypeFlags FL;
    return static_cast<FL>(static_cast<FL>(a) | static_cast<FL>(b));
}