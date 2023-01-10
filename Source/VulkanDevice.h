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

#include <RTGL1/RTGL1.h>

#include <memory>

#include "Common.h"

#include "CommandBufferManager.h"
#include "PhysicalDevice.h"
#include "Scene.h"
#include "Swapchain.h"
#include "Queues.h"
#include "GlobalUniform.h"
#include "PathTracer.h"
#include "Rasterizer.h"
#include "Framebuffers.h"
#include "MemoryAllocator.h"
#include "TextureManager.h"
#include "BlueNoise.h"
#include "ImageComposition.h"
#include "Tonemapping.h"
#include "CubemapManager.h"
#include "Denoiser.h"
#include "UserFunction.h"
#include "Bloom.h"
#include "Sharpening.h"
#include "DLSS.h"
#include "RenderResolutionHelper.h"
#include "DecalManager.h"
#include "EffectWipe.h"
#include "EffectSimple_Instances.h"
#include "LightGrid.h"
#include "FSR2.h"
#include "FrameState.h"
#include "PortalList.h"
#include "RestirBuffers.h"
#include "Volumetric.h"
#include "DebugWindows.h"
#include "ScratchImmediate.h"
#include "FolderObserver.h"
#include "TextureMeta.h"
#include "VulkanDevice_Dev.h"

namespace RTGL1
{

struct Devmode;


class VulkanDevice
{
public:
    explicit VulkanDevice( const RgInstanceCreateInfo* pInfo );
    ~VulkanDevice();

    VulkanDevice( const VulkanDevice& other )                = delete;
    VulkanDevice( VulkanDevice&& other ) noexcept            = delete;
    VulkanDevice& operator=( const VulkanDevice& other )     = delete;
    VulkanDevice& operator=( VulkanDevice&& other ) noexcept = delete;

    void UploadMeshPrimitive( const RgMeshInfo* pMesh, const RgMeshPrimitiveInfo* pPrimitive );
    void UploadNonWorldPrimitive( const RgMeshPrimitiveInfo* pPrimitive,
                                  const float*               pViewProjection,
                                  const RgViewport*          pViewport );
    void UploadDecal( const RgDecalUploadInfo* pInfo );
    void UploadLensFlare( const RgLensFlareUploadInfo* pInfo );

    void UploadDirectionalLight( const RgDirectionalLightUploadInfo* pInfo );
    void UploadSphericalLight( const RgSphericalLightUploadInfo* pInfo );
    void UploadSpotlight( const RgSpotLightUploadInfo* pInfo );
    void UploadPolygonalLight( const RgPolygonalLightUploadInfo* pInfo );
    void UploadLight( const GenericLightPtr& light );

    void ProvideOriginalTexture( const RgOriginalTextureInfo* pInfo );
    void ProvideOriginalCubemapTexture( const RgOriginalCubemapInfo* pInfo );
    void MarkOriginalTextureAsDeleted( const char* pTextureName );

    void StartFrame( const char* pMapName );
    void DrawFrame( const RgDrawFrameInfo* pInfo );


    bool IsSuspended() const;
    bool IsUpscaleTechniqueAvailable( RgRenderUpscaleTechnique technique ) const;

    RgPrimitiveVertex* ScratchAllocForVertices( uint32_t count );
    void               ScratchFree( const RgPrimitiveVertex* pPointer );

    inline void ScratchGetIndices( RgUtilImScratchTopology topology,
                                   uint32_t                vertexCount,
                                   const uint32_t**        ppOutIndices,
                                   uint32_t*               pOutIndexCount );
    inline void ImScratchClear();
    inline void ImScratchStart( RgUtilImScratchTopology topology );
    inline void ImScratchEnd();
    inline void ImScratchVertex( const float& x, const float& y, const float& z );
    inline void ImScratchNormal( const float& x, const float& y, const float& z );
    inline void ImScratchTexCoord( const float& u, const float& v );
    inline void ImScratchTexCoord_Layer1( const float& u, const float& v );
    inline void ImScratchTexCoord_Layer2( const float& u, const float& v );
    inline void ImScratchTexCoord_LayerLightmap( const float& u, const float& v );
    inline void ImScratchColor( const RgColor4DPacked32& color );
    inline void ImScratchSetToPrimitive( RgMeshPrimitiveInfo* pTarget );


