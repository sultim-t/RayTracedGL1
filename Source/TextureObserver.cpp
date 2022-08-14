// Copyright (c) 2022 Sultim Tsyrendashiev
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

#include "TextureObserver.h"

RTGL1::TextureObserver::TextureObserver()
{
}

RTGL1::TextureObserver::~TextureObserver()
{
}

void RTGL1::TextureObserver::CheckPathsAndReupload()
{
}

void RTGL1::TextureObserver::RegisterPath(RgMaterial index, std::optional<std::filesystem::path> path)
{
    if (index == RG_NO_MATERIAL)
    {
        return;
    }

    if (!path.has_value() || path.value().empty())
    {
        return;
    }

    assert(texturePaths.find(index) == texturePaths.end());

    texturePaths[index] = std::move(path.value());
}

void RTGL1::TextureObserver::Remove(RgMaterial index)
{
    //static_assert(false);
}
