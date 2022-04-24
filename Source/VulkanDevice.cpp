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

#include "VulkanDevice.h"

#include <algorithm>
#include <stdlib.h>
#include <cstring>
#include <cmath>
#include <stdexcept>

#include "HaltonSequence.h"
#include "Matrix.h"
#include "RenderResolutionHelper.h"
#include "RgException.h"
#include "Utils.h"
#include "Generated/ShaderCommonC.h"

using namespace RTGL1;

VulkanDevice::VulkanDevice(const RgInstanceCreateInfo *info) :
    instance(VK_NULL_HANDLE),
    device(VK_NULL_HANDLE),
    surface(VK_NULL_HANDLE),
    currentFrameState(),
    frameId(1),
    waitForOutOfFrameFence(false),
    enableValidationLayer(info->enableValidationLayer == RG_TRUE),
    debugMessenger(VK_NULL_HANDLE),
    userPrint{ std::make_unique<UserPrint>(info->pfnPrint, info->pUserPrintData) },
    userFileLoad{ std::make_shared<UserFileLoad>(info->pfnOpenFile, info->pfnCloseFile, info->pUserLoadFileData) },
    rayCullBackFacingTriangles(info->rayCullBackFacingTriangles),
    allowGeometryWithSkyFlag(info->allowGeometryWithSkyFlag),
    lensFlareVerticesInScreenSpace(info->lensFlareVerticesInScreenSpace),
    previousFrameTime(-1.0 / 60.0),
    currentFrameTime(0)
{
    ValidateCreateInfo(info);



    vbProperties.vertexArrayOfStructs = info->vertexArrayOfStructs == RG_TRUE;
    vbProperties.positionStride = info->vertexPositionStride;
    vbProperties.normalStride = info->vertexNormalStride;
    vbProperties.texCoordStride = info->vertexTexCoordStride;
    vbProperties.colorStride = info->vertexColorStride;



    // init vulkan instance 
    CreateInstance(*info);


    // create VkSurfaceKHR using user's function
    surface = GetSurfaceFromUser(instance, *info);


    // create selected physical device
    physDevice          = std::make_shared<PhysicalDevice>(instance);
    queues              = std::make_shared<Queues>(physDevice->Get(), surface);

    // create vulkan device and set extension function pointers
    CreateDevice();

    CreateSyncPrimitives();

    // set device
    queues->SetDevice(device);


    memAllocator        = std::make_shared<MemoryAllocator>(instance, device, physDevice);

    cmdManager          = std::make_shared<CommandBufferManager>(device, queues);

    uniform             = std::make_shared<GlobalUniform>(device, memAllocator);

    swapchain           = std::make_shared<Swapchain>(device, surface, physDevice, cmdManager);

    // for world samplers with modifyable lod biad
    worldSamplerManager     = std::make_shared<SamplerManager>(device, 8, info->textureSamplerForceMinificationFilterLinear);
    genericSamplerManager   = std::make_shared<SamplerManager>(device, 0, info->textureSamplerForceMinificationFilterLinear);

    framebuffers        = std::make_shared<Framebuffers>(
        device,
        memAllocator, 
        cmdManager);

    blueNoise           = std::make_shared<BlueNoise>(
        device,
        info->pBlueNoiseFilePath,
        memAllocator,
        cmdManager, 
        userFileLoad);

    textureManager      = std::make_shared<TextureManager>(
        device, 
        memAllocator,
        worldSamplerManager,
        cmdManager,
        userFileLoad,
        *info);

    cubemapManager      = std::make_shared<CubemapManager>(
        device,
        memAllocator,
        genericSamplerManager,
        cmdManager,
        userFileLoad,
        info->pOverridenTexturesFolderPath,
        info->pOverridenAlbedoAlphaTexturePostfix);

    shaderManager       = std::make_shared<ShaderManager>(
        device,
        info->pShaderFolderPath,
        userFileLoad);

    scene               = std::make_shared<Scene>(
        device,
        physDevice,
        memAllocator,
        cmdManager,
        textureManager,
        uniform,
        shaderManager,
        vbProperties);
   
    rasterizer          = std::make_shared<Rasterizer>(
        device,
        physDevice->Get(),
        shaderManager,
        textureManager,
        uniform,
        genericSamplerManager,
        memAllocator,
        framebuffers,
        cmdManager,
        *info);

    decalManager        = std::make_shared<DecalManager>(
        device,
        memAllocator,
        shaderManager,
        uniform,
        framebuffers,
        textureManager);

    rtPipeline          = std::make_shared<RayTracingPipeline>(
        device, 
        physDevice, 
        memAllocator, 
        shaderManager,
        scene,
        uniform, 
        textureManager,
        framebuffers, 
        blueNoise, 
        cubemapManager,
        rasterizer->GetRenderCubemap(),
        *info);

    pathTracer          = std::make_shared<PathTracer>(device, rtPipeline);

    tonemapping         = std::make_shared<Tonemapping>(
        device,
        framebuffers,
        shaderManager,
        uniform,
        memAllocator);

    imageComposition    = std::make_shared<ImageComposition>(
        device, 
        framebuffers, 
        shaderManager, 
        uniform, 
        tonemapping);

    bloom               = std::make_shared<Bloom>(
        device,
        framebuffers,
        shaderManager,
        uniform,
        tonemapping);

    amdFsr              = std::make_shared<SuperResolution>(
        device,
        framebuffers,
        shaderManager);

    nvDlss              = std::make_shared<DLSS>(
        instance, 
        device, 
        physDevice->Get(),
        info->pAppGUID,
        enableValidationLayer);

    sharpening          = std::make_shared<Sharpening>(
        device,
        framebuffers,
        shaderManager);

    denoiser            = std::make_shared<Denoiser>(
        device,
        framebuffers,
        shaderManager,
        uniform,
        scene->GetASManager());

    effectWipe          = std::make_shared<EffectWipe>(
        device,
        framebuffers,
        uniform,
        blueNoise,
        shaderManager);

#define CONSTRUCT_SIMPLE_EFFECT(T) std::make_shared<T>(device, framebuffers, uniform, shaderManager)
    effectRadialBlur            = CONSTRUCT_SIMPLE_EFFECT(EffectRadialBlur);
    effectChromaticAberration   = CONSTRUCT_SIMPLE_EFFECT(EffectChromaticAberration);
    effectInverseBW             = CONSTRUCT_SIMPLE_EFFECT(EffectInverseBW);
    effectHueShift              = CONSTRUCT_SIMPLE_EFFECT(EffectHueShift);
    effectDistortedSides        = CONSTRUCT_SIMPLE_EFFECT(EffectDistortedSides);
    effectColorTint             = CONSTRUCT_SIMPLE_EFFECT(EffectColorTint);
    effectCrtDemodulateEncode   = CONSTRUCT_SIMPLE_EFFECT(EffectCrtDemodulateEncode);
    effectCrtDecode             = CONSTRUCT_SIMPLE_EFFECT(EffectCrtDecode);
    effectInterlacing           = CONSTRUCT_SIMPLE_EFFECT(EffectInterlacing);
#undef SIMPLE_EFFECT_CONSTRUCTOR_PARAMS


    shaderManager->Subscribe(denoiser);
    shaderManager->Subscribe(imageComposition);
    shaderManager->Subscribe(rasterizer);
    shaderManager->Subscribe(decalManager);
    shaderManager->Subscribe(rtPipeline);
    shaderManager->Subscribe(tonemapping);
    shaderManager->Subscribe(scene->GetVertexPreprocessing());
    shaderManager->Subscribe(bloom);
    shaderManager->Subscribe(amdFsr);
    shaderManager->Subscribe(sharpening);
    shaderManager->Subscribe(effectWipe);
    shaderManager->Subscribe(effectRadialBlur);
    shaderManager->Subscribe(effectChromaticAberration);
    shaderManager->Subscribe(effectInverseBW);
    shaderManager->Subscribe(effectHueShift);
    shaderManager->Subscribe(effectDistortedSides);
    shaderManager->Subscribe(effectColorTint);
    shaderManager->Subscribe(effectCrtDemodulateEncode);
    shaderManager->Subscribe(effectCrtDecode);
    shaderManager->Subscribe(effectInterlacing);

    framebuffers->Subscribe(rasterizer);
    framebuffers->Subscribe(decalManager);
}

