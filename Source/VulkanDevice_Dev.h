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

    bool antiFirefly{ true };

    struct
    {
        bool enable;

        int   maxBounceShadows;
        bool  enableSecondBounceForIndirect;
        float directDiffuseSensitivityToChange;
        float indirectDiffuseSensitivityToChange;
        float specularSensitivityToChange;

        bool  disableEyeAdaptation;
        float ev100Min;
        float ev100Max;
        float saturation[ 3 ];
        float crosstalk[ 3 ];

        float                    fovDeg;
        bool                     vsync;
        RgRenderUpscaleTechnique upscaleTechnique;
        RgRenderSharpenTechnique sharpenTechnique;
        RgRenderResolutionMode   resolutionMode;
        float                    customRenderSizeScale;
        bool                     pixelizedEnable;
        int                      pixelized[ 2 ];
        RgExtent2D               pixelizedForPtr;

        float lightmapScreenCoverage;

    } drawInfoOvrd;

    bool ignoreExternalGeometry{ false };

    bool materialsTableEnable{ false };

    DebugPrimMode            primitivesTableMode{ DebugPrimMode::None };
    std::vector< DebugPrim > primitivesTable{};

    bool breakOnTexturePrimitive{ false };
    bool breakOnTextureImage{ false };
    char breakOnTexture[ 256 ];

    RgMessageSeverityFlags logFlags{ RG_MESSAGE_SEVERITY_VERBOSE | RG_MESSAGE_SEVERITY_INFO |
                                     RG_MESSAGE_SEVERITY_WARNING | RG_MESSAGE_SEVERITY_ERROR };
    bool                   logAutoScroll{ true };
    bool                   logCompact{ true };
    std::deque< std::tuple< RgMessageSeverityFlags, std::string, uint64_t /* str hash */ > > logs{};
};

}
