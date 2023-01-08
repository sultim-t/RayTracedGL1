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

namespace RTGL1
{
struct Devmode
{
    struct DebugPrim
    {
        std::optional< UploadResult > result;
        uint32_t                      callIndex;
        uint32_t                      objectId;
        std::string                   meshName;
        uint32_t                      primitiveIndex;
        std::string                   primitiveName;
        std::string                   textureName;
    };

    enum class DebugPrimMode : int
    {
        None,
        RayTraced,
        Rasterized,
        NonWorld,
        Decal,
    };

    bool     debugWindowOnTop{ false };
    bool     reloadShaders{ false };
    uint32_t debugShowFlags{ 0 };

    struct
    {
        bool enable;

        bool  antiFirefly;
        int   maxBounceShadows;
        bool  enableSecondBounceForIndirect;
        float directDiffuseSensitivityToChange;
        float indirectDiffuseSensitivityToChange;
        float specularSensitivityToChange;

        float ev100Min;
        float ev100Max;
        float saturation[ 3 ];
        float crosstalk[ 3 ];

        float                    fovDeg;
        bool                     vsync;
        RgRenderUpscaleTechnique upscaleTechnique;
        RgRenderSharpenTechnique sharpenTechnique;
        RgRenderResolutionMode   resolutionMode;
        int                      customRenderSize[ 2 ];
        bool                     pixelizedEnable;
        int                      pixelized[ 2 ];
        RgExtent2D               pixelizedForPtr;

        float lightmapScreenCoverage;

    } drawInfoOvrd;
    struct
    {
        RgDrawFrameInfo                   c{};
        RgDrawFrameRenderResolutionParams c_RenderResolution{};
        RgDrawFrameIlluminationParams     c_Illumination{};
        RgDrawFrameVolumetricParams       c_Volumetric{};
        RgDrawFrameTonemappingParams      c_Tonemapping{};
        RgDrawFrameBloomParams            c_Bloom{};
        RgDrawFrameReflectRefractParams   c_ReflectRefract{};
        RgDrawFrameSkyParams              c_Sky{};
        RgDrawFrameTexturesParams         c_Textures{};
        RgDrawFrameLightmapParams         c_Lightmap{};
    } drawInfoCopy;

    bool materialsTableEnable{ false };

    DebugPrimMode            primitivesTableMode{ DebugPrimMode::None };
    std::vector< DebugPrim > primitivesTable{};

    RgMessageSeverityFlags logFlags{ RG_MESSAGE_SEVERITY_VERBOSE | RG_MESSAGE_SEVERITY_INFO |
                                     RG_MESSAGE_SEVERITY_WARNING | RG_MESSAGE_SEVERITY_ERROR };
    bool                   logAutoScroll{ true };
    bool                   logCompact{ true };
    std::deque< std::tuple< RgMessageSeverityFlags, std::string, uint64_t /* str hash */ > > logs{};
};

namespace detail
{
    template< typename T >
    struct DefaultParams
    {
    };

    template<>
    struct DefaultParams< RgDrawFrameRenderResolutionParams >
    {
        constexpr static RgDrawFrameRenderResolutionParams value = {
            .upscaleTechnique     = RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2,
            .sharpenTechnique     = RG_RENDER_SHARPEN_TECHNIQUE_NONE,
            .resolutionMode       = RG_RENDER_RESOLUTION_MODE_QUALITY,
            .customRenderSize     = {},
            .pPixelizedRenderSize = nullptr,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameIlluminationParams >
    {
        constexpr static RgDrawFrameIlluminationParams value = {
            .maxBounceShadows                            = 2,
            .enableSecondBounceForIndirect               = true,
            .cellWorldSize                               = 1.0f,
            .directDiffuseSensitivityToChange            = 0.5f,
            .indirectDiffuseSensitivityToChange          = 0.2f,
            .specularSensitivityToChange                 = 0.5f,
            .polygonalLightSpotlightFactor               = 2.0f,
            .lightUniqueIdIgnoreFirstPersonViewerShadows = nullptr,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameVolumetricParams >
    {
        constexpr static RgDrawFrameVolumetricParams value = {
            .enable              = true,
            .useSimpleDepthBased = false,
            .volumetricFar       = std::numeric_limits< float >::max(),
            .ambientColor        = { 0.8f, 0.85f, 1.0f },
            .scaterring          = 0.2f,
            .sourceColor         = { 0, 0, 0 },
            .sourceDirection     = { 0, 1, 0 },
            .sourceAssymetry     = 0.75f,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameTonemappingParams >
    {
        constexpr static RgDrawFrameTonemappingParams value = {
            .ev100Min            = 0.0f,
            .ev100Max            = 10.0f,
            .luminanceWhitePoint = 10.0f,
            .saturation          = { 0, 0, 0 },
            .crosstalk           = { 1, 1, 1 },
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameBloomParams >
    {
        constexpr static RgDrawFrameBloomParams value = {
            .bloomIntensity          = 1.0f,
            .inputThreshold          = 4.0f,
            .bloomEmissionMultiplier = 16.0f,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameReflectRefractParams >
    {
        constexpr static RgDrawFrameReflectRefractParams value = {
            .maxReflectRefractDepth                     = 2,
            .typeOfMediaAroundCamera                    = RgMediaType::RG_MEDIA_TYPE_VACUUM,
            .indexOfRefractionGlass                     = 1.52f,
            .indexOfRefractionWater                     = 1.33f,
            .forceNoWaterRefraction                     = false,
            .waterWaveSpeed                             = 1.0f,
            .waterWaveNormalStrength                    = 1.0f,
            .waterColor                                 = { 0.3f, 0.73f, 0.63f },
            .acidColor                                  = { 0.0f, 0.66f, 0.55f },
            .acidDensity                                = 10.0f,
            .waterWaveTextureDerivativesMultiplier      = 1.0f,
            .waterTextureAreaScale                      = 1.0f,
            .disableBackfaceReflectionsForNoMediaChange = false,
            .portalNormalTwirl                          = false,
        };
    };

    template<>
    struct DefaultParams< RgDrawFrameSkyParams >
    {
        constexpr static RgDrawFrameSkyParams value = {
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
        constexpr static RgDrawFrameTexturesParams value = {
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
        constexpr static RgDrawFrameLightmapParams value = {
            .lightmapScreenCoverage = 0.0f,
        };
    };
}

template< typename T >
constexpr const T& AccessParams( const T* originalParams )
{
    if( originalParams == nullptr )
    {
        return detail::DefaultParams< T >::value;
    }
    return *originalParams;
}

}