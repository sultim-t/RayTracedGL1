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

#pragma once

namespace RTGL1
{

namespace detail
{
    template< typename T >
    struct DefaultParams
    {
    };

    template<>
    struct DefaultParams< RgDrawFrameRenderResolutionParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_RENDER_RESOLUTION;

        constexpr static RgDrawFrameRenderResolutionParams value = {
            .sType                = sType,
            .pNext                = nullptr,
            .upscaleTechnique     = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2,
            .sharpenTechnique     = RG_RENDER_SHARPEN_TECHNIQUE_NONE,
            .resolutionMode       = RG_RENDER_RESOLUTION_MODE_QUALITY,
            .customRenderSize     = {},
            .pPixelizedRenderSize = nullptr,
            .resetUpscalerHistory = false,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameIlluminationParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_ILLUMINATION;

        constexpr static RgDrawFrameIlluminationParams value = {
            .sType                                       = sType,
            .pNext                                       = nullptr,
            .maxBounceShadows                            = 2,
            .enableSecondBounceForIndirect               = true,
            .cellWorldSize                               = 1.0f,
            .directDiffuseSensitivityToChange            = 0.5f,
            .indirectDiffuseSensitivityToChange          = 0.2f,
            .specularSensitivityToChange                 = 0.5f,
            .polygonalLightSpotlightFactor               = 2.0f,
            .lightUniqueIdIgnoreFirstPersonViewerShadows = nullptr,
            .lightstyleValuesCount                       = 0,
            .pLightstyleValues                           = nullptr,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameVolumetricParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_VOLUMETRIC;

        constexpr static RgDrawFrameVolumetricParams value = {
            .sType                   = sType,
            .pNext                   = nullptr,
            .enable                  = true,
            .useSimpleDepthBased     = false,
            .volumetricFar           = std::numeric_limits< float >::max(),
            .ambientColor            = { 0.8f, 0.85f, 1.0f },
            .scaterring              = 0.2f,
            .assymetry               = 0.75f,
            .useIlluminationVolume   = false,
            .fallbackSourceColor     = { 0, 0, 0 },
            .fallbackSourceDirection = { 0, -1, 0 },
            .lightMultiplier         = 1.0f,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameTonemappingParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_TONEMAPPING;

        constexpr static RgDrawFrameTonemappingParams value = {
            .sType                = sType,
            .pNext                = nullptr,
            .disableEyeAdaptation = false,
            .ev100Min             = 0.0f,
            .ev100Max             = 10.0f,
            .luminanceWhitePoint  = 10.0f,
            .saturation           = { 0, 0, 0 },
            .crosstalk            = { 1, 1, 1 },
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameBloomParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_BLOOM;

        constexpr static RgDrawFrameBloomParams value = {
            .sType                   = sType,
            .pNext                   = nullptr,
            .bloomIntensity          = 1.0f,
            .inputThreshold          = 4.0f,
            .bloomEmissionMultiplier = 16.0f,
            .lensDirtIntensity       = 2.0f,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameReflectRefractParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_REFLECTREFRACT;

        constexpr static RgDrawFrameReflectRefractParams value = {
            .sType                                      = sType,
            .pNext                                      = nullptr,
            .maxReflectRefractDepth                     = 2,
            .typeOfMediaAroundCamera                    = RgMediaType::RG_MEDIA_TYPE_VACUUM,
            .indexOfRefractionGlass                     = 1.52f,
            .indexOfRefractionWater                     = 1.33f,
            .thinMediaWidth                             = 0.1f,
            .waterWaveSpeed                             = 1.0f,
            .waterWaveNormalStrength                    = 1.0f,
            .waterColor                                 = { 0.3f, 0.73f, 0.63f },
            .acidColor                                  = { 0.0f, 0.66f, 0.55f },
            .acidDensity                                = 10.0f,
            .waterWaveTextureDerivativesMultiplier      = 1.0f,
            .waterTextureAreaScale                      = 1.0f,
            .portalNormalTwirl                          = false,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameSkyParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_SKY;

        constexpr static RgDrawFrameSkyParams value = {
            .sType                       = sType,
            .pNext                       = nullptr,
            .skyType                     = RgSkyType::RG_SKY_TYPE_COLOR,
            .skyColorDefault             = { 199 / 255.0f, 233 / 255.0f, 255 / 255.0f },
            .skyColorMultiplier          = 1000.0f,
            .skyColorSaturation          = 1.0f,
            .skyViewerPosition           = {},
            .pSkyCubemapTextureName      = nullptr,
            .skyCubemapRotationTransform = {},
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameTexturesParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_TEXTURES;

        constexpr static RgDrawFrameTexturesParams value = {
            .sType                  = sType,
            .pNext                  = nullptr,
            .dynamicSamplerFilter   = RG_SAMPLER_FILTER_LINEAR,
            .normalMapStrength      = 1.0f,
            .emissionMapBoost       = 100.0f,
            .emissionMaxScreenColor = 1.5f,
            .minRoughness           = 0.0f,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameLightmapParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_LIGHTMAP;

        constexpr static RgDrawFrameLightmapParams value = {
            .sType                  = sType,
            .pNext                  = nullptr,
            .lightmapScreenCoverage = 0.0f,
        };
    };

    template<>
    struct DefaultParams< RgDrawFramePostEffectsParams >
    {
        constexpr static RgStructureType sType = RG_STRUCTURE_TYPE_POSTEFFECTS;

        constexpr static RgDrawFramePostEffectsParams value = {
            .sType                 = sType,
            .pNext                 = nullptr,
            .pWipe                 = nullptr,
            .pRadialBlur           = nullptr,
            .pChromaticAberration  = nullptr,
            .pInverseBlackAndWhite = nullptr,
            .pHueShift             = nullptr,
            .pDistortedSides       = nullptr,
            .pWaves                = nullptr,
            .pColorTint            = nullptr,
            .pTeleport             = nullptr,
            .pCRT                  = nullptr,
        };
    };

    // Helpers

    inline RgStructureType ReadSType( const void* params )
    {
        constexpr size_t sTypeOffset = 0;

        // clang-format off
        static_assert( offsetof( RgDrawFrameRenderResolutionParams, sType ) == sTypeOffset );
        static_assert( offsetof( RgDrawFrameIlluminationParams,     sType ) == sTypeOffset );
        static_assert( offsetof( RgDrawFrameVolumetricParams,       sType ) == sTypeOffset );
        static_assert( offsetof( RgDrawFrameTonemappingParams,      sType ) == sTypeOffset );
        static_assert( offsetof( RgDrawFrameBloomParams,            sType ) == sTypeOffset );
        static_assert( offsetof( RgDrawFrameReflectRefractParams,   sType ) == sTypeOffset );
        static_assert( offsetof( RgDrawFrameSkyParams,              sType ) == sTypeOffset );
        static_assert( offsetof( RgDrawFrameTexturesParams,         sType ) == sTypeOffset );
        static_assert( offsetof( RgDrawFrameLightmapParams,         sType ) == sTypeOffset );
        static_assert( offsetof( RgDrawFramePostEffectsParams,      sType ) == sTypeOffset );
        
        static_assert( sizeof( RgDrawFrameRenderResolutionParams  ::sType ) == sizeof( RgStructureType ) );
        static_assert( sizeof( RgDrawFrameIlluminationParams      ::sType ) == sizeof( RgStructureType ) );
        static_assert( sizeof( RgDrawFrameVolumetricParams        ::sType ) == sizeof( RgStructureType ) );
        static_assert( sizeof( RgDrawFrameTonemappingParams       ::sType ) == sizeof( RgStructureType ) );
        static_assert( sizeof( RgDrawFrameBloomParams             ::sType ) == sizeof( RgStructureType ) );
        static_assert( sizeof( RgDrawFrameReflectRefractParams    ::sType ) == sizeof( RgStructureType ) );
        static_assert( sizeof( RgDrawFrameSkyParams               ::sType ) == sizeof( RgStructureType ) );
        static_assert( sizeof( RgDrawFrameTexturesParams          ::sType ) == sizeof( RgStructureType ) );
        static_assert( sizeof( RgDrawFrameLightmapParams          ::sType ) == sizeof( RgStructureType ) );
        static_assert( sizeof( RgDrawFramePostEffectsParams       ::sType ) == sizeof( RgStructureType ) );
        // clang-format on

        auto ptr = static_cast< const uint8_t* >( params ) + sTypeOffset;
        return *reinterpret_cast< std::add_pointer_t< const RgStructureType > >( ptr );
    }

    inline void* ReadPNext( void* params )
    {
        constexpr size_t pNextOffset = 8;

        // clang-format off
        static_assert( offsetof( RgDrawFrameRenderResolutionParams, pNext ) == pNextOffset );
        static_assert( offsetof( RgDrawFrameIlluminationParams,     pNext ) == pNextOffset );
        static_assert( offsetof( RgDrawFrameVolumetricParams,       pNext ) == pNextOffset );
        static_assert( offsetof( RgDrawFrameTonemappingParams,      pNext ) == pNextOffset );
        static_assert( offsetof( RgDrawFrameBloomParams,            pNext ) == pNextOffset );
        static_assert( offsetof( RgDrawFrameReflectRefractParams,   pNext ) == pNextOffset );
        static_assert( offsetof( RgDrawFrameSkyParams,              pNext ) == pNextOffset );
        static_assert( offsetof( RgDrawFrameTexturesParams,         pNext ) == pNextOffset );
        static_assert( offsetof( RgDrawFrameLightmapParams,         pNext ) == pNextOffset );
        static_assert( offsetof( RgDrawFramePostEffectsParams,      pNext ) == pNextOffset );
        
        static_assert( sizeof( RgDrawFrameRenderResolutionParams  ::pNext ) == sizeof( void* ) );
        static_assert( sizeof( RgDrawFrameIlluminationParams      ::pNext ) == sizeof( void* ) );
        static_assert( sizeof( RgDrawFrameVolumetricParams        ::pNext ) == sizeof( void* ) );
        static_assert( sizeof( RgDrawFrameTonemappingParams       ::pNext ) == sizeof( void* ) );
        static_assert( sizeof( RgDrawFrameBloomParams             ::pNext ) == sizeof( void* ) );
        static_assert( sizeof( RgDrawFrameReflectRefractParams    ::pNext ) == sizeof( void* ) );
        static_assert( sizeof( RgDrawFrameSkyParams               ::pNext ) == sizeof( void* ) );
        static_assert( sizeof( RgDrawFrameTexturesParams          ::pNext ) == sizeof( void* ) );
        static_assert( sizeof( RgDrawFrameLightmapParams          ::pNext ) == sizeof( void* ) );
        static_assert( sizeof( RgDrawFramePostEffectsParams       ::pNext ) == sizeof( void* ) );
        // clang-format on

        auto ptr = static_cast< uint8_t* >( params ) + pNextOffset;
        return *reinterpret_cast< std::add_pointer_t< void* > >( ptr );
    }

    template< typename T, bool AsConst >
    std::conditional_t< AsConst, const T*, T* > TryAccessParams( void* liststart )
    {
        void* next = liststart;

        while( next )
        {
            RgStructureType sType = ReadSType( next );

            switch( sType )
            {
                case RG_STRUCTURE_TYPE_RENDER_RESOLUTION:
                case RG_STRUCTURE_TYPE_ILLUMINATION:
                case RG_STRUCTURE_TYPE_VOLUMETRIC:
                case RG_STRUCTURE_TYPE_TONEMAPPING:
                case RG_STRUCTURE_TYPE_BLOOM:
                case RG_STRUCTURE_TYPE_REFLECTREFRACT:
                case RG_STRUCTURE_TYPE_SKY:
                case RG_STRUCTURE_TYPE_TEXTURES:
                case RG_STRUCTURE_TYPE_LIGHTMAP:
                case RG_STRUCTURE_TYPE_POSTEFFECTS:
                    // found matching sType
                    if( sType == DefaultParams< T >::sType )
                    {
                        return static_cast< T* >( next );
                    }
                    break;

                case RG_STRUCTURE_TYPE_NONE:
                default:
                    debug::Error( "Found invalid sType: {} on {:#x}",
                                  std::underlying_type_t< RgStructureType >{ sType },
                                  uint64_t( next ) );
            }

            next = ReadPNext( next );
        }

        return static_cast< T* >( nullptr );
    }
}


template< typename T >
const T& AccessParams( const RgDrawFrameInfo& info )
{
    if( auto p = detail::TryAccessParams< T, true >( info.pParams ) )
    {
        return *p;
    }

    // default if not found
    return detail::DefaultParams< T >::value;
}

template< typename T >
T* AccessParamsForWrite( RgDrawFrameInfo& info )
{
    if( auto p = detail::TryAccessParams< T, false >( info.pParams ) )
    {
        return p;
    }

    return nullptr;
}


struct DrawFrameInfoCopy
{
    RgDrawFrameInfo info{};

    explicit DrawFrameInfoCopy( const RgDrawFrameInfo& original ) : info( original )
    {
        // clang-format off
        storage_RenderResolution = AccessParams< RgDrawFrameRenderResolutionParams >( original );
        storage_Illumination     = AccessParams< RgDrawFrameIlluminationParams     >( original );
        storage_Volumetric       = AccessParams< RgDrawFrameVolumetricParams       >( original );
        storage_Tonemapping      = AccessParams< RgDrawFrameTonemappingParams      >( original );
        storage_Bloom            = AccessParams< RgDrawFrameBloomParams            >( original );
        storage_ReflectRefract   = AccessParams< RgDrawFrameReflectRefractParams   >( original );
        storage_Sky              = AccessParams< RgDrawFrameSkyParams              >( original );
        storage_Textures         = AccessParams< RgDrawFrameTexturesParams         >( original );
        storage_Lightmap         = AccessParams< RgDrawFrameLightmapParams         >( original );
        storage_PostEffects      = AccessParams< RgDrawFramePostEffectsParams      >( original );

        storage_RenderResolution.pNext = nullptr;
        storage_Illumination    .pNext = &storage_RenderResolution;
        storage_Volumetric      .pNext = &storage_Illumination;
        storage_Tonemapping     .pNext = &storage_Volumetric;
        storage_Bloom           .pNext = &storage_Tonemapping;
        storage_ReflectRefract  .pNext = &storage_Bloom;
        storage_Sky             .pNext = &storage_ReflectRefract;
        storage_Textures        .pNext = &storage_Sky;
        storage_Lightmap        .pNext = &storage_Textures;
        storage_PostEffects     .pNext = &storage_Lightmap;
        info                    .pParams = &storage_PostEffects;
        // clang-format on
    }

    ~DrawFrameInfoCopy() = default;

    DrawFrameInfoCopy( const DrawFrameInfoCopy& )            = delete;
    DrawFrameInfoCopy( DrawFrameInfoCopy&& )                 = delete;
    DrawFrameInfoCopy& operator=( const DrawFrameInfoCopy& ) = delete;
    DrawFrameInfoCopy& operator=( DrawFrameInfoCopy&& )      = delete;

private:
    RgDrawFrameRenderResolutionParams storage_RenderResolution{};
    RgDrawFrameIlluminationParams     storage_Illumination{};
    RgDrawFrameVolumetricParams       storage_Volumetric{};
    RgDrawFrameTonemappingParams      storage_Tonemapping{};
    RgDrawFrameBloomParams            storage_Bloom{};
    RgDrawFrameReflectRefractParams   storage_ReflectRefract{};
    RgDrawFrameSkyParams              storage_Sky{};
    RgDrawFrameTexturesParams         storage_Textures{};
    RgDrawFrameLightmapParams         storage_Lightmap{};
    RgDrawFramePostEffectsParams      storage_PostEffects{};
};

}