VulkanDevice::~VulkanDevice()
{
    vkDeviceWaitIdle(device);

    physDevice.reset();
    queues.reset();
    swapchain.reset();
    cmdManager.reset();
    framebuffers.reset();
    tonemapping.reset();
    imageComposition.reset();
    bloom.reset();
    amdFsr.reset();
    nvDlss.reset();
    sharpening.reset();
    effectWipe.reset();
    effectRadialBlur.reset();
    effectChromaticAberration.reset();
    effectInverseBW.reset();
    effectHueShift.reset();
    effectDistortedSides.reset();
    effectColorTint.reset();
    effectCrtDemodulateEncode.reset();
    effectCrtDecode.reset();
    effectInterlacing.reset();
    denoiser.reset();
    uniform.reset();
    scene.reset();
    shaderManager.reset();
    rtPipeline.reset();
    pathTracer.reset();
    rasterizer.reset();
    decalManager.reset();
    worldSamplerManager.reset();
    genericSamplerManager.reset();
    blueNoise.reset();
    textureManager.reset();
    cubemapManager.reset();
    memAllocator.reset();

    vkDestroySurfaceKHR(instance, surface, nullptr);
    DestroySyncPrimitives();

    DestroyDevice();
    DestroyInstance();
}

VkCommandBuffer VulkanDevice::BeginFrame(const RgStartFrameInfo &startInfo)
{
    uint32_t frameIndex = currentFrameState.IncrementFrameIndexAndGet();

    if (!waitForOutOfFrameFence)
    {
        // wait for previous cmd with the same frame index
        Utils::WaitAndResetFence(device, frameFences[frameIndex]);
    }
    else
    {
        Utils::WaitAndResetFences(device, frameFences[frameIndex], outOfFrameFences[frameIndex]);
    }

    swapchain->RequestNewSize(startInfo.surfaceSize.width, startInfo.surfaceSize.height);
    swapchain->RequestVsync(startInfo.requestVSync);
    swapchain->AcquireImage(imageAvailableSemaphores[frameIndex]);

    VkSemaphore semaphoreToWaitOnSubmit = imageAvailableSemaphores[frameIndex];


    // if out-of-frame cmd exist, submit it
    {
        VkCommandBuffer preFrameCmd = currentFrameState.GetPreFrameCmdAndRemove();
        if (preFrameCmd != VK_NULL_HANDLE)
        {
            // Signal inFrameSemaphore after completion.
            // Signal outOfFrameFences, but for the next frame
            // because we can't reset cmd pool with cmds (in this case 
            // it's preFrameCmd) that are in use.
            cmdManager->Submit(preFrameCmd,
                               semaphoreToWaitOnSubmit, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               inFrameSemaphores[frameIndex],
                               outOfFrameFences[(frameIndex + 1) % MAX_FRAMES_IN_FLIGHT]);

            // should wait other semaphore in this case
            semaphoreToWaitOnSubmit = inFrameSemaphores[frameIndex];

            waitForOutOfFrameFence = true;
        }
        else
        {
            waitForOutOfFrameFence = false;
        }
    }
    currentFrameState.SetSemaphore(semaphoreToWaitOnSubmit);


    if (startInfo.requestShaderReload)
    {
        shaderManager->ReloadShaders();
    }


    // reset cmds for current frame index
    cmdManager->PrepareForFrame(frameIndex);

    // clear the data that were created MAX_FRAMES_IN_FLIGHT ago
    worldSamplerManager->PrepareForFrame(frameIndex);
    genericSamplerManager->PrepareForFrame(frameIndex);
    textureManager->PrepareForFrame(frameIndex);
    cubemapManager->PrepareForFrame(frameIndex);
    rasterizer->PrepareForFrame(frameIndex, startInfo.requestRasterizedSkyGeometryReuse);
    decalManager->PrepareForFrame(frameIndex);

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    BeginCmdLabel(cmd, "Prepare for frame");

    // start dynamic geometry recording to current frame
    scene->PrepareForFrame(cmd, frameIndex);

    return cmd;
}

void VulkanDevice::FillUniform(ShGlobalUniform *gu, const RgDrawFrameInfo &drawInfo) const
{
    const float IdentityMat4x4[16] =
    {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    {
        memcpy(gu->viewPrev, gu->view, 16 * sizeof(float));
        memcpy(gu->projectionPrev, gu->projection, 16 * sizeof(float));

        memcpy(gu->view, drawInfo.view, 16 * sizeof(float));
        memcpy(gu->projection, drawInfo.projection, 16 * sizeof(float));

        Matrix::Inverse(gu->invView, drawInfo.view);
        Matrix::Inverse(gu->invProjection, drawInfo.projection);

        gu->cameraPosition[0] = gu->invView[12];
        gu->cameraPosition[1] = gu->invView[13];
        gu->cameraPosition[2] = gu->invView[14];
    }

    {
        static_assert(sizeof(gu->instanceGeomInfoOffset) == sizeof(gu->instanceGeomInfoOffsetPrev), "");
        memcpy(gu->instanceGeomInfoOffsetPrev, gu->instanceGeomInfoOffset, sizeof(gu->instanceGeomInfoOffset));
    }

    { 
        // to remove additional division by 4 bytes in shaders
        gu->positionsStride = vbProperties.positionStride / 4;
        gu->normalsStride = vbProperties.normalStride / 4;
        gu->texCoordsStride = vbProperties.texCoordStride / 4;
    }

    {
        gu->frameId = frameId;
        gu->timeDelta = (float)std::max<double>(currentFrameTime - previousFrameTime, 0.001);
        gu->time = (float)currentFrameTime;
    }

    {
        gu->renderWidth = (float)renderResolution.Width();
        gu->renderHeight = (float)renderResolution.Height();
        // render width must be always even for checkerboarding!
        assert((int)gu->renderWidth % 2 == 0);

        gu->upscaledRenderWidth = (float)renderResolution.UpscaledWidth();
        gu->upscaledRenderHeight = (float)renderResolution.UpscaledHeight();

        if (renderResolution.IsNvDlssEnabled())
        {
            RgFloat2D jitter = HaltonSequence::GetJitter_Halton23(frameId);

            gu->jitterX = jitter.data[0];
            gu->jitterY = jitter.data[1];
        }
        else
        {
            gu->jitterX = 0.0f;
            gu->jitterY = 0.0f;
        }
    }

    {
        gu->stopEyeAdaptation = drawInfo.disableEyeAdaptation;

        if (drawInfo.pTonemappingParams != nullptr)
        {
            gu->minLogLuminance = drawInfo.pTonemappingParams->minLogLuminance;
            gu->maxLogLuminance = drawInfo.pTonemappingParams->maxLogLuminance;
            gu->luminanceWhitePoint = drawInfo.pTonemappingParams->luminanceWhitePoint;
        }
        else
        {
            gu->minLogLuminance = -7.0f;
            gu->maxLogLuminance = 0.0f;
            gu->luminanceWhitePoint = 11.0f;
        }
    }

    {
        gu->lightCountSpherical         = scene->GetLightManager()->GetSphericalLightCount();
        gu->lightCountSphericalPrev     = scene->GetLightManager()->GetSphericalLightCountPrev();

        gu->lightCountDirectional       = scene->GetLightManager()->GetDirectionalLightCount();
        gu->lightCountDirectionalPrev   = scene->GetLightManager()->GetDirectionalLightCountPrev();

        gu->lightCountSpotlight         = scene->GetLightManager()->GetSpotlightCount();
        gu->lightCountSpotlightPrev     = scene->GetLightManager()->GetSpotlightCountPrev();

        gu->lightCountPolygonal         = scene->GetLightManager()->GetPolygonalLightCount();
        gu->lightCountPolygonalPrev     = scene->GetLightManager()->GetPolygonalLightCountPrev();
    }

    {
        static_assert(sizeof(gu->skyCubemapRotationTransform) == sizeof(IdentityMat4x4) && 
                      sizeof(IdentityMat4x4) == 16 * sizeof(float), "Recheck skyCubemapRotationTransform sizes");
        memcpy(gu->skyCubemapRotationTransform, IdentityMat4x4, 16 * sizeof(float));

        if (drawInfo.pSkyParams != nullptr)
        {
            const auto &sp = *drawInfo.pSkyParams;

            memcpy(gu->skyColorDefault, sp.skyColorDefault.data, sizeof(float) * 3);
            gu->skyColorMultiplier = sp.skyColorMultiplier;
            gu->skyColorSaturation = std::max(sp.skyColorSaturation, 0.0f);

            memcpy(gu->skyViewerPosition, sp.skyViewerPosition.data, sizeof(float) * 3);

            gu->skyType =
                sp.skyType == RG_SKY_TYPE_CUBEMAP ? SKY_TYPE_CUBEMAP :
                sp.skyType == RG_SKY_TYPE_RASTERIZED_GEOMETRY ? SKY_TYPE_RASTERIZED_GEOMETRY :
                SKY_TYPE_COLOR;

            gu->skyCubemapIndex = cubemapManager->IsCubemapValid(sp.skyCubemap) ? sp.skyCubemap : RG_EMPTY_CUBEMAP;

            if (!Utils::IsAlmostZero(drawInfo.pSkyParams->skyCubemapRotationTransform))
            {
                Utils::SetMatrix3ToGLSLMat4(gu->skyCubemapRotationTransform, drawInfo.pSkyParams->skyCubemapRotationTransform);
            }
        }
        else
        {
            gu->skyColorDefault[0] = gu->skyColorDefault[1] = gu->skyColorDefault[2] = gu->skyColorDefault[3] = 1.0f;
            gu->skyColorMultiplier = 1.0f;
            gu->skyColorSaturation = 1.0f;
            memset(gu->skyViewerPosition, 0, sizeof(gu->skyViewerPosition));
            gu->skyType = SKY_TYPE_COLOR;
            gu->skyCubemapIndex = RG_EMPTY_CUBEMAP;
        }

        for (uint32_t i = 0; i < 6; i++)
        {
            float *viewProjDst = &gu->viewProjCubemap[16 * i];

            Matrix::GetCubemapViewProjMat(viewProjDst, i, gu->skyViewerPosition);
        }
    }

    gu->debugShowFlags = 0;

    if (drawInfo.pDebugParams != nullptr)
    {
        if (drawInfo.pDebugParams->showGradients)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_GRADIENTS;
        }
        if (drawInfo.pDebugParams->showMotionVectors)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_MOTION_VECTORS;
        }
        if (drawInfo.pDebugParams->showSectors)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_SECTORS;
        }
    }

    if (drawInfo.pTexturesParams != nullptr)
    {
        gu->normalMapStrength = drawInfo.pTexturesParams->normalMapStrength;
        gu->emissionMapBoost = std::max(drawInfo.pTexturesParams->emissionMapBoost, 0.0f);
        gu->emissionMaxScreenColor = std::max(drawInfo.pTexturesParams->emissionMaxScreenColor, 0.0f);
    }
    else
    {
        gu->normalMapStrength = 1.0f;
        gu->emissionMapBoost = 100.0f;
        gu->emissionMaxScreenColor = 1.5f;
    }

    if (drawInfo.pShadowParams != nullptr)
    {
        gu->maxBounceShadowsDirectionalLights   = drawInfo.pShadowParams->maxBounceShadowsDirectionalLights;
        gu->maxBounceShadowsSphereLights        = drawInfo.pShadowParams->maxBounceShadowsSphereLights;
        gu->maxBounceShadowsSpotlights          = drawInfo.pShadowParams->maxBounceShadowsSpotlights;
        gu->maxBounceShadowsPolygonalLights     = drawInfo.pShadowParams->maxBounceShadowsPolygonalLights;
        gu->polyLightSpotlightFactor            = std::max(0.0f, drawInfo.pShadowParams->polygonalLightSpotlightFactor);
        gu->firefliesClamp                      = std::max(0.0f, drawInfo.pShadowParams->sphericalPolygonalLightsFirefliesClamp);
    }
    else
    {
        gu->maxBounceShadowsDirectionalLights = 8;
        gu->maxBounceShadowsSphereLights = 2;
        gu->maxBounceShadowsSpotlights = 2;
        gu->maxBounceShadowsPolygonalLights = 2;
        gu->polyLightSpotlightFactor = 2.0f;
        gu->firefliesClamp = 3.0f;
    }

    if (drawInfo.pBloomParams != nullptr)
    {
        gu->bloomThreshold          = std::max(drawInfo.pBloomParams->inputThreshold, 0.0f);
        gu->bloomThresholdLength    = std::max(drawInfo.pBloomParams->inputThresholdLength, 0.0f);
        gu->bloomUpsampleRadius     = std::max(drawInfo.pBloomParams->upsampleRadius, 0.0f);
        gu->bloomIntensity          = std::max(drawInfo.pBloomParams->bloomIntensity, 0.0f);
        gu->bloomEmissionMultiplier = std::max(drawInfo.pBloomParams->bloomEmissionMultiplier, 0.0f);
        gu->bloomSkyMultiplier      = std::max(drawInfo.pBloomParams->bloomSkyMultiplier, 0.0f);
        gu->bloomEmissionSaturationBias = clamp(drawInfo.pBloomParams->bloomEmissionSaturationBias, -1.0f, 20.0f);
    }
    else
    {
        gu->bloomThreshold = 15.0f;
        gu->bloomThresholdLength = 0.25f;
        gu->bloomUpsampleRadius = 1.0f;
        gu->bloomIntensity = 1.0f;
        gu->bloomEmissionMultiplier = 64.0f;
        gu->bloomSkyMultiplier = 0.05f;
        gu->bloomEmissionSaturationBias = 0.0f;
    }

    static_assert(
        RG_MEDIA_TYPE_VACUUM == MEDIA_TYPE_VACUUM &&
        RG_MEDIA_TYPE_WATER == MEDIA_TYPE_WATER &&
        RG_MEDIA_TYPE_GLASS == MEDIA_TYPE_GLASS, 
        "Interface and GLSL constants must be identical");

    static_assert(
        sizeof(gu->portalInputToOutputTransform0) == 4 * sizeof(float) &&
        sizeof(gu->portalInputToOutputTransform1) == 4 * sizeof(float) &&
        sizeof(gu->portalInputToOutputTransform2) == 4 * sizeof(float),
        "Recheck uniform member and interface member sizes");

    if (drawInfo.pReflectRefractParams != nullptr)
    {
        const auto &rr = *drawInfo.pReflectRefractParams;

        if (rr.typeOfMediaAroundCamera >= 0 &&
            rr.typeOfMediaAroundCamera < MEDIA_TYPE_COUNT)
        {
            gu->cameraMediaType = rr.typeOfMediaAroundCamera;
        }
        else
        {
            gu->cameraMediaType = MEDIA_TYPE_VACUUM;
        }

        gu->reflectRefractMaxDepth = std::min(4u, rr.maxReflectRefractDepth);

        memcpy(gu->portalInputToOutputTransform0, rr.portalRelativeRotation.matrix[0], 3 * sizeof(float));
        memcpy(gu->portalInputToOutputTransform1, rr.portalRelativeRotation.matrix[1], 3 * sizeof(float));
        memcpy(gu->portalInputToOutputTransform2, rr.portalRelativeRotation.matrix[2], 3 * sizeof(float));
        gu->portalInputToOutputTransform0[3] = rr.portalOutputPosition.data[0] - rr.portalInputPosition.data[0];
        gu->portalInputToOutputTransform1[3] = rr.portalOutputPosition.data[1] - rr.portalInputPosition.data[1];
        gu->portalInputToOutputTransform2[3] = rr.portalOutputPosition.data[2] - rr.portalInputPosition.data[2];
        memcpy(gu->portalInputPosition, rr.portalInputPosition.data, 3 * sizeof(float));

        gu->enableShadowsFromReflRefr = !!rr.reflectRefractCastShadows;
        gu->enableIndirectFromReflRefr = !!rr.reflectRefractToIndirect;
    
        gu->indexOfRefractionGlass = std::max(0.0f, rr.indexOfRefractionGlass);
        gu->indexOfRefractionWater = std::max(0.0f, rr.indexOfRefractionWater);

        memcpy(gu->waterExtinction, rr.waterExtinction.data, 3 * sizeof(float));

        gu->forceNoWaterRefraction = !!rr.forceNoWaterRefraction;
        gu->waterWaveSpeed = rr.waterWaveSpeed;
        gu->waterWaveStrength = rr.waterWaveNormalStrength;
        gu->waterTextureDerivativesMultiplier = std::max(0.0f, rr.waterWaveTextureDerivativesMultiplier);
        if (rr.waterTextureAreaScale < 0.0001f)
        {
            gu->waterTextureAreaScale = 1.0f;
        }
        else
        {
            gu->waterTextureAreaScale = rr.waterTextureAreaScale;
        }
    
        gu->noBackfaceReflForNoMediaChange = !!rr.disableBackfaceReflectionsForNoMediaChange;
    }
    else
    {
        gu->cameraMediaType = MEDIA_TYPE_VACUUM;
        gu->reflectRefractMaxDepth = 2;
        memset(gu->portalInputToOutputTransform0, 0, sizeof(gu->portalInputToOutputTransform0));
        memset(gu->portalInputToOutputTransform1, 0, sizeof(gu->portalInputToOutputTransform1));
        memset(gu->portalInputToOutputTransform2, 0, sizeof(gu->portalInputToOutputTransform2));
        gu->portalInputToOutputTransform0[0] = 1.0f;
        gu->portalInputToOutputTransform1[1] = 1.0f;
        gu->portalInputToOutputTransform2[2] = 1.0f;
        gu->portalInputPosition[0] = 0.0f;
        gu->portalInputPosition[1] = 0.0f;
        gu->portalInputPosition[2] = 0.0f;
    
        gu->enableShadowsFromReflRefr = false;
        gu->enableIndirectFromReflRefr = true;

        gu->indexOfRefractionGlass = 1.52f;
        gu->indexOfRefractionWater = 1.33f;

        gu->waterExtinction[0] = 0.030f; 
        gu->waterExtinction[1] = 0.019f;
        gu->waterExtinction[2] = 0.013f;

        gu->forceNoWaterRefraction = false;
        gu->waterWaveSpeed = 1.0f;
        gu->waterWaveStrength = 1.0f;
        gu->waterTextureDerivativesMultiplier = 1.0f;
        gu->waterTextureAreaScale = 1.0f;

        gu->noBackfaceReflForNoMediaChange = false;
    }

    gu->rayCullBackFaces = rayCullBackFacingTriangles ? 1 : 0;
    gu->rayLength = clamp(drawInfo.rayLength, 0.1f, (float)MAX_RAY_LENGTH);
    gu->primaryRayMinDist = clamp(drawInfo.primaryRayMinDist, 0.001f, gu->rayLength);

    {
        gu->rayCullMaskWorld = 0;

        if (drawInfo.rayCullMaskWorld & RG_DRAW_FRAME_RAY_CULL_WORLD_0_BIT)
        {
            gu->rayCullMaskWorld |= INSTANCE_MASK_WORLD_0;
        }

        if (drawInfo.rayCullMaskWorld & RG_DRAW_FRAME_RAY_CULL_WORLD_1_BIT)
        {
            gu->rayCullMaskWorld |= INSTANCE_MASK_WORLD_1;
        }

        if (drawInfo.rayCullMaskWorld & RG_DRAW_FRAME_RAY_CULL_WORLD_2_BIT)
        {
            if (allowGeometryWithSkyFlag)
            {
                throw RgException(RG_WRONG_ARGUMENT, "RG_DRAW_FRAME_RAY_CULL_WORLD_2_BIT cannot be used, as RgInstanceCreateInfo::allowGeometryWithSkyFlag was true");
            }

            gu->rayCullMaskWorld |= INSTANCE_MASK_WORLD_2;
        }

    #if RAYCULLMASK_SKY_IS_WORLD2
        if (drawInfo.rayCullMaskWorld & RG_DRAW_FRAME_RAY_CULL_SKY_BIT)
        {
            if (!allowGeometryWithSkyFlag)
            {
                throw RgException(RG_WRONG_ARGUMENT, "RG_DRAW_FRAME_RAY_CULL_SKY_BIT cannot be used, as RgInstanceCreateInfo::allowGeometryWithSkyFlag was false");
            }

            gu->rayCullMaskWorld |= INSTANCE_MASK_WORLD_2;
        }
    #else
        #error Handle RG_DRAW_FRAME_RAY_CULL_SKY_BIT, if there is no WORLD_2
    #endif


        if (allowGeometryWithSkyFlag)
        {
            gu->rayCullMaskWorld_Shadow = gu->rayCullMaskWorld & (~INSTANCE_MASK_WORLD_2);
        }
        else
        {
            gu->rayCullMaskWorld_Shadow = gu->rayCullMaskWorld;
        }
    }

    gu->waterNormalTextureIndex = textureManager->GetWaterNormalTextureIndex();

    gu->cameraRayConeSpreadAngle = atanf((2.0f * tanf(drawInfo.fovYRadians * 0.5f)) / (float)renderResolution.Height());

    if (Utils::IsAlmostZero(drawInfo.worldUpVector))
    {
        gu->worldUpVector[0] = 0.0f;
        gu->worldUpVector[1] = 1.0f;
        gu->worldUpVector[2] = 0.0f;
    }
    else
    {
        gu->worldUpVector[0] = drawInfo.worldUpVector.data[0];
        gu->worldUpVector[1] = drawInfo.worldUpVector.data[1];
        gu->worldUpVector[2] = drawInfo.worldUpVector.data[2];
    }

    gu->useSqrtRoughnessForIndirect = !!drawInfo.useSqrtRoughnessForIndirect;

    gu->lensFlareCullingInputCount = rasterizer->GetLensFlareCullingInputCount();
    gu->applyViewProjToLensFlares = !lensFlareVerticesInScreenSpace;
}

void VulkanDevice::Render(VkCommandBuffer cmd, const RgDrawFrameInfo &drawInfo)
{
    // end of "Prepare for frame" label
    EndCmdLabel(cmd);


    const uint32_t frameIndex = currentFrameState.GetFrameIndex();

    
    bool mipLodBiasUpdated = worldSamplerManager->TryChangeMipLodBias(frameIndex, renderResolution.GetMipLodBias());
    const RgFloat2D jitter = renderResolution.IsNvDlssEnabled() ? HaltonSequence::GetJitter_Halton23(frameId) : RgFloat2D{ 0, 0 };

    textureManager->SubmitDescriptors(frameIndex, drawInfo.pTexturesParams, mipLodBiasUpdated);
    cubemapManager->SubmitDescriptors(frameIndex);


    // submit geometry and upload uniform after getting data from a scene
    const bool raysCanBeTraced = scene->SubmitForFrame(cmd, frameIndex, uniform, 
                                                       uniform->GetData()->rayCullMaskWorld, 
                                                       allowGeometryWithSkyFlag, 
                                                       drawInfo.pReflectRefractParams ? drawInfo.pReflectRefractParams->isReflRefrAlphaTested : false,
                                                       drawInfo.disableRayTracing);


    framebuffers->PrepareForSize(renderResolution.GetResolutionState());
    

    if (!drawInfo.disableRasterization)
    {
        rasterizer->SubmitForFrame(cmd, frameIndex);

        // draw rasterized sky to albedo before tracing primary rays
        if (uniform->GetData()->skyType == RG_SKY_TYPE_RASTERIZED_GEOMETRY)
        {
            rasterizer->DrawSkyToCubemap(cmd, frameIndex, textureManager, uniform);
            rasterizer->DrawSkyToAlbedo(cmd, frameIndex, textureManager, uniform->GetData()->view, uniform->GetData()->skyViewerPosition, uniform->GetData()->projection, jitter, renderResolution);
        }
    }


    assert(!!(uniform->GetData()->areFramebufsInitedByRT) == raysCanBeTraced);


    if (raysCanBeTraced)
    {
        decalManager->SubmitForFrame(cmd, frameIndex);

        pathTracer->Bind(
            cmd, frameIndex, 
            scene, uniform, textureManager, 
            framebuffers, blueNoise, cubemapManager, rasterizer->GetRenderCubemap());

        pathTracer->TracePrimaryRays(cmd, frameIndex, renderResolution.Width(), renderResolution.Height(), framebuffers);

        // draw decals on top of primary surface
        decalManager->Draw(cmd, frameIndex, uniform, framebuffers, textureManager);

        if (uniform->GetData()->reflectRefractMaxDepth > 0)
        {
            pathTracer->TraceReflectionRefractionRays(cmd, frameIndex, renderResolution.Width(), renderResolution.Height(), framebuffers);
        }

        // save and merge samples from previous illumination results
        denoiser->MergeSamples(cmd, frameIndex, uniform, scene->GetASManager());

        // update the illumination
        pathTracer->TraceDirectllumination(  cmd, frameIndex, renderResolution.Width(), renderResolution.Height(), framebuffers);
        pathTracer->TraceIndirectllumination(cmd, frameIndex, renderResolution.Width(), renderResolution.Height(), framebuffers);

        denoiser->Denoise(cmd, frameIndex, uniform);

        // tonemapping
        tonemapping->Tonemap(cmd, frameIndex, uniform);
    }


    bool enableBloom = drawInfo.pBloomParams == nullptr || (drawInfo.pBloomParams != nullptr && drawInfo.pBloomParams->bloomIntensity > 0.0f);

    if (enableBloom)
    {
        bloom->Prepare(cmd, frameIndex, uniform, tonemapping);
    }


    // final image composition
    imageComposition->Compose(cmd, frameIndex, uniform, tonemapping);

    if (!drawInfo.disableRasterization)
    {
        // draw rasterized geometry into the final image
        rasterizer->DrawToFinalImage(cmd, frameIndex, textureManager,
                                     uniform->GetData()->view, uniform->GetData()->projection,
                                     raysCanBeTraced, drawInfo.pLensFlareParams);
    }


    FramebufferImageIndex currentResultImage = FramebufferImageIndex::FB_IMAGE_INDEX_FINAL;
    {
        VkExtent2D extent = { renderResolution.Width(), renderResolution.Height() };


        // upscale finalized image
        if (renderResolution.IsNvDlssEnabled())
        {
            currentResultImage = nvDlss->Apply(cmd, frameIndex, framebuffers, renderResolution, jitter);
            extent = { renderResolution.UpscaledWidth(), renderResolution.UpscaledHeight() };
        }
        else if (renderResolution.IsAmdFsrEnabled())
        {
            currentResultImage = amdFsr->Apply(cmd, frameIndex, framebuffers, renderResolution);
            extent = { renderResolution.UpscaledWidth(), renderResolution.UpscaledHeight() };
        }

        currentResultImage = framebuffers->BlitForEffects(cmd, frameIndex, currentResultImage, renderResolution.GetBlitFilter());


        // save for the next frame, if needed
        if (drawInfo.pRenderResolutionParams != nullptr && drawInfo.pRenderResolutionParams->interlacing)
        {
            framebuffers->CopyToHistoryBuffer(cmd, frameIndex, currentResultImage);
        }
    }


    const CommonnlyUsedEffectArguments args = { cmd, frameIndex, framebuffers, uniform, renderResolution.UpscaledWidth(), renderResolution.UpscaledHeight(), (float)currentFrameTime };
    {
        if (renderResolution.IsSharpeningEnabled())
        {
            currentResultImage = sharpening->Apply(
                cmd, frameIndex, framebuffers, renderResolution.UpscaledWidth(), renderResolution.UpscaledHeight(), currentResultImage,
                renderResolution.GetSharpeningTechnique(), renderResolution.GetSharpeningIntensity());
        }
        if (drawInfo.pRenderResolutionParams != nullptr && drawInfo.pRenderResolutionParams->interlacing)
        {
            if (effectInterlacing->Setup(args))
            {
                currentResultImage = effectInterlacing->Apply(args, currentResultImage);
            }
        }
        if (enableBloom)
        {
            currentResultImage = bloom->Apply(cmd, frameIndex, uniform, renderResolution.UpscaledWidth(), renderResolution.UpscaledHeight(), currentResultImage);
        }
        if (effectColorTint->Setup(args, drawInfo.postEffectParams.pColorTint))
        {
            currentResultImage = effectColorTint->Apply(args, currentResultImage);
        }
        if (effectInverseBW->Setup(args, drawInfo.postEffectParams.pInverseBlackAndWhite))
        {
            currentResultImage = effectInverseBW->Apply(args, currentResultImage);
        }
        if (effectHueShift->Setup(args, drawInfo.postEffectParams.pHueShift))
        {
            currentResultImage = effectHueShift->Apply(args, currentResultImage);
        }
        if (effectChromaticAberration->Setup(args, drawInfo.postEffectParams.pChromaticAberration))
        {
            currentResultImage = effectChromaticAberration->Apply(args, currentResultImage);
        }
        if (effectDistortedSides->Setup(args, drawInfo.postEffectParams.pDistortedSides))
        {
            currentResultImage = effectDistortedSides->Apply(args, currentResultImage);
        }
        if (effectRadialBlur->Setup(args, drawInfo.postEffectParams.pRadialBlur))
        {
            currentResultImage = effectRadialBlur->Apply(args, currentResultImage);
        }
    }

    // draw geometry such as HUD directly into the swapchain image
    if (!drawInfo.disableRasterization)
    {
        rasterizer->DrawToSwapchain(cmd, frameIndex, currentResultImage, textureManager,
                                    uniform->GetData()->view, uniform->GetData()->projection);
    }

    // post-effect that work on swapchain geometry too
    {
        if (effectWipe->Setup(args, drawInfo.postEffectParams.pWipe, swapchain, frameId))
        {
            currentResultImage = effectWipe->Apply(args, blueNoise, currentResultImage);
        }
        if (drawInfo.postEffectParams.pCRT != nullptr && drawInfo.postEffectParams.pCRT->isActive)
        {
            effectCrtDemodulateEncode->Setup(args);
            currentResultImage = effectCrtDemodulateEncode->Apply(args, currentResultImage);

            effectCrtDecode->Setup(args);
            currentResultImage = effectCrtDecode->Apply(args, currentResultImage);
        }
    }

    // blit result image to present on a surface
    framebuffers->PresentToSwapchain(
        cmd, frameIndex, swapchain,
        currentResultImage, VK_FILTER_NEAREST);
}

void VulkanDevice::EndFrame(VkCommandBuffer cmd)
{
    uint32_t frameIndex = currentFrameState.GetFrameIndex();
    VkSemaphore semaphoreToWait = currentFrameState.GetSemaphoreForWaitAndRemove();

    // submit command buffer, but wait until presentation engine has completed using image
    cmdManager->Submit(
        cmd, 
        semaphoreToWait,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
        renderFinishedSemaphores[frameIndex],
        frameFences[frameIndex]);

    // present on a surface when rendering will be finished
    swapchain->Present(queues, renderFinishedSemaphores[frameIndex]);

    frameId++;
}



#pragma region RTGL1 interface implementation

void VulkanDevice::StartFrame(const RgStartFrameInfo *startInfo)
{
    if (currentFrameState.WasFrameStarted())
    {
        throw RgException(RG_FRAME_WASNT_ENDED);
    }

    if (startInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (startInfo->surfaceSize.width == 0 || startInfo->surfaceSize.height == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "surfaceSize dimensions must be >0");
    }

    VkCommandBuffer newFrameCmd = BeginFrame(*startInfo);
    currentFrameState.OnBeginFrame(newFrameCmd);
}

void VulkanDevice::DrawFrame(const RgDrawFrameInfo *drawInfo)
{
    if (!currentFrameState.WasFrameStarted())
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    if (drawInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    VkCommandBuffer cmd = currentFrameState.GetCmdBuffer();

    previousFrameTime = currentFrameTime;
    currentFrameTime = drawInfo->currentTime;

    renderResolution.Setup(drawInfo->pRenderResolutionParams,
                           swapchain->GetWidth(), swapchain->GetHeight(), nvDlss);

    if (renderResolution.Width() > 0 && renderResolution.Height() > 0)
    {
        FillUniform(uniform->GetData(), *drawInfo);
        Render(cmd, *drawInfo);
    }

    EndFrame(cmd);
    currentFrameState.OnEndFrame();
}

bool RTGL1::VulkanDevice::IsRenderUpscaleTechniqueAvailable(RgRenderUpscaleTechnique technique) const
{
    switch (technique)
    {
        case RG_RENDER_UPSCALE_TECHNIQUE_NEAREST:
        case RG_RENDER_UPSCALE_TECHNIQUE_LINEAR:
        case RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR:
            return true;
        case RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS:
            return nvDlss->IsDlssAvailable();
        default:
            throw RgException(RG_WRONG_ARGUMENT, "Incorrect technique was passed to rgIsRenderUpscaleTechniqueAvailable");
    }
}

void VulkanDevice::Print(const char *pMessage) const
{
    userPrint->Print(pMessage);
}


void VulkanDevice::UploadGeometry(const RgGeometryUploadInfo *uploadInfo)
{
    using namespace std::string_literals;

    if (uploadInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (uploadInfo->pVertexData == nullptr || uploadInfo->vertexCount == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Incorrect vertex data");
    }

    if ((uploadInfo->pIndexData == nullptr && uploadInfo->indexCount != 0) ||
        (uploadInfo->pIndexData != nullptr && uploadInfo->indexCount == 0))
    {
        throw RgException(RG_WRONG_ARGUMENT, "Incorrect index data");
    }

    if (uploadInfo->geomType != RG_GEOMETRY_TYPE_STATIC &&
        uploadInfo->geomType != RG_GEOMETRY_TYPE_STATIC_MOVABLE &&
        uploadInfo->geomType != RG_GEOMETRY_TYPE_DYNAMIC &&

        uploadInfo->passThroughType != RG_GEOMETRY_PASS_THROUGH_TYPE_OPAQUE &&
        uploadInfo->passThroughType != RG_GEOMETRY_PASS_THROUGH_TYPE_ALPHA_TESTED &&
        uploadInfo->passThroughType != RG_GEOMETRY_PASS_THROUGH_TYPE_MIRROR &&
        uploadInfo->passThroughType != RG_GEOMETRY_PASS_THROUGH_TYPE_PORTAL &&
        uploadInfo->passThroughType != RG_GEOMETRY_PASS_THROUGH_TYPE_WATER_ONLY_REFLECT &&
        uploadInfo->passThroughType != RG_GEOMETRY_PASS_THROUGH_TYPE_WATER_REFLECT_REFRACT &&
        uploadInfo->passThroughType != RG_GEOMETRY_PASS_THROUGH_TYPE_GLASS_REFLECT_REFRACT &&

        uploadInfo->visibilityType != RG_GEOMETRY_VISIBILITY_TYPE_WORLD_0 &&
        uploadInfo->visibilityType != RG_GEOMETRY_VISIBILITY_TYPE_WORLD_1 &&
        uploadInfo->visibilityType != RG_GEOMETRY_VISIBILITY_TYPE_WORLD_2 &&
        uploadInfo->visibilityType != RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON &&
        uploadInfo->visibilityType != RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON_VIEWER && 
        uploadInfo->visibilityType != RG_GEOMETRY_VISIBILITY_TYPE_SKY)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Incorrect type of ray traced geometry");
    }

    if (allowGeometryWithSkyFlag)
    {
        if (uploadInfo->visibilityType == RG_GEOMETRY_VISIBILITY_TYPE_WORLD_2)
        {
            throw RgException(RG_WRONG_ARGUMENT, "Geometry with RG_GEOMETRY_VISIBILITY_TYPE_WORLD_2 cannot be used, as RgInstanceCreateInfo::allowGeometryWithSkyFlag was true");
        }
    }
    else
    {
        if (uploadInfo->visibilityType == RG_GEOMETRY_VISIBILITY_TYPE_SKY)
        {
            throw RgException(RG_WRONG_ARGUMENT, "Geometry with RG_GEOMETRY_VISIBILITY_TYPE_SKY cannot be used, as RgInstanceCreateInfo::allowGeometryWithSkyFlag was false");
        }
    }

    if ((uploadInfo->flags & RG_GEOMETRY_UPLOAD_REFL_REFR_ALBEDO_MULTIPLY_BIT) != 0 &&
        (uploadInfo->flags & RG_GEOMETRY_UPLOAD_REFL_REFR_ALBEDO_ADD_BIT) != 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "RG_GEOMETRY_UPLOAD_REFL_REFR_ALBEDO_MULTIPLY_BIT and RG_GEOMETRY_UPLOAD_REFL_REFR_ALBEDO_ADD_BIT must be set separately");
    }

    if (scene->DoesUniqueIDExist(uploadInfo->uniqueID))
    {
        throw RgException(RG_WRONG_ARGUMENT, "Geometry with ID="s + std::to_string(uploadInfo->uniqueID) + " already exists");
    }

    scene->Upload(currentFrameState.GetFrameIndex(), *uploadInfo);
}

void VulkanDevice::UpdateGeometryTransform(const RgUpdateTransformInfo *updateInfo)
{
    if (updateInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UpdateTransform(*updateInfo);
}

void RTGL1::VulkanDevice::UpdateGeometryTexCoords(const RgUpdateTexCoordsInfo *updateInfo)
{
    if (updateInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UpdateTexCoords(*updateInfo);
}

void VulkanDevice::UploadRasterizedGeometry(const RgRasterizedGeometryUploadInfo *pUploadInfo,
                                                const float *pViewProjection, const RgViewport *pViewport)
{
    if (pUploadInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (pUploadInfo->renderType != RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT &&
        pUploadInfo->renderType != RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN &&
        pUploadInfo->renderType != RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Incorrect render type of rasterized geometry");
    }

    if ((pUploadInfo->pArrays != nullptr && pUploadInfo->pStructs != nullptr) || 
        (pUploadInfo->pArrays == nullptr && pUploadInfo->pStructs == nullptr))
    {
        throw RgException(RG_WRONG_ARGUMENT, "Exactly one of pArrays and pStructs must be not null");
    }

    if ((pUploadInfo->pIndexData == nullptr && pUploadInfo->indexCount != 0) ||
        (pUploadInfo->pIndexData != nullptr && pUploadInfo->indexCount == 0))
    {
        throw RgException(RG_WRONG_ARGUMENT, "Incorrect index data");
    }

    rasterizer->Upload(currentFrameState.GetFrameIndex(), *pUploadInfo, pViewProjection, pViewport);
}

void RTGL1::VulkanDevice::UploadLensFlare(const RgLensFlareUploadInfo *pUploadInfo)
{
    if (pUploadInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (pUploadInfo->indexCount == 0 || pUploadInfo->pIndexData == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Lens flare index data and count must not be null");
    }

    if (pUploadInfo->vertexCount == 0 || pUploadInfo->pVertexData == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Lens flare vertex data and count must not be null");
    }

    rasterizer->UploadLensFlare(currentFrameState.GetFrameIndex(), *pUploadInfo);
}

void RTGL1::VulkanDevice::UploadDecal(const RgDecalUploadInfo *pUploadInfo)
{
    if (pUploadInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    decalManager->Upload(currentFrameState.GetFrameIndex(), *pUploadInfo, textureManager);
}

void VulkanDevice::SubmitStaticGeometries()
{
    scene->SubmitStatic();
}

void VulkanDevice::StartNewStaticScene()
{
    scene->StartNewStatic();
}

void VulkanDevice::UploadLight(const RgDirectionalLightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameState.GetFrameIndex(), uniform, *pLightInfo);
}

void VulkanDevice::UploadLight(const RgSphericalLightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameState.GetFrameIndex(), *pLightInfo);
}

void VulkanDevice::UploadLight(const RgSpotlightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameState.GetFrameIndex(), uniform, *pLightInfo);
}

void RTGL1::VulkanDevice::UploadLight(const RgPolygonalLightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameState.GetFrameIndex(), *pLightInfo);
}

void RTGL1::VulkanDevice::SetPotentialVisibility(SectorID sectorID_A, SectorID sectorID_B)
{
    scene->SetPotentialVisibility(sectorID_A, sectorID_B);
}

void VulkanDevice::CreateStaticMaterial(const RgStaticMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (createInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    *result = textureManager->CreateStaticMaterial(currentFrameState.GetCmdBufferForMaterials(cmdManager), 
                                                   currentFrameState.GetFrameIndex(), 
                                                   *createInfo);
}

void VulkanDevice::CreateAnimatedMaterial(const RgAnimatedMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (createInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (createInfo->frameCount == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Animated materials must have non-zero amount of frames");
    }

    *result = textureManager->CreateAnimatedMaterial(currentFrameState.GetCmdBufferForMaterials(cmdManager), 
                                                     currentFrameState.GetFrameIndex(), 
                                                     *createInfo);
}

void VulkanDevice::ChangeAnimatedMaterialFrame(RgMaterial animatedMaterial, uint32_t frameIndex)
{
    if (!currentFrameState.WasFrameStarted())
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    bool wasChanged = textureManager->ChangeAnimatedMaterialFrame(animatedMaterial, frameIndex);
}

void VulkanDevice::CreateDynamicMaterial(const RgDynamicMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (createInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (createInfo->size.width == 0 || createInfo->size.height == 0)
    {
        throw RgException(RG_WRONG_MATERIAL_PARAMETER, "Dynamic materials must have non-zero width and height, but given size is (" +
                          std::to_string(createInfo->size.width) + ", " + std::to_string(createInfo->size.height) + ")");
    }

    *result = textureManager->CreateDynamicMaterial(currentFrameState.GetCmdBufferForMaterials(cmdManager),
                                                    currentFrameState.GetFrameIndex(), 
                                                    *createInfo);
}

void VulkanDevice::UpdateDynamicMaterial(const RgDynamicMaterialUpdateInfo *updateInfo)
{
    if (!currentFrameState.WasFrameStarted())
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    if (updateInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    bool wasUpdated = textureManager->UpdateDynamicMaterial(currentFrameState.GetCmdBuffer(), *updateInfo);
}

void VulkanDevice::DestroyMaterial(RgMaterial material)
{
    textureManager->DestroyMaterial(currentFrameState.GetFrameIndex(), material);
}
void VulkanDevice::CreateSkyboxCubemap(const RgCubemapCreateInfo *createInfo, RgCubemap *result)
{
    *result = cubemapManager->CreateCubemap(currentFrameState.GetCmdBufferForMaterials(cmdManager), currentFrameState.GetFrameIndex(), *createInfo);
}
void VulkanDevice::DestroyCubemap(RgCubemap cubemap)
{
    cubemapManager->DestroyCubemap(currentFrameState.GetFrameIndex(), cubemap);
}
#pragma endregion 



#pragma region init / destroy

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    if (pUserData == nullptr)
    {
        return VK_FALSE;
    }


    // DLSS: ignore error 'VUID-VkCuLaunchInfoNVX-paramCount-arraylength' - 'paramCount must be greater than 0'
    if (pCallbackData->messageIdNumber == 2044605652)
    {
        return VK_FALSE;
    }


    const char *msg;

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        msg = "Vulkan::VERBOSE::[%d][%s]\n%s\n\n";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        msg = "Vulkan::INFO::[%d][%s]\n%s\n\n";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        msg = "Vulkan::WARNING::[%d][%s]\n%s\n\n";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        msg = "Vulkan::ERROR::[%d][%s]\n%s\n\n";
    }
    else
    {
        msg = "Vulkan::[%d][%s]\n%s\n\n";
    }

    char buf[1024];
    snprintf(buf, sizeof(buf) / sizeof(buf[0]), msg, pCallbackData->messageIdNumber, pCallbackData->pMessageIdName, pCallbackData->pMessage);

    auto *userPrint = static_cast<UserPrint*>(pUserData);
    userPrint->Print(buf);

    return VK_FALSE;
}

void VulkanDevice::CreateInstance(const RgInstanceCreateInfo &info)
{
    std::vector<const char *> layerNames;

    if (enableValidationLayer)
    {
        layerNames.push_back("VK_LAYER_KHRONOS_validation");
        layerNames.push_back("VK_LAYER_LUNARG_monitor");
    }

    std::vector<VkExtensionProperties> supportedInstanceExtensions;
    uint32_t supportedExtensionsCount;

    if (vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionsCount, nullptr) == VK_SUCCESS)
    {
        supportedInstanceExtensions.resize(supportedExtensionsCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionsCount, supportedInstanceExtensions.data());
    }

    std::vector<const char *> extensions =
    {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,

    #ifdef RG_USE_SURFACE_WIN32
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_WIN32

    #ifdef RG_USE_SURFACE_METAL
        VK_EXT_METAL_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_METAL

    #ifdef RG_USE_SURFACE_WAYLAND
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_WAYLAND

    #ifdef RG_USE_SURFACE_XCB
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_XCB

    #ifdef RG_USE_SURFACE_XLIB
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    #endif // RG_USE_SURFACE_XLIB
    };

    if (enableValidationLayer)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    for (const char *n : DLSS::GetDlssVulkanInstanceExtensions())
    {
        const bool isSupported = std::any_of(supportedInstanceExtensions.cbegin(), supportedInstanceExtensions.cend(),
            [&](const VkExtensionProperties& ext)
            {
                return !std::strcmp(ext.extensionName, n);
            }
        );

        if (!isSupported)
        {
            continue;
        }

        extensions.push_back(n);
    }


    VkApplicationInfo appInfo = {};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = info.pAppName;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledLayerNames = layerNames.data();
    instanceInfo.enabledLayerCount = layerNames.size();
    instanceInfo.ppEnabledExtensionNames = extensions.data();
    instanceInfo.enabledExtensionCount = extensions.size();

    VkResult r = vkCreateInstance(&instanceInfo, nullptr, &instance);
    VK_CHECKERROR(r);


    if (enableValidationLayer)
    {
        InitInstanceExtensionFunctions_DebugUtils(instance);

        if (userPrint)
        {
            // init debug utilsdebugMessenger
            VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {};
            debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            debugMessengerInfo.pfnUserCallback = DebugMessengerCallback;
            debugMessengerInfo.pUserData = static_cast<void *>(userPrint.get());

            r = svkCreateDebugUtilsMessengerEXT(instance, &debugMessengerInfo, nullptr, &debugMessenger);
            VK_CHECKERROR(r);
        }
    }
}

void VulkanDevice::CreateDevice()
{
    VkPhysicalDeviceFeatures features = {};
    features.robustBufferAccess = 1;
    features.fullDrawIndexUint32 = 1;
    features.imageCubeArray = 1;
    features.independentBlend = 1;
    features.geometryShader = 0;
    features.tessellationShader = 0;
    features.sampleRateShading = 0;
    features.dualSrcBlend = 0;
    features.logicOp = 1;
    features.multiDrawIndirect = 1;
    features.drawIndirectFirstInstance = 1;
    features.depthClamp = 1;
    features.depthBiasClamp = 1;
    features.fillModeNonSolid = 0;
    features.depthBounds = 1;
    features.wideLines = 0;
    features.largePoints = 0;
    features.alphaToOne = 0;
    features.multiViewport = 1;
    features.samplerAnisotropy = 1;
    features.textureCompressionETC2 = 0;
    features.textureCompressionASTC_LDR = 0;
    features.textureCompressionBC = 0;
    features.occlusionQueryPrecise = 0;
    features.pipelineStatisticsQuery = 1;
    features.vertexPipelineStoresAndAtomics = 1;
    features.fragmentStoresAndAtomics = 1;
    features.shaderTessellationAndGeometryPointSize = 1;
    features.shaderImageGatherExtended = 1;
    features.shaderStorageImageExtendedFormats = 1;
    features.shaderStorageImageMultisample = 1;
    features.shaderStorageImageReadWithoutFormat = 1;
    features.shaderStorageImageWriteWithoutFormat = 1;
    features.shaderUniformBufferArrayDynamicIndexing = 1;
    features.shaderSampledImageArrayDynamicIndexing = 1;
    features.shaderStorageBufferArrayDynamicIndexing = 1;
    features.shaderStorageImageArrayDynamicIndexing = 1;
    features.shaderClipDistance = 1;
    features.shaderCullDistance = 1;
    features.shaderFloat64 = 1;
    features.shaderInt64 = 1;
    features.shaderInt16 = 1;
    features.shaderResourceResidency = 1;
    features.shaderResourceMinLod = 1;
    features.sparseBinding = 0;
    features.sparseResidencyBuffer = 0;
    features.sparseResidencyImage2D = 0;
    features.sparseResidencyImage3D = 0;
    features.sparseResidency2Samples = 0;
    features.sparseResidency4Samples = 0;
    features.sparseResidency8Samples = 0;
    features.sparseResidency16Samples = 0;
    features.sparseResidencyAliased = 0;
    features.variableMultisampleRate = 0;
    features.inheritedQueries = 1;

    VkPhysicalDeviceVulkan12Features vulkan12Features = {};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.samplerMirrorClampToEdge = 1;
    vulkan12Features.runtimeDescriptorArray = 1;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = 1;
    vulkan12Features.shaderStorageBufferArrayNonUniformIndexing = 1;
    vulkan12Features.bufferDeviceAddress = 1;
    vulkan12Features.shaderFloat16 = 1;
    vulkan12Features.drawIndirectCount = 1;

    VkPhysicalDeviceMultiviewFeatures multiviewFeatures = {};
    multiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    multiviewFeatures.pNext = &vulkan12Features;
    multiviewFeatures.multiview = 1;

    VkPhysicalDevice16BitStorageFeatures storage16 = {};
    storage16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
    storage16.pNext = &multiviewFeatures;
    storage16.storageBuffer16BitAccess = 1;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2Features = {};
    sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
    sync2Features.pNext = &storage16;
    sync2Features.synchronization2 = 1;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.pNext = &sync2Features;
    rtPipelineFeatures.rayTracingPipeline = 1;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.pNext = &rtPipelineFeatures;
    asFeatures.accelerationStructure = 1;

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
    physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalDeviceFeatures2.pNext = &asFeatures;
    physicalDeviceFeatures2.features = features;

    std::vector<VkExtensionProperties> supportedDeviceExtensions;
    uint32_t supportedExtensionsCount;

    if (vkEnumerateDeviceExtensionProperties(physDevice->Get(), nullptr, &supportedExtensionsCount, nullptr) == VK_SUCCESS)
    {
        supportedDeviceExtensions.resize(supportedExtensionsCount);
        vkEnumerateDeviceExtensionProperties(physDevice->Get(), nullptr, &supportedExtensionsCount, supportedDeviceExtensions.data());
    }

    std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
    };

    for (const char *n : DLSS::GetDlssVulkanDeviceExtensions())
    {
        const bool isSupported = std::any_of(supportedDeviceExtensions.cbegin(), supportedDeviceExtensions.cend(),
            [&](const VkExtensionProperties& ext)
            {
                return !std::strcmp(ext.extensionName, n);
            }
        );

        if (!isSupported)
        {
            continue;
        }

        deviceExtensions.push_back(n);
    }


    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queues->GetDeviceQueueCreateInfos(queueCreateInfos);

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;
    deviceCreateInfo.pNext = &physicalDeviceFeatures2;
    deviceCreateInfo.enabledExtensionCount = (uint32_t) deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkResult r = vkCreateDevice(physDevice->Get(), &deviceCreateInfo, nullptr, &device);
    VK_CHECKERROR(r);

    InitDeviceExtensionFunctions(device);

    if (enableValidationLayer)
    {
        InitDeviceExtensionFunctions_DebugUtils(device);
    }
}

void VulkanDevice::CreateSyncPrimitives()
{
    VkResult r;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFenceCreateInfo nonSignaledFenceInfo = {};
    nonSignaledFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
        VK_CHECKERROR(r);
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        VK_CHECKERROR(r);
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &inFrameSemaphores[i]);
        VK_CHECKERROR(r);

        r = vkCreateFence(device, &fenceInfo, nullptr, &frameFences[i]);
        VK_CHECKERROR(r);
        r = vkCreateFence(device, &nonSignaledFenceInfo, nullptr, &outOfFrameFences[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, imageAvailableSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, "Image available semaphore");
        SET_DEBUG_NAME(device, renderFinishedSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, "Render finished semaphore");
        SET_DEBUG_NAME(device, inFrameSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, "In-frame semaphore");
        SET_DEBUG_NAME(device, frameFences[i], VK_OBJECT_TYPE_FENCE, "Frame fence");
        SET_DEBUG_NAME(device, outOfFrameFences[i], VK_OBJECT_TYPE_FENCE, "Out of frame fence");
    }
}

VkSurfaceKHR VulkanDevice::GetSurfaceFromUser(VkInstance instance, const RgInstanceCreateInfo &info)
{
    VkSurfaceKHR surface;
    VkResult r;


#ifdef RG_USE_SURFACE_WIN32
    if (info.pWin32SurfaceInfo != nullptr)
    {
        VkWin32SurfaceCreateInfoKHR win32Info = {};
        win32Info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        win32Info.hinstance = info.pWin32SurfaceInfo->hinstance;
        win32Info.hwnd = info.pWin32SurfaceInfo->hwnd;

        r = vkCreateWin32SurfaceKHR(instance, &win32Info, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pWin32SurfaceInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pWin32SurfaceInfo is specified, but the library wasn't built with RG_USE_SURFACE_WIN32 option");
    }
#endif // RG_USE_SURFACE_WIN32


#ifdef RG_USE_SURFACE_METAL
    if (info.pMetalSurfaceCreateInfo != nullptr)
    {
        VkMetalSurfaceCreateInfoEXT metalInfo = {};
        metalInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        metalInfo.pLayer = info.pMetalSurfaceCreateInfo->pLayer;

        r = vkCreateMetalSurfaceEXT(instance, &metalInfo, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pMetalSurfaceCreateInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pMetalSurfaceCreateInfo is specified, but the library wasn't built with RG_USE_SURFACE_METAL option");
    }
#endif // RG_USE_SURFACE_METAL


#ifdef RG_USE_SURFACE_WAYLAND
    if (info.pWaylandSurfaceCreateInfo != nullptr)
    {
        VkWaylandSurfaceCreateInfoKHR wlInfo = {};
        wlInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        wlInfo.display = info.pWaylandSurfaceCreateInfo->display;
        wlInfo.surface = info.pWaylandSurfaceCreateInfo->surface;

        r = (instance, &wlInfo, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pWaylandSurfaceCreateInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pWaylandSurfaceCreateInfo is specified, but the library wasn't built with RG_USE_SURFACE_WAYLAND option");
    }
#endif // RG_USE_SURFACE_WAYLAND


#ifdef RG_USE_SURFACE_XCB
    if (info.pXcbSurfaceCreateInfo != nullptr)
    {
        VkXcbSurfaceCreateInfoKHR xcbInfo = {};
        xcbInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        xcbInfo.connection = info.pXcbSurfaceCreateInfo->connection;
        xcbInfo.window = info.pXcbSurfaceCreateInfo->window;

        r = vkCreateXcbSurfaceKHR(instance, &xcbInfo, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pXcbSurfaceCreateInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pXcbSurfaceCreateInfo is specified, but the library wasn't built with RG_USE_SURFACE_XCB option");
    }
#endif // RG_USE_SURFACE_XCB


#ifdef RG_USE_SURFACE_XLIB
    if (info.pXlibSurfaceCreateInfo != nullptr)
    {
        VkXlibSurfaceCreateInfoKHR xlibInfo = {};
        xlibInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        xlibInfo.dpy = info.pXlibSurfaceCreateInfo->dpy;
        xlibInfo.window = info.pXlibSurfaceCreateInfo->window;

        r = vkCreateXlibSurfaceKHR(instance, &xlibInfo, nullptr, &surface);
        VK_CHECKERROR(r);

        return surface;
    }
#else
    if (info.pXlibSurfaceCreateInfo != nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "pXlibSurfaceCreateInfo is specified, but the library wasn't built with RG_USE_SURFACE_XLIB option");
    }
#endif // RG_USE_SURFACE_XLIB


    throw RgException(RG_WRONG_ARGUMENT, "Surface info wasn't specified");
}

void VulkanDevice::DestroyInstance()
{
    if (debugMessenger != VK_NULL_HANDLE)
    {
        svkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
}

void VulkanDevice::DestroyDevice()
{
    vkDestroyDevice(device, nullptr);
}

void VulkanDevice::DestroySyncPrimitives()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, inFrameSemaphores[i], nullptr);

        vkDestroyFence(device, frameFences[i], nullptr);
        vkDestroyFence(device, outOfFrameFences[i], nullptr);
    }
}

void RTGL1::VulkanDevice::ValidateCreateInfo(const RgInstanceCreateInfo *pInfo)
{
    using namespace std::string_literals;

    if (pInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    {
        int count =
            !!pInfo->pWin32SurfaceInfo +
            !!pInfo->pMetalSurfaceCreateInfo +
            !!pInfo->pWaylandSurfaceCreateInfo +
            !!pInfo->pXcbSurfaceCreateInfo +
            !!pInfo->pXlibSurfaceCreateInfo;

        if (count != 1)
        {
            throw RgException(RG_WRONG_ARGUMENT, "Exactly one of the surface infos must be not null");
        }
    }

    if (pInfo->rasterizedSkyCubemapSize == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "rasterizedSkyCubemapSize must be non-zero");
    }

    if (pInfo->primaryRaysMaxAlbedoLayers > MATERIALS_MAX_LAYER_COUNT)
    {
        throw RgException(RG_WRONG_ARGUMENT, "primaryRaysMaxAlbedoLayers must be <="s + std::to_string(MATERIALS_MAX_LAYER_COUNT));
    }

    if (pInfo->indirectIlluminationMaxAlbedoLayers > MATERIALS_MAX_LAYER_COUNT)
    {
        throw RgException(RG_WRONG_ARGUMENT, "indirectIlluminationMaxAlbedoLayers must be <="s + std::to_string(MATERIALS_MAX_LAYER_COUNT));
    }
}

#pragma endregion 
