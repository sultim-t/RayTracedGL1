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

#include "ImageLoader.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include <algorithm>
#include <cassert>

RTGL1::ImageLoader::~ImageLoader()
{
    assert( loadedImages.empty() );
}

bool RTGL1::ImageLoader::LoadTextureFile( const std::filesystem::path& path,
                                          ktxTexture**                 ppTexture )
{
    KTX_error_code r = ktxTexture_CreateFromNamedFile(
        path.string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, ppTexture );

    return r == KTX_SUCCESS;
}

std::optional< RTGL1::ImageLoader::ResultInfo > RTGL1::ImageLoader::Load(
    const std::filesystem::path& path )
{
    if( path.empty() )
    {
        return std::nullopt;
    }

    ktxTexture* pTexture = nullptr;
    bool        loaded   = LoadTextureFile( path, &pTexture );

    if( !loaded )
    {
        return std::nullopt;
    }

    assert( pTexture->numDimensions == 2 );
    assert( pTexture->numLevels <= MAX_PREGENERATED_MIPMAP_LEVELS );
    assert( pTexture->numLayers == 1 );
    assert( pTexture->numFaces == 1 );


    ResultInfo result = {
        .levelOffsets   = {},
        .levelSizes     = {},
        .levelCount     = std::min( pTexture->numLevels, MAX_PREGENERATED_MIPMAP_LEVELS ),
        .isPregenerated = true,
        .pData          = ktxTexture_GetData( pTexture ),
        .dataSize       = static_cast< uint32_t >( ktxTexture_GetDataSize( pTexture ) ),
        .baseSize       = { pTexture->baseWidth, pTexture->baseHeight },
        .format         = ktxTexture_GetVkFormat( pTexture ),
    };

    // get mipmap offsets / sizes
    for( uint32_t level = 0; level < result.levelCount; level++ )
    {
        ktx_size_t offset = 0;
        auto       r      = ktxTexture_GetImageOffset( pTexture, level, 0, 0, &offset );

        ktx_size_t size = ktxTexture_GetImageSize( pTexture, level );

        if( r != KTX_SUCCESS || size == 0 )
        {
            result.levelCount = level + 1;
            break;
        }

        result.levelOffsets[ level ] = static_cast< uint32_t >( offset );
        result.levelSizes[ level ]   = static_cast< uint32_t >( size );
    }


    loadedImages.push_back( pTexture );
    return result;
}

std::optional< RTGL1::ImageLoader::LayeredResultInfo > RTGL1::ImageLoader::LoadLayered(
    const std::filesystem::path& path )
{
    if( path.empty() )
    {
        return std::nullopt;
    }

    ktxTexture* pTexture = nullptr;
    bool        loaded   = LoadTextureFile( path, &pTexture );

    if( !loaded )
    {
        return std::nullopt;
    }

    assert( pTexture->numDimensions == 2 );
    assert( pTexture->numLevels == 1 );
    assert( pTexture->numFaces == 1 );


    LayeredResultInfo result = {
        .layerData = {},
        .dataSize  = static_cast< uint32_t >( ktxTexture_GetDataSize( pTexture ) ),
        .baseSize  = { pTexture->baseWidth, pTexture->baseHeight },
        .format    = ktxTexture_GetVkFormat( pTexture ),
    };

    uint8_t* pData = ktxTexture_GetData( pTexture );

    for( uint32_t i = 0; i < pTexture->numLayers; i++ )
    {
        ktx_size_t       offset;
        ktx_error_code_e r = ktxTexture_GetImageOffset( pTexture, 0, i, 0, &offset );

        if( r != KTX_SUCCESS )
        {
            continue;
        }

        result.layerData.push_back( pData + offset );
    }

    loadedImages.push_back( pTexture );
    return result;
}

void RTGL1::ImageLoader::FreeLoaded()
{
    for( ktxTexture* p : loadedImages )
    {
        ktxTexture_Destroy( p );
    }

    loadedImages.clear();
}
