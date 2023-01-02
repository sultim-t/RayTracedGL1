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

#include "TextureMeta.h"

#include "Const.h"
#include "Utils.h"


namespace
{
std::string_view TEXTURES_FILENAME = "textures.json";

template< size_t N >
    requires( N == 3 || N == 4 )
RgColor4DPacked32 ClampAndPackColor( std::array< int, N > color )
{
    if constexpr( N == 3 )
    {
        uint8_t rgb[] = {
            uint8_t( std::clamp( color[ 0 ], 0, 255 ) ),
            uint8_t( std::clamp( color[ 1 ], 0, 255 ) ),
            uint8_t( std::clamp( color[ 2 ], 0, 255 ) ),
        };
        return RTGL1::Utils::PackColor( rgb[ 0 ], rgb[ 1 ], rgb[ 2 ], 255 );
    }
    if constexpr( N == 4 )
    {
        uint8_t rgba[] = {
            uint8_t( std::clamp( color[ 0 ], 0, 255 ) ),
            uint8_t( std::clamp( color[ 1 ], 0, 255 ) ),
            uint8_t( std::clamp( color[ 2 ], 0, 255 ) ),
            uint8_t( std::clamp( color[ 3 ], 0, 255 ) ),
        };
        return RTGL1::Utils::PackColor( rgba[ 0 ], rgba[ 1 ], rgba[ 2 ], rgba[ 3 ] );
    }

    assert( 0 );
    return 0;
}
}

RTGL1::TextureMetaManager::TextureMetaManager( std::filesystem::path _databaseFolder )
    : databaseFolder( std::move( _databaseFolder ) )
{
    sourceGlobal = databaseFolder / TEXTURES_FILENAME;
}

auto RTGL1::TextureMetaManager::Access( const char* pTextureName ) const
    -> std::optional< RTGL1::TextureMeta >
{
    if( Utils::IsCstrEmpty( pTextureName ) )
    {
        return std::nullopt;
    }

    auto strTextureName = std::string( pTextureName );
    {
        auto found = dataScene.find( strTextureName );
        if( found != dataScene.end() )
        {
            return found->second;
        }
    }
    {
        auto found = dataGlobal.find( strTextureName );
        if( found != dataGlobal.end() )
        {
            return found->second;
        }
    }
    return std::nullopt;
}

void RTGL1::TextureMetaManager::RereadFromFiles( std::filesystem::path sceneFile )
{
    sourceScene = std::move( sceneFile );

    dataGlobal.clear();
    dataScene.clear();

    auto reread = [ this ]( const std::filesystem::path &filepath, auto& data ) {
        if( !std::filesystem::exists( filepath ) )
        {
            return;
        }

        if( auto arr = json_parser::ReadFileAs< TextureMetaArray >( filepath ) )
        {
            for( const TextureMeta& v : arr->array )
            {
                if( !data.contains( v.textureName ) )
                {
                    std::string key = v.textureName;
                    data.insert_or_assign( key, std::move( v ) );
                }
                else
                {
                    debug::Warning( "{}: textureName \"{}\" seen not once in the array, ignoring",
                                    filepath.string(),
                                    v.textureName );
                }
            }

            debug::Info( "Reloaded texture meta: {}", filepath.string() );
        }
    };

    reread( sourceGlobal, dataGlobal );
    reread( sourceScene, dataScene );
}

void RTGL1::TextureMetaManager::Modify( RgMeshPrimitiveInfo& prim,
                                        RgEditorInfo&        editor,
                                        bool                 isStatic ) const
{
    assert( prim.pEditorInfo == &editor );

    if( auto meta = Access( prim.pTextureName ) )
    {
        if( meta->forceGenerateNormals )
        {
            prim.flags &= ~RG_MESH_PRIMITIVE_DONT_GENERATE_NORMALS;
        }

        if( meta->forceAlphaTest )
        {
            prim.flags |= RG_MESH_PRIMITIVE_ALPHA_TESTED;
        }

        if( meta->forceTranslucent )
        {
            prim.flags |= RG_MESH_PRIMITIVE_TRANSLUCENT;
        }
        bool isTranslucent =
            ( prim.flags & RG_MESH_PRIMITIVE_TRANSLUCENT ) ||
            ( Utils::UnpackAlphaFromPacked32( prim.color ) < MESH_TRANSLUCENT_ALPHA_THRESHOLD );

        {
            editor.attachedLight.intensity = meta->attachedLightIntensity;
            editor.attachedLight.color     = Utils::PackColor( meta->attachedLightColor[ 0 ],
                                                           meta->attachedLightColor[ 1 ],
                                                           meta->attachedLightColor[ 2 ],
                                                           255 );
            editor.attachedLightExists =
                editor.attachedLight.intensity > 0.0f &&
                !Utils::IsColor4DPacked32Zero< false >( editor.attachedLight.color );
        }

        if( meta->isWater )
        {
            prim.flags |= RG_MESH_PRIMITIVE_WATER;
            prim.flags &= ~RG_MESH_PRIMITIVE_GLASS;
            prim.flags &= ~RG_MESH_PRIMITIVE_MIRROR;
        }
        else if( ( meta->isGlass ) || ( meta->isGlassIfTranslucent && isTranslucent ) )
        {
            prim.flags &= ~RG_MESH_PRIMITIVE_WATER;
            prim.flags |= RG_MESH_PRIMITIVE_GLASS;
            prim.flags &= ~RG_MESH_PRIMITIVE_MIRROR;
        }
        else if( meta->isMirror )
        {
            prim.flags &= ~RG_MESH_PRIMITIVE_WATER;
            prim.flags &= ~RG_MESH_PRIMITIVE_GLASS;
            prim.flags |= RG_MESH_PRIMITIVE_MIRROR;
        }

        prim.emissive = Utils::Saturate( meta->emissiveMult );

        editor.pbrInfoExists = true;
        {
            editor.pbrInfo.metallicDefault  = Utils::Saturate( meta->metallicDefault );
            editor.pbrInfo.roughnessDefault = Utils::Saturate( meta->roughnessDefault );
        }
    }
}

void RTGL1::TextureMetaManager::RereadFromFiles( std::string_view currentSceneName )
{
    RereadFromFiles( databaseFolder / SCENES_FOLDER / currentSceneName / TEXTURES_FILENAME );
}

void RTGL1::TextureMetaManager::OnFileChanged( FileType                     type,
                                               const std::filesystem::path& filepath )
{
    if( type == FileType::JSON )
    {
        if( filepath == sourceGlobal || filepath == sourceScene )
        {
            RereadFromFiles( sourceScene );
        }
    }
}