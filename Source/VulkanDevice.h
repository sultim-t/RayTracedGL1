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
#include "LibraryConfig.h"
#include "PortalList.h"
#include "RestirBuffers.h"
#include "Volumetric.h"
#include "DebugWindows.h"
#include "ScratchImmediate.h"
#include "FolderObserver.h"
#include "TextureMeta.h"

namespace RTGL1
{

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

    void UploadDirectionalLight( const RgDirectionalLightUploadInfo* pInfo );
    void UploadSphericalLight( const RgSphericalLightUploadInfo* pInfo );
    void UploadSpotlight( const RgSpotLightUploadInfo* pInfo );
    void UploadPolygonalLight( const RgPolygonalLightUploadInfo* pInfo );

    void ProvideOriginalTexture( const RgOriginalTextureInfo* pInfo );
    void ProvideOriginalCubemapTexture( const RgOriginalCubemapInfo* pInfo );
    void MarkOriginalTextureAsDeleted( const char* pTextureName );

    void StartFrame();
    void DrawFrame( const RgDrawFrameInfo* pInfo );


    bool IsSuspended() const;
    bool IsUpscaleTechniqueAvailable( RgRenderUpscaleTechnique technique ) const;

    RgPrimitiveVertex* ScratchAllocForVertices( uint32_t count );
    void               ScratchFree( const RgPrimitiveVertex* pPointer );
    void               ScratchGetIndices( RgUtilImScratchTopology topology,
                                          uint32_t                vertexCount,
                                          const uint32_t**        ppOutIndices,
                                          uint32_t*               pOutIndexCount );

    void ImScratchClear();
    void ImScratchStart( RgUtilImScratchTopology topology );
    void ImScratchEnd();
    void ImScratchVertex( const float& x, const float& y, const float& z );
    void ImScratchTexCoord( const float& u, const float& v );
    void ImScratchColor( const RgColor4DPacked32& color );
    void ImScratchSetToPrimitive( RgMeshPrimitiveInfo* pTarget );


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
    void DrawDebugWindows() const;

    VkCommandBuffer BeginFrame();
    void            Render( VkCommandBuffer cmd, const RgDrawFrameInfo& drawInfo );
    void            EndFrame( VkCommandBuffer cmd );

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

    LibraryConfig::Config             libconfig;
    VkDebugUtilsMessengerEXT          debugMessenger;
    std::unique_ptr< UserPrint >      userPrint;
    std::shared_ptr< DebugWindows >   debugWindows;
    ScratchImmediate                  scratchImmediate;
    std::unique_ptr< FolderObserver > observer;

    struct DebugPrim
    {
        UploadResult result;
        uint32_t     callIndex;
        uint32_t     objectId;
        std::string  meshName;
        uint32_t     primitiveIndex;
        std::string  primitiveName;
        std::string  textureName;
    };
    struct DebugNonWorld
    {
        uint32_t    callIndex;
        std::string textureName;
    };
    mutable struct
    {
        bool                         debugWindowOnTop{ false };
        bool                         overrideDrawInfo{ false };
        bool                         ovrdVsync{ false };
        bool                         reloadShaders{ false };
        uint32_t                     debugShowFlags{ 0 };
        bool                         materialsTableEnable{ false };
        bool                         primitivesTableEnable{ false };
        std::vector< DebugPrim >     primitivesTable{};
        bool                         nonworldTableEnable{ false };
        std::vector< DebugNonWorld > nonworldTable{};
        RgMessageSeverityFlags logFlags{ RG_MESSAGE_SEVERITY_VERBOSE | RG_MESSAGE_SEVERITY_INFO |
                                         RG_MESSAGE_SEVERITY_WARNING | RG_MESSAGE_SEVERITY_ERROR };
        std::vector< std::pair< RgMessageSeverityFlags, std::string > > logs{};
    } debugData;

    bool rayCullBackFacingTriangles;
    bool allowGeometryWithSkyFlag;

    RenderResolutionHelper renderResolution;

    double previousFrameTime;
    double currentFrameTime;

    bool vsync;
};

}
