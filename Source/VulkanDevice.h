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
#include "SuperResolution.h"
#include "Sharpening.h"
#include "DLSS.h"
#include "RenderResolutionHelper.h"
#include "DecalManager.h"
#include "EffectWipe.h"
#include "EffectSimple_Instances.h"

namespace RTGL1
{

class VulkanDevice
{
public:
    explicit VulkanDevice(const RgInstanceCreateInfo *pInfo);
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice& other) = delete;
    VulkanDevice(VulkanDevice&& other) noexcept = delete;
    VulkanDevice& operator=(const VulkanDevice& other) = delete;
    VulkanDevice& operator=(VulkanDevice&& other) noexcept = delete;

    void UploadGeometry(const RgGeometryUploadInfo *pUploadInfo);
    void UpdateGeometryTransform(const RgUpdateTransformInfo *pUpdateInfo);
    void UpdateGeometryTexCoords(const RgUpdateTexCoordsInfo *pUpdateInfo);

    void UploadRasterizedGeometry(const RgRasterizedGeometryUploadInfo *pUploadInfo,
                                  const float *pViewProjection, const RgViewport *pViewport);
    void UploadLensFlare(const RgLensFlareUploadInfo *pUploadInfo);
    void UploadDecal(const RgDecalUploadInfo *pUploadInfo);

    void SubmitStaticGeometries();
    void StartNewStaticScene();

    void UploadLight(const RgDirectionalLightUploadInfo *pLightInfo);
    void UploadLight(const RgSphericalLightUploadInfo *pLightInfo);
    void UploadLight(const RgSpotlightUploadInfo *pLightInfo);
    void UploadLight(const RgPolygonalLightUploadInfo *pLightInfo);

    void SetPotentialVisibility(SectorID sectorID_A, SectorID sectorID_B);

    void CreateStaticMaterial(const RgStaticMaterialCreateInfo *pCreateInfo, RgMaterial *pResult);
    void CreateAnimatedMaterial(const RgAnimatedMaterialCreateInfo *pCreateInfo, RgMaterial *pResult);
    void ChangeAnimatedMaterialFrame(RgMaterial animatedMaterial, uint32_t frameIndex);
    void CreateDynamicMaterial(const RgDynamicMaterialCreateInfo *pCreateInfo, RgMaterial *pResult);
    void UpdateDynamicMaterial(const RgDynamicMaterialUpdateInfo *pUpdateInfo);
    void DestroyMaterial(RgMaterial material);

    void CreateSkyboxCubemap(const RgCubemapCreateInfo *pCreateInfo, RgCubemap *pResult);
    void DestroyCubemap(RgCubemap cubemap);


    void StartFrame(const RgStartFrameInfo *pStartInfo);
    void DrawFrame(const RgDrawFrameInfo *pFrameInfo);


    bool IsRenderUpscaleTechniqueAvailable(RgRenderUpscaleTechnique technique) const;


    void Print(const char *pMessage) const;

private:
    void CreateInstance(const RgInstanceCreateInfo &info);
    void CreateDevice();
    void CreateSyncPrimitives();
    static VkSurfaceKHR GetSurfaceFromUser(VkInstance instance, const RgInstanceCreateInfo &info);
    void ValidateCreateInfo(const RgInstanceCreateInfo *pInfo);

    void DestroyInstance();
    void DestroyDevice();
    void DestroySyncPrimitives();

    void FillUniform(ShGlobalUniform *gu, const RgDrawFrameInfo &drawInfo) const;

    VkCommandBuffer BeginFrame(const RgStartFrameInfo &startInfo);
    void Render(VkCommandBuffer cmd, const RgDrawFrameInfo &drawInfo);
    void EndFrame(VkCommandBuffer cmd);

private:
    struct FrameState
    {
    private:
        // [0..MAX_FRAMES_IN_FLIGHT-1]
        uint32_t            frameIndex;
        VkCommandBuffer     frameCmd;
        VkSemaphore         semaphoreToWait;
        // This cmd buffer is used for materials that 
        // are uploaded out of rgStartFrame - rgDrawFrame when
        // 'frameCmd' doesn't exist
        VkCommandBuffer     preFrameCmd;

    public:
        FrameState() : 
            frameIndex(MAX_FRAMES_IN_FLIGHT - 1), 
            frameCmd(VK_NULL_HANDLE), 
            semaphoreToWait(VK_NULL_HANDLE),
            preFrameCmd(VK_NULL_HANDLE)
        {}
       
        FrameState(const FrameState &other) = delete;
        FrameState(FrameState &&other) noexcept = delete;
        FrameState &operator=(const FrameState &other) = delete;
        FrameState &operator=(FrameState &&other) noexcept = delete;

        uint32_t IncrementFrameIndexAndGet()
        {
            frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
            return frameIndex;
        }

        uint32_t GetFrameIndex() const
        {
            assert(frameIndex >= 0 && frameIndex < MAX_FRAMES_IN_FLIGHT);
            return frameIndex;
        }

        static uint32_t GetPrevFrameIndex(uint32_t frameIndex)
        {
            assert(frameIndex >= 0 && frameIndex < MAX_FRAMES_IN_FLIGHT);
            return (frameIndex + (MAX_FRAMES_IN_FLIGHT - 1)) % MAX_FRAMES_IN_FLIGHT;
        }

