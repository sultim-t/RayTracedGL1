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

#pragma once

#include <filesystem>

namespace RTGL1
{

enum class FileType
{
    Unknown,
    GLTF,
    KTX2,
    PNG,
    TGA,
    JPG,
};

inline FileType MakeFileType( const std::filesystem::path& p )
{
    // 2 allocations...
    auto ext = p.extension().string();

    std::ranges::transform( ext, ext.begin(), []( unsigned char c ) { return std::tolower( c ); } );

    if( ext == ".gltf" )
    {
        return FileType::GLTF;
    }
    else if( ext == ".ktx2" )
    {
        return FileType::KTX2;
    }
    else if( ext == ".png" )
    {
        return FileType::PNG;
    }
    else if( ext == ".tga" )
    {
        return FileType::TGA;
    }
    else if( ext == ".jpg" || ext == ".jpeg" )
    {
        return FileType::JPG;
    }

    return FileType::Unknown;
}

class IFileDependency
{
public:
    virtual ~IFileDependency()                                                         = default;
    virtual void OnFileChanged( FileType type, const std::filesystem::path& filepath ) = 0;
};

}