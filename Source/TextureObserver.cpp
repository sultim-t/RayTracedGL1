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

#if 0

#include "TextureManager.h"
#include "Generated/ShaderCommonC.h"

bool RTGL1::TextureObserver::HaveChanged( std::vector< DependentFile >& files )
{
    bool changed = false;

    for( DependentFile& f : files )
    {
        auto tm = std::filesystem::last_write_time( f.path );

        if( tm > f.lastWriteTime )
        {
            f.lastWriteTime = tm;
            changed         = true;
        }
    }

    return changed;
}

void RTGL1::TextureObserver::CheckPathsAndReupload( VkCommandBuffer cmd,
                                                    TextureManager& manager,
                                                    ImageLoaderDev* loader )
{
    if( loader == nullptr )
    {
        return;
    }

    {
        using namespace std::chrono_literals;

        constexpr auto frequency = 0.05s;

        auto           now = std::chrono::system_clock::now();
        if( now - lastCheck < frequency )
        {
            return;
        }

        lastCheck = now;
    }

    for( auto& [ materialIndex, files ] : materials )
    {
        if( !HaveChanged( files ) )
        {
            continue;
        }

        for( const DependentFile& f : files )
        {
            if( auto newImage = loader->Load( f.path ) )
            {
                if( newImage->dataSize != f.dataSize )
                {
                    assert( 0 && "Trying to hot-reload the image, but the data size is mismatching "
                                 "with what originally was specified."
                                 "A new texture file must have the same image size." );
                    continue;
                }

                RgMaterialUpdateInfo info =
                {
                    .target = materialIndex,
                    .textures = 
                    {
                        .pDataAlbedoAlpha               = f.textureType == MATERIAL_ALBEDO_ALPHA_INDEX                  ? newImage->pData : nullptr,
                        .pDataRoughnessMetallicEmission = f.textureType == MATERIAL_ROUGHNESS_METALLIC_EMISSION_INDEX   ? newImage->pData : nullptr,
                        .pDataNormal                    = f.textureType == MATERIAL_NORMAL_INDEX                        ? newImage->pData : nullptr,
                    },
                };

                manager.UpdateMaterial( cmd, info );

                loader->FreeLoaded();
            }
        }
    }
}

void RTGL1::TextureObserver::RegisterPath(
    RgMaterial                                      index,
    std::optional< std::filesystem::path >          path,
    const std::optional< ImageLoader::ResultInfo >& imageInfo,
    uint32_t                                        textureType )
{
    if( index == RG_NO_MATERIAL )
    {
        return;
    }

    if( !path || path->empty() )
    {
        return;
    }

    if( !imageInfo || imageInfo->dataSize == 0 || imageInfo->pData == nullptr )
    {
        return;
    }

    if( materials.find( index ) == materials.end() )
    {
        materials[ index ] = {};
    }

    if( !std::filesystem::exists( path.value() ) )
    {
        return;
    }

    auto tm = std::filesystem::last_write_time( path.value() );

    materials[ index ].emplace_back( DependentFile{
        .path          = std::move( path.value() ),
        .lastWriteTime = tm,
        .dataSize      = imageInfo->dataSize,
        .format        = imageInfo->format,
        .textureType   = textureType,
    } );
}

void RTGL1::TextureObserver::Remove( RgMaterial index )
{
    materials.erase( index );
}

#endif
