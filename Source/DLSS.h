// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include <vector>

#include "Framebuffers.h"

struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;

namespace RTGL1
{


class RenderResolutionHelper;


class DLSS
{
public:
    DLSS( VkInstance       instance,
          VkDevice         device,
          VkPhysicalDevice physDevice,
          const char*      pAppGuid,
          bool             enableDebug );
    ~DLSS();

    DLSS( const DLSS& other )                = delete;
    DLSS( DLSS&& other ) noexcept            = delete;
    DLSS& operator=( const DLSS& other )     = delete;
    DLSS& operator=( DLSS&& other ) noexcept = delete;

    FramebufferImageIndex Apply( VkCommandBuffer                        cmd,
                                 uint32_t                               frameIndex,
                                 const std::shared_ptr< Framebuffers >& framebuffers,
                                 const RenderResolutionHelper&          renderResolution,
                                 RgFloat2D                              jitterOffset,
                                 bool                                   resetAccumulation );

    void GetOptimalSettings( uint32_t               userWidth,
                             uint32_t               userHeight,
                             RgRenderResolutionMode mode,
                             uint32_t*              pOutWidth,
                             uint32_t*              pOutHeight,
                             float*                 pOutSharpness ) const;

    bool IsDlssAvailable() const;

    static std::vector< const char* > GetDlssVulkanInstanceExtensions();
    static std::vector< const char* > GetDlssVulkanDeviceExtensions();

private:
    bool TryInit( VkInstance       instance,
                  VkDevice         device,
                  VkPhysicalDevice physDevice,
                  const char*      pAppGuid,
                  bool             enableDebug );
    bool CheckSupport() const;
    void DestroyDlssFeature();
    void Destroy();

    bool AreSameDlssFeatureValues( const RenderResolutionHelper& renderResolution ) const;
    void SaveDlssFeatureValues( const RenderResolutionHelper& renderResolution );

    bool ValidateDlssFeature( VkCommandBuffer cmd, const RenderResolutionHelper& renderResolution );

private:
    VkDevice device;

    bool                 isInitialized;
    NVSDK_NGX_Parameter* pParams;
    NVSDK_NGX_Handle*    pDlssFeature;

    struct PrevDlssFeatureValues
    {
        uint32_t renderWidth;
        uint32_t renderHeight;
        uint32_t upscaledWidth;
        uint32_t upscaledHeight;
    } prevDlssFeatureValues;
};

}