        void OnBeginFrame(VkCommandBuffer cmd)
        {
            assert(frameCmd == VK_NULL_HANDLE);
            frameCmd = cmd;
        }

        void OnEndFrame()
        {
            assert(frameCmd != VK_NULL_HANDLE);
            // pre-frame cmd must be submitted by this time
            assert(preFrameCmd == VK_NULL_HANDLE);
            frameCmd = VK_NULL_HANDLE;
        }

        VkCommandBuffer GetCmdBuffer() const
        {
            // only in-frame usage
            assert(WasFrameStarted());
            return frameCmd;
        }

        VkCommandBuffer GetCmdBufferForMaterials(const std::shared_ptr<CommandBufferManager> &cmdManager)
        {  
            if (WasFrameStarted())
            {
                // use default cmd buffer, if frame was started
                return GetCmdBuffer();
            }

            // use custom cmd buffer, if out-of-frame call,
            // because the default one doesn't exist yet
            if (preFrameCmd == VK_NULL_HANDLE)
            {
                preFrameCmd = cmdManager->StartGraphicsCmd();
            }

            return preFrameCmd;
        }

        VkCommandBuffer GetPreFrameCmdAndRemove()
        {
            VkCommandBuffer c = preFrameCmd;

            preFrameCmd = VK_NULL_HANDLE;
            return c;
        }

        bool WasFrameStarted() const
        {
            return frameCmd != nullptr;
        }

        void SetSemaphore(VkSemaphore s)
        {
            semaphoreToWait = s;
        }

        VkSemaphore GetSemaphoreForWaitAndRemove()
        {
            VkSemaphore s = semaphoreToWait;

            semaphoreToWait = VK_NULL_HANDLE;
            return s;
        }
    };

private:
    VkInstance          instance;
    VkDevice            device;
    VkSurfaceKHR        surface;

    FrameState          currentFrameState;

    // incremented every frame
    uint32_t            frameId;

    VkFence             frameFences[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore         imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore         renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore         inFrameSemaphores[MAX_FRAMES_IN_FLIGHT] = {};

    bool                waitForOutOfFrameFence;
    VkFence             outOfFrameFences[MAX_FRAMES_IN_FLIGHT] = {};

    std::shared_ptr<PhysicalDevice>         physDevice;
    std::shared_ptr<Queues>                 queues;
    std::shared_ptr<Swapchain>              swapchain;

    std::shared_ptr<MemoryAllocator>        memAllocator;

    std::shared_ptr<CommandBufferManager>   cmdManager;

    std::shared_ptr<Framebuffers>           framebuffers;

    std::shared_ptr<GlobalUniform>          uniform;
    std::shared_ptr<Scene>                  scene;

    std::shared_ptr<ShaderManager>          shaderManager;
    std::shared_ptr<RayTracingPipeline>     rtPipeline;
    std::shared_ptr<PathTracer>             pathTracer;
    std::shared_ptr<Rasterizer>             rasterizer;
    std::shared_ptr<DecalManager>           decalManager;
    std::shared_ptr<Denoiser>               denoiser;
    std::shared_ptr<Tonemapping>            tonemapping;
    std::shared_ptr<ImageComposition>       imageComposition;
    std::shared_ptr<Bloom>                  bloom;
    std::shared_ptr<SuperResolution>        amdFsr;
    std::shared_ptr<DLSS>                   nvDlss;
    std::shared_ptr<Sharpening>             sharpening;
    std::shared_ptr<EffectWipe>                 effectWipe;
    std::shared_ptr<EffectRadialBlur>           effectRadialBlur;
    std::shared_ptr<EffectChromaticAberration>  effectChromaticAberration;
    std::shared_ptr<EffectInverseBW>            effectInverseBW;
    std::shared_ptr<EffectHueShift>             effectHueShift;
    std::shared_ptr<EffectDistortedSides>       effectDistortedSides;
    std::shared_ptr<EffectColorTint>            effectColorTint;
    std::shared_ptr<EffectCrtDemodulateEncode>  effectCrtDemodulateEncode;
    std::shared_ptr<EffectCrtDecode>            effectCrtDecode;

    std::shared_ptr<SamplerManager>         worldSamplerManager;
    std::shared_ptr<SamplerManager>         genericSamplerManager;
    std::shared_ptr<BlueNoise>              blueNoise;
    std::shared_ptr<TextureManager>         textureManager;
    std::shared_ptr<CubemapManager>         cubemapManager;

    bool                                    enableValidationLayer;
    VkDebugUtilsMessengerEXT                debugMessenger;
    std::unique_ptr<UserPrint>              userPrint;
    std::shared_ptr<UserFileLoad>           userFileLoad;

    VertexBufferProperties                  vbProperties = {};
    bool                                    rayCullBackFacingTriangles;
    bool                                    allowGeometryWithSkyFlag;
    bool                                    lensFlareVerticesInScreenSpace;

    RenderResolutionHelper                  renderResolution;

    double                                  previousFrameTime;
    double                                  currentFrameTime;
};

}
