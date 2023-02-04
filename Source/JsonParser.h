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

#include <array>
#include <filesystem>
#include <string>
#include <optional>
#include <vector>

namespace RTGL1
{

struct LibraryConfig
{
    constexpr static int Version{ 0 };
    constexpr static int RequiredVersion{ 0 };

    bool developerMode    = false;
    bool vulkanValidation = false;
    bool dlssValidation   = false;
    bool fpsMonitor       = false;
};



struct TextureMeta
{
    constexpr static int Version{ 0 };
    constexpr static int RequiredVersion{ 0 };

    std::string textureName = {};

    bool forceIgnore      = false;
    bool forceAlphaTest   = false;
    bool forceTranslucent = false;
    bool forceOpaque      = false;

    bool forceGenerateNormals = false;
    bool forceExactNormals    = false;

    bool isMirror             = false;
    bool isWater              = false;
    bool isWaterIfTranslucent = false;
    bool isGlass              = false;
    bool isGlassIfTranslucent = false;
    bool isAcid               = false;

    bool isGlassIfSmooth  = false;
    bool isMirrorIfSmooth = false;

    bool isThinMedia = false;

    float metallicDefault  = 0.0f;
    float roughnessDefault = 1.0f;
    float emissiveMult     = 0.0f;

    float                    attachedLightIntensity = 0.0f;
    std::array< uint8_t, 3 > attachedLightColor     = { { 255, 255, 255 } };
};

struct TextureMetaArray
{
    constexpr static int Version{ 0 };
    constexpr static int RequiredVersion{ 0 };

    std::vector< TextureMeta > array;
};



struct SceneMeta
{
    constexpr static int Version{ 0 };
    constexpr static int RequiredVersion{ 0 };

    std::string sceneName = {};

    std::optional< float > sky;

    std::optional< float > scatter;
    std::optional< float > volumeFar;
    std::optional< float > volumeAssymetry;
    std::optional< float > volumeLightMultiplier;

    std::optional< std::array< float, 3 > > volumeAmbient;
};

struct SceneMetaArray
{
    constexpr static int Version{ 0 };
    constexpr static int RequiredVersion{ 0 };

    std::vector< SceneMeta > array;
};



struct PrimitiveExtraInfo
{
    int isGlass         = 0;
    int isSkyVisibility = 0;
};



namespace json_parser
{
    namespace detail
    {
        auto ReadTextureMetaArray( const std::filesystem::path& path )
            -> std::optional< TextureMetaArray >;

        auto ReadSceneMetaArray( const std::filesystem::path& path )
            -> std::optional< SceneMetaArray >;

        auto ReadLibraryConfig( const std::filesystem::path& path )
            -> std::optional< LibraryConfig >;

        auto ReadLightExtraInfo( const std::string_view& data ) -> RgLightExtraInfo;

        auto ReadPrimitiveExtraInfo( const std::string_view& data ) -> PrimitiveExtraInfo;
    }



    template< typename T >
    auto ReadFileAs( const std::filesystem::path& path ) = delete;

    template<>
    inline auto ReadFileAs< TextureMetaArray >( const std::filesystem::path& path )
    {
        return detail::ReadTextureMetaArray( path );
    }

    template<>
    inline auto ReadFileAs< SceneMetaArray >( const std::filesystem::path& path )
    {
        return detail::ReadSceneMetaArray( path );
    }

    template<>
    inline auto ReadFileAs< LibraryConfig >( const std::filesystem::path& path )
    {
        return detail::ReadLibraryConfig( path );
    }



    template< typename T >
    auto ReadStringAs( const std::string_view& str ) = delete;

    template<>
    inline auto ReadStringAs< RgLightExtraInfo >( const std::string_view& data )
    {
        return detail::ReadLightExtraInfo( data );
    }

    template<>
    inline auto ReadStringAs< PrimitiveExtraInfo >( const std::string_view& data )
    {
        return detail::ReadPrimitiveExtraInfo( data );
    }



    std::string MakeJsonString( const RgLightExtraInfo& info );
}

}