    void Print( std::string_view msg, RgMessageSeverityFlags severity ) const;

private:
    void CreateInstance( const RgInstanceCreateInfo& info );
    void CreateDevice();
    void CreateSyncPrimitives();
    void ValidateCreateInfo( const RgInstanceCreateInfo* pInfo ) const;

    void DestroyInstance();
    void DestroyDevice();
    void DestroySyncPrimitives();

    void FillUniform( ShGlobalUniform* gu, const RgDrawFrameInfo& drawInfo ) const;

    VkCommandBuffer BeginFrame( const char* pMapName );
    void            Render( VkCommandBuffer cmd, const RgDrawFrameInfo& drawInfo );
    void            EndFrame( VkCommandBuffer cmd );

private:
    void                   Dev_Draw() const;
    const RgDrawFrameInfo& Dev_Override( const RgDrawFrameInfo& original ) const;
    void                   Dev_TryBreak( const char* pTextureName, bool isImageUpload );

private:
    VkInstance   instance;
    VkDevice     device;
    VkSurfaceKHR surface;

    FrameState currentFrameState;

    // incremented every frame
    uint32_t frameId;

    VkFence     frameFences[ MAX_FRAMES_IN_FLIGHT ]              = {};
    VkSemaphore imageAvailableSemaphores[ MAX_FRAMES_IN_FLIGHT ] = {};
    VkSemaphore renderFinishedSemaphores[ MAX_FRAMES_IN_FLIGHT ] = {};
    VkSemaphore inFrameSemaphores[ MAX_FRAMES_IN_FLIGHT ]        = {};

    bool    waitForOutOfFrameFence;
    VkFence outOfFrameFences[ MAX_FRAMES_IN_FLIGHT ] = {};

    std::shared_ptr< PhysicalDevice > physDevice;
    std::shared_ptr< Queues >         queues;
    std::shared_ptr< Swapchain >      swapchain;

    std::shared_ptr< MemoryAllocator > memAllocator;

    std::shared_ptr< CommandBufferManager > cmdManager;

    std::shared_ptr< Framebuffers >  framebuffers;
    std::shared_ptr< RestirBuffers > restirBuffers;
    std::shared_ptr< Volumetric >    volumetric;

    std::shared_ptr< GlobalUniform >     uniform;
    std::shared_ptr< Scene >             scene;
    std::shared_ptr< SceneImportExport > sceneImportExport;

    std::shared_ptr< ShaderManager >             shaderManager;
    std::shared_ptr< RayTracingPipeline >        rtPipeline;
    std::shared_ptr< PathTracer >                pathTracer;
    std::shared_ptr< Rasterizer >                rasterizer;
    std::shared_ptr< DecalManager >              decalManager;
    std::shared_ptr< PortalList >                portalList;
    std::shared_ptr< LightManager >              lightManager;
    std::shared_ptr< LightGrid >                 lightGrid;
    std::shared_ptr< Denoiser >                  denoiser;
    std::shared_ptr< Tonemapping >               tonemapping;
    std::shared_ptr< ImageComposition >          imageComposition;
    std::shared_ptr< Bloom >                     bloom;
    std::shared_ptr< FSR2 >                      amdFsr2;
    std::shared_ptr< DLSS >                      nvDlss;
    std::shared_ptr< Sharpening >                sharpening;
    std::shared_ptr< EffectWipe >                effectWipe;
    std::shared_ptr< EffectRadialBlur >          effectRadialBlur;
    std::shared_ptr< EffectChromaticAberration > effectChromaticAberration;
    std::shared_ptr< EffectInverseBW >           effectInverseBW;
    std::shared_ptr< EffectHueShift >            effectHueShift;
    std::shared_ptr< EffectDistortedSides >      effectDistortedSides;
    std::shared_ptr< EffectWaves >               effectWaves;
    std::shared_ptr< EffectColorTint >           effectColorTint;
    std::shared_ptr< EffectCrtDemodulateEncode > effectCrtDemodulateEncode;
    std::shared_ptr< EffectCrtDecode >           effectCrtDecode;

    std::shared_ptr< SamplerManager >     worldSamplerManager;
    std::shared_ptr< SamplerManager >     genericSamplerManager;
    std::shared_ptr< BlueNoise >          blueNoise;
    std::shared_ptr< TextureManager >     textureManager;
    std::shared_ptr< TextureMetaManager > textureMetaManager;
    std::shared_ptr< CubemapManager >     cubemapManager;

    std::filesystem::path ovrdFolder;

    LibraryConfig                     libconfig;
    VkDebugUtilsMessengerEXT          debugMessenger;
    std::unique_ptr< UserPrint >      userPrint;
    std::shared_ptr< DebugWindows >   debugWindows;
    ScratchImmediate                  scratchImmediate;
    std::unique_ptr< FolderObserver > observer;

    std::unique_ptr< Devmode > devmode;

    bool rayCullBackFacingTriangles;
    bool allowGeometryWithSkyFlag;

    RenderResolutionHelper renderResolution;

    double previousFrameTime;
    double currentFrameTime;

    bool vsync;
};

}


inline void RTGL1::VulkanDevice::ScratchGetIndices( RgUtilImScratchTopology topology,
                                                    uint32_t                vertexCount,
                                                    const uint32_t**        ppOutIndices,
                                                    uint32_t*               pOutIndexCount )
{
    const auto indices = scratchImmediate.GetIndices( topology, vertexCount );

    *ppOutIndices   = indices.data();
    *pOutIndexCount = uint32_t( indices.size() );
}

inline void RTGL1::VulkanDevice::ImScratchClear()
{
    scratchImmediate.Clear();
}

inline void RTGL1::VulkanDevice::ImScratchStart( RgUtilImScratchTopology topology )
{
    scratchImmediate.StartPrimitive( topology );
}

inline void RTGL1::VulkanDevice::ImScratchEnd()
{
    scratchImmediate.EndPrimitive();
}

inline void RTGL1::VulkanDevice::ImScratchVertex( const float& x, const float& y, const float& z )
{
    scratchImmediate.Vertex( x, y, z );
}

inline void RTGL1::VulkanDevice::ImScratchNormal( const float& x, const float& y, const float& z )
{
    scratchImmediate.Normal( x, y, z );
}

inline void RTGL1::VulkanDevice::ImScratchTexCoord( const float& u, const float& v )
{
    scratchImmediate.TexCoord( u, v );
}

inline void RTGL1::VulkanDevice::ImScratchTexCoord_Layer1( const float& u, const float& v )
{
    scratchImmediate.TexCoord_Layer1( u, v );
}

inline void RTGL1::VulkanDevice::ImScratchTexCoord_Layer2( const float& u, const float& v )
{
    scratchImmediate.TexCoord_Layer2( u, v );
}

inline void RTGL1::VulkanDevice::ImScratchTexCoord_LayerLightmap( const float& u, const float& v )
{
    scratchImmediate.TexCoord_LayerLightmap( u, v );
}

inline void RTGL1::VulkanDevice::ImScratchColor( const RgColor4DPacked32& color )
{
    scratchImmediate.Color( color );
}

inline void RTGL1::VulkanDevice::ImScratchSetToPrimitive( RgMeshPrimitiveInfo* pTarget )
{
    scratchImmediate.SetToPrimitive( pTarget );
}
