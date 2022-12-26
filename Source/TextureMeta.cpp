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

#include "JsonReader.inl"


// clang-format off
JSON_TYPE( RTGL1::TextureMeta )

    "textureName", &T::textureName,
    "forceIgnore", &T::forceIgnore,
    "forceAlphaTest", &T::forceAlphaTest,
    "forceRasterized", &T::forceRasterized,
    "isMirror", &T::isMirror,
    "isWater", &T::isWater,
    "isGlass", &T::isGlass,
    "emissive", &T::emissive,
    "attachedLightIntensity", &T::attachedLightIntensity,
    "attachedLightColor", &T::attachedLightColor

JSON_TYPE_END;
// clang-format on


// clang-format off
JSON_TYPE( RTGL1::TextureMetaArray )

    "array", &T::array

JSON_TYPE_END;
// clang-format on


namespace
{
std::string_view TEXTURES_FILENAME = "textures.json";
}

RTGL1::TextureMetaManager::TextureMetaManager( std::filesystem::path _databaseFolder )
    : databaseFolder( std::move( _databaseFolder ) )
{
    sourceGlobal = databaseFolder / TEXTURES_FILENAME;
}

void RTGL1::TextureMetaManager::RereadFromFiles( std::filesystem::path sceneFile )
{
    sourceScene = std::move( sceneFile );

    dataGlobal.clear();
    dataScene.clear();

    auto reread = [ this ]( const std::filesystem::path filepath, auto& data ) {
        if( !std::filesystem::exists( filepath ) )
        {
            return;
        }

        if( auto arr = json_reader::LoadFileAs< TextureMetaArray >( filepath ) )
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
        }
    };

    reread( sourceGlobal, dataGlobal );
    reread( sourceScene, dataScene );
}

void RTGL1::TextureMetaManager::RereadFromFiles( std::string_view currentSceneName )
{
    RereadFromFiles( databaseFolder / SCENES_FOLDER / currentSceneName /
                     TEXTURES_FILENAME );
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