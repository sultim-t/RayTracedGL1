// Copyright (c) 2023 Sultim Tsyrendashiev
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

#include "SceneMeta.h"

#include "Utils.h"

RTGL1::SceneMetaManager::SceneMetaManager( std::filesystem::path filepath )
    : metafile( std::move( filepath ) )
{
}

void RTGL1::SceneMetaManager::Modify( std::string_view             sceneName,
                                      RgDrawFrameVolumetricParams& volumetric,
                                      RgDrawFrameSkyParams&        sky ) const
{
    auto iter = data.find( std::string( sceneName ) );
    if( iter == data.end() )
    {
        return;
    }
    assert( iter->first == sceneName );

    const SceneMeta& m = iter->second;

    if( m.scatter )
    {
        volumetric.scaterring = *m.scatter;
    }

    if( m.sky )
    {
        sky.skyColorMultiplier = *m.sky;
    }

    if( m.volumeFar )
    {
        volumetric.volumetricFar = *m.volumeFar;
    }

    if( m.volumeAssymetry )
    {
        volumetric.assymetry = std::clamp( *m.volumeAssymetry, 0.0f, 1.0f );
    }

    if( m.volumeAmbient )
    {
        volumetric.ambientColor = { RG_ACCESS_VEC3( *m.volumeAmbient ) };
    }
}

void RTGL1::SceneMetaManager::OnFileChanged( FileType type, const std::filesystem::path& filepath )
{
    if( type == FileType::JSON )
    {
        if( metafile == filepath )
        {
            data.clear();

            if( auto arr = json_parser::ReadFileAs< SceneMetaArray >( metafile ) )
            {
                for( SceneMeta& m : arr->array )
                {
                    if( !data.contains( m.sceneName ) )
                    {
                        std::string key = m.sceneName;
                        data.insert_or_assign( key, std::move( m ) );
                    }
                    else
                    {
                        debug::Warning( "{}: sceneName \"{}\" seen not once in the array, ignoring",
                                        metafile.string(),
                                        m.sceneName );
                    }
                }

                debug::Info( "Reloaded scene meta: {}", metafile.string() );
            }
        }
    }
}
