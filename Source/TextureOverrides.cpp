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
    namespace detail
    {
        template< typename T >
        auto LoadByFullPath( T& specificLoader, const std::filesystem::path& filepath )
        {
            if( std::filesystem::is_regular_file( filepath ) )
            {
                if( auto r = specificLoader.Load( filepath ) )
                {
                    return r;
                }
            }

            return std::optional< ImageLoader::ResultInfo >{};
        }

        template< size_t I, typename Loaders >
            requires( I >= std::tuple_size_v< Loaders > )
        auto LoadByIndex( Loaders&,
                          const std::filesystem::path&,
                          std::string_view,
                          std::string_view,
                          std::filesystem::path& outPath )
        {
            assert( outPath.empty() );
            return std::optional< ImageLoader::ResultInfo >{};
        }

        template< size_t I, typename Loaders >
            requires( I < std::tuple_size_v< Loaders > )
        auto LoadByIndex( Loaders&                     loaders,
                          const std::filesystem::path& ovrdFolder,
                          std::string_view             name,
                          std::string_view             postfix,
                          std::filesystem::path&       outPath )
        {
            if( auto l = std::get< I >( loaders ) )
            {
                using LoaderType = std::remove_pointer_t< std::tuple_element_t< I, Loaders > >;

                auto basePath = ovrdFolder / LoaderType::GetFolder();

                for( const char* ext : LoaderType::GetExtensions() )
                {
                    auto filepath =
                        TextureOverrides::GetTexturePath( basePath, name, postfix, ext );

                    if( auto r = LoadByFullPath( *l, filepath ) )
                    {
                        outPath = std::move( filepath );
                        return r;
                    }
                }
            }

            return LoadByIndex< I + 1 >( loaders, ovrdFolder, name, postfix, outPath );
        }

        template< size_t I, typename Loaders >
            requires( I >= std::tuple_size_v< Loaders > )
        auto LoadByFullPathByIndex( Loaders&, const std::filesystem::path& )
        {
            return std::optional< ImageLoader::ResultInfo >{};
        }

        template< size_t I, typename Loaders >
            requires( I < std::tuple_size_v< Loaders > )
        auto LoadByFullPathByIndex( Loaders& loaders, const std::filesystem::path& filepath )
        {
            if( auto l = std::get< I >( loaders ) )
            {
                if( auto r = LoadByFullPath( *l, filepath ) )
                {
                    return r;
                }
            }

            return LoadByFullPathByIndex< I + 1 >( loaders, filepath );
        }
    }

    template< typename Loaders >
    auto Load( Loaders&                     loaders,
               const std::filesystem::path& ovrdFolder,
               std::string_view             name,
               std::string_view             postfix,
               std::filesystem::path&       outPath )
    {
        return detail::LoadByIndex< 0 >( loaders, ovrdFolder, name, postfix, outPath );
    }

    template< typename Loaders >
    auto Load( Loaders& loaders, const std::filesystem::path& fullpath )
    {
        return detail::LoadByFullPathByIndex< 0 >( loaders, fullpath );
    }

    template< typename Loaders >
    void FreeLoaded( Loaders& loaders )
    {
        std::apply(
            []( auto*... specific ) {
                //
                auto freeIfNotNull = []( auto* t ) {
                    if( t )
                    {
                        t->FreeLoaded();
                    }
                };

                ( freeIfNotNull( specific ), ... );
            },
            loaders );
    }
}

}

TextureOverrides::TextureOverrides( const std::filesystem::path& _ovrdFolder,
                                    std::string_view             _name,
                                    std::string_view             _postfix,
                                    const void*                  _defaultPixels,
                                    const RgExtent2D&            _defaultSize,
                                    VkFormat                     _defaultFormat,
                                    Loader                       _loader )
    : result{ std::nullopt }, debugname{}, iloader( std::move( _loader ) )
{
    Utils::SafeCstrCopy( debugname, _name );

    
    std::visit(
        [ & ]( auto&& specific ) {
            if( auto r = loader::Load( specific, _ovrdFolder, _name, _postfix, path ) )
            {
                r->format = Utils::IsSRGB( _defaultFormat ) ? Utils::ToSRGB( r->format )
                                                            : Utils::ToUnorm( r->format );

                result = r;
            }
        },
        iloader );

    // if file wasn't found, use defaults
    if( !result )
    {
        assert( path.empty() );

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
                path = GetTexturePath( _ovrdFolder / TEXTURES_FOLDER_DEV, _name, _postfix, "" );
            }
        }
    }
}

TextureOverrides::TextureOverrides( const std::filesystem::path& _fullPath,
                                    bool                         _isSRGB,
                                    Loader                       _loader )
    : result{ std::nullopt }, debugname{}, iloader( std::move( _loader ) )
{
    Utils::SafeCstrCopy( debugname, _fullPath.string() );

    std::visit(
        [ & ]( auto&& specific ) {
            if( auto r = loader::Load( specific, _fullPath ) )
            {
                r->format = _isSRGB ? Utils::ToSRGB( r->format ) : Utils::ToUnorm( r->format );

                result = r;
                path   = _fullPath;
            }
        },
        iloader );
}

TextureOverrides::~TextureOverrides()
{
    std::visit( []( auto&& specific ) { loader::FreeLoaded( specific ); }, iloader );
}

std::filesystem::path TextureOverrides::GetTexturePath( std::filesystem::path basePath,
                                                        std::string_view      name,
                                                        std::string_view      postfix,
                                                        std::string_view      extension )
{
    auto validName = std::string( name );

    for( char& c : validName )
    {
        switch( c )
        {
            case '<':
            case '>':
            case ':':
            case '\"':
            case '|':
            case '?':
            case '*': {
                debug::Verbose( "Invalid char \'{}\' in material name: {}", c, name );
                c = '_';
                break;
            }

            case '\\': {
                c = '/';
                break;
            }

            default: break;
        }
    }

    return basePath.append( validName ).make_preferred().concat( postfix ).concat( extension );
}
