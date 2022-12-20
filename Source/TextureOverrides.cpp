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

#include "TextureOverrides.h"

#include <array>
#include <filesystem>
#include <span>

#include "Const.h"
#include "ImageLoader.h"
#include "Utils.h"

using namespace RTGL1;

namespace
{
template< size_t N >
void SafeCopy( char ( &dst )[ N ], std::string_view src )
{
    memset( dst, 0, N );

    for( size_t i = 0; i < N - 1 && i < src.length(); i++ )
    {
        dst[ i ] = src[ i ];
    }
}

std::optional< uint32_t > ResolveDefaultDataSize( VkFormat format, const RgExtent2D& size )
{
    if( format != VK_FORMAT_R8G8B8A8_SRGB && format != VK_FORMAT_R8G8B8A8_UNORM )
    {
        assert( 0 && "Default can be only RGBA8" );
        return std::nullopt;
    }
    constexpr uint32_t defaultBytesPerPixel = 4;

    return defaultBytesPerPixel * size.width * size.height;
}

namespace loader
{
    auto Load( TextureOverrides::Loader loader, const std::filesystem::path& filepath )
    {
        if( std::holds_alternative< ImageLoaderDev* >( loader ) )
        {
            return std::get< ImageLoaderDev* >( loader )->Load( filepath );
        }
        else
        {
            return std::get< ImageLoader* >( loader )->Load( filepath );
        }
    }

    void FreeLoaded( TextureOverrides::Loader loader )
    {
        if( std::holds_alternative< ImageLoaderDev* >( loader ) )
        {
            std::get< ImageLoaderDev* >( loader )->FreeLoaded();
        }
        else
        {
            std::get< ImageLoader* >( loader )->FreeLoaded();
        }
    }

    std::span< const char* > GetExtensions( TextureOverrides::Loader loader )
    {
        if( std::holds_alternative< ImageLoaderDev* >( loader ) )
        {
            static const char* arr[] = { ".png", ".tga", ".jpg", ".jpeg" };
            return arr;
        }
        else
        {
            static const char* arr[] = { ".ktx2" };
            return arr;
        }
    }
}

std::filesystem::path GetTexturePath( std::filesystem::path basePath,
                                      std::string_view      relativePath,
                                      std::string_view      postfix,
                                      std::string_view      extension )
{
    if( relativePath.empty() )
    {
        return basePath.replace_extension( "" ).concat( postfix ).replace_extension( extension );
    }

    return basePath.append( relativePath )
        .replace_extension( "" )
        .concat( postfix )
        .replace_extension( extension );
}
}

TextureOverrides::TextureOverrides( const std::filesystem::path& _basePath,
                                    std::string_view             _relativePath,
                                    std::string_view             _postfix,
                                    const void*                  _defaultPixels,
                                    const RgExtent2D&            _defaultSize,
                                    VkFormat                     _defaultFormat,
                                    Loader                       _loader )
    : result{ std::nullopt }, debugname{}, loader( _loader )
{
    SafeCopy( debugname, _relativePath );


    for( const char* ext : loader::GetExtensions( loader ) )
    {
        auto p = GetTexturePath( _basePath, _relativePath, _postfix, ext );

        if( !std::filesystem::is_regular_file( p ) )
        {
            continue;
        }

        if( auto r = loader::Load( loader, p ) )
        {
            r->format = Utils::IsSRGB( _defaultFormat ) ? Utils::ToSRGB( r->format )
                                                        : Utils::ToUnorm( r->format );

            result = r;
            path   = std::move( p );

            break;
        }
    }


    // if file wasn't found, use defaults
    if( !result )
    {
        if( _defaultPixels )
        {
            if( auto dataSize = ResolveDefaultDataSize( _defaultFormat, _defaultSize ) )
            {
                result = ImageLoader::ResultInfo{
                    .levelOffsets   = { 0 },
                    .levelSizes     = { *dataSize },
                    .levelCount     = 1,
                    .isPregenerated = false,
                    .pData          = static_cast< const uint8_t* >( _defaultPixels ),
                    .dataSize       = *dataSize,
                    .baseSize       = _defaultSize,
                    .format         = _defaultFormat,
                };
                path = GetTexturePath( _basePath, _relativePath, _postfix, "" );
            }
        }
    }
}

TextureOverrides::TextureOverrides( const std::filesystem::path& _fullPath,
                                    bool                         _isSRGB,
                                    Loader                       _loader )
    : result{ std::nullopt }, debugname{}, loader( _loader )
{
    if( !std::filesystem::is_regular_file( _fullPath ) )
    {
        return;
    }
    SafeCopy( debugname, _fullPath.string() );

    if( auto r = loader::Load( loader, _fullPath ) )
    {
        r->format = _isSRGB ? Utils::ToSRGB( r->format )
                                                    : Utils::ToUnorm( r->format );

        result = r;
        path   = _fullPath;
    }
}

TextureOverrides::~TextureOverrides()
{
    loader::FreeLoaded( loader );
}
