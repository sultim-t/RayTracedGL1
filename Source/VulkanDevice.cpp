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
#include <cstring>
#include <stdexcept>

#include "HaltonSequence.h"
#include "Matrix.h"
#include "RenderResolutionHelper.h"
#include "RgException.h"
#include "Utils.h"
#include "Generated/ShaderCommonC.h"

using namespace RTGL1;

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

    const float aspect = static_cast< float >( renderResolution.Width() ) /
                         static_cast< float >( renderResolution.Height() );

    {
        memcpy( gu->viewPrev, gu->view, 16 * sizeof( float ) );
        memcpy( gu->projectionPrev, gu->projection, 16 * sizeof( float ) );

        memcpy( gu->view, drawInfo.view, 16 * sizeof( float ) );

        Matrix::MakeProjectionMatrix(
            gu->projection, aspect, drawInfo.fovYRadians, drawInfo.cameraNear, drawInfo.cameraFar );

        Matrix::Inverse( gu->invView, gu->view );
        Matrix::Inverse( gu->invProjection, gu->projection );

        memcpy( gu->cameraPositionPrev, gu->cameraPosition, 3 * sizeof( float ) );
        gu->cameraPosition[ 0 ] = gu->invView[ 12 ];
        gu->cameraPosition[ 1 ] = gu->invView[ 13 ];
        gu->cameraPosition[ 2 ] = gu->invView[ 14 ];
    }

    {
        static_assert(sizeof(gu->instanceGeomInfoOffset) == sizeof(gu->instanceGeomInfoOffsetPrev), "");
        memcpy(gu->instanceGeomInfoOffsetPrev, gu->instanceGeomInfoOffset, sizeof(gu->instanceGeomInfoOffset));
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

        RgFloat2D jitter =
            renderResolution.IsNvDlssEnabled() ? HaltonSequence::GetJitter_Halton23(frameId) :
            renderResolution.IsAmdFsr2Enabled() ? FSR2::GetJitter(renderResolution.GetResolutionState(), frameId) :
            RgFloat2D{ 0, 0 };

        gu->jitterX = jitter.data[0];
        gu->jitterY = jitter.data[1];
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
        gu->lightCount         = scene->GetLightManager()->GetLightCount();
        gu->lightCountPrev     = scene->GetLightManager()->GetLightCountPrev();

        gu->directionalLightExists = scene->GetLightManager()->DoesDirectionalLightExist();
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
            gu->skyType = SKY_TYPE_COLOR;
            gu->skyCubemapIndex = RG_EMPTY_CUBEMAP;
        }

        RgFloat3D skyViewerPosition = drawInfo.pSkyParams ? drawInfo.pSkyParams->skyViewerPosition : RgFloat3D{ 0,0,0 };

        for (uint32_t i = 0; i < 6; i++)
        {
            float *viewProjDst = &gu->viewProjCubemap[16 * i];

            Matrix::GetCubemapViewProjMat(viewProjDst, i, skyViewerPosition.data, drawInfo.cameraNear, drawInfo.cameraFar);
        }
    }

    gu->debugShowFlags = 0;

    if (drawInfo.pDebugParams != nullptr)
    {
        RgDebugDrawFlags fs = drawInfo.pDebugParams->drawFlags;
        
        if (fs & RG_DEBUG_DRAW_ONLY_DIFFUSE_DIRECT_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_ONLY_DIRECT_DIFFUSE;
        }
        else if (fs & RG_DEBUG_DRAW_ONLY_DIFFUSE_INDIRECT_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_ONLY_INDIRECT_DIFFUSE;
        }
        else if (fs & RG_DEBUG_DRAW_ONLY_SPECULAR_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_ONLY_SPECULAR;
        }
        else if (fs & RG_DEBUG_DRAW_UNFILTERED_DIFFUSE_DIRECT_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_UNFILTERED_DIFFUSE;
        }
        else if (fs & RG_DEBUG_DRAW_UNFILTERED_DIFFUSE_INDIRECT_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_UNFILTERED_INDIRECT;
        }
        else if (fs & RG_DEBUG_DRAW_UNFILTERED_SPECULAR_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_UNFILTERED_SPECULAR;
        }

        if (fs & RG_DEBUG_DRAW_ALBEDO_WHITE_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_ALBEDO_WHITE;
        }
        if (fs & RG_DEBUG_DRAW_MOTION_VECTORS_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_MOTION_VECTORS;
        }
        if (fs & RG_DEBUG_DRAW_GRADIENTS_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_GRADIENTS;
        }
        if (fs & RG_DEBUG_DRAW_LIGHT_GRID_BIT)
        {
            gu->debugShowFlags |= DEBUG_SHOW_FLAG_LIGHT_GRID;
        }
    }

    if (drawInfo.pTexturesParams != nullptr)
    {
        gu->normalMapStrength = drawInfo.pTexturesParams->normalMapStrength;
        gu->emissionMapBoost = std::max(drawInfo.pTexturesParams->emissionMapBoost, 0.0f);
        gu->emissionMaxScreenColor = std::max(drawInfo.pTexturesParams->emissionMaxScreenColor, 0.0f);
        gu->useSqrtRoughnessForIndirect = !!drawInfo.pTexturesParams->useSqrtRoughnessForIndirect;
        gu->minRoughness = std::clamp(drawInfo.pTexturesParams->minRoughness, 0.0f, 1.0f);
    }
    else
    {
        gu->normalMapStrength = 1.0f;
        gu->emissionMapBoost = 100.0f;
        gu->emissionMaxScreenColor = 1.5f;
        gu->useSqrtRoughnessForIndirect = false;
        gu->minRoughness = 0.0f;
    }

    if (drawInfo.pIlluminationParams != nullptr)
    {
        gu->maxBounceShadowsLights      = drawInfo.pIlluminationParams->maxBounceShadows;
        gu->polyLightSpotlightFactor    = std::max(0.0f, drawInfo.pIlluminationParams->polygonalLightSpotlightFactor);
        gu->firefliesClamp              = std::max(0.0f, drawInfo.pIlluminationParams->sphericalPolygonalLightsFirefliesClamp);
        gu->lightIndexIgnoreFPVShadows  = scene->GetLightManager()->GetLightIndexIgnoreFPVShadows(currentFrameState.GetFrameIndex(), drawInfo.pIlluminationParams->lightUniqueIdIgnoreFirstPersonViewerShadows);
        gu->cellWorldSize               = std::max(drawInfo.pIlluminationParams->cellWorldSize, 0.001f);
        gu->gradientMultDiffuse         = std::clamp(drawInfo.pIlluminationParams->directDiffuseSensitivityToChange, 0.0f, 1.0f);
        gu->gradientMultIndirect        = std::clamp(drawInfo.pIlluminationParams->indirectDiffuseSensitivityToChange, 0.0f, 1.0f);
        gu->gradientMultSpecular        = std::clamp(drawInfo.pIlluminationParams->specularSensitivityToChange, 0.0f, 1.0f);
    }
    else
    {
        gu->maxBounceShadowsLights = 2;
        gu->polyLightSpotlightFactor = 2.0f;
        gu->firefliesClamp = 3.0f;
        gu->lightIndexIgnoreFPVShadows = LIGHT_INDEX_NONE;
        gu->cellWorldSize = 1.0f;
        gu->gradientMultDiffuse = 0.5f;
        gu->gradientMultIndirect = 0.2f;
        gu->gradientMultSpecular = 0.5f;
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

        gu->twirlPortalNormal = !!rr.portalNormalTwirl;
    }
    else
    {
        gu->cameraMediaType = MEDIA_TYPE_VACUUM;
        gu->reflectRefractMaxDepth = 2;
    
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

        gu->twirlPortalNormal = false;
    }

    gu->rayCullBackFaces = rayCullBackFacingTriangles ? 1 : 0;
    gu->rayLength = clamp(drawInfo.rayLength, 0.1f, (float)MAX_RAY_LENGTH);
    gu->primaryRayMinDist = clamp(drawInfo.cameraNear, 0.001f, gu->rayLength);

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

    if (drawInfo.pLightmapParams != nullptr)
    {
        gu->lightmapEnable = !!drawInfo.pLightmapParams->enableLightmaps;

        if (drawInfo.pLightmapParams->lightmapLayerIndex == 1 || drawInfo.pLightmapParams->lightmapLayerIndex == 2)
        {
            gu->lightmapLayer = drawInfo.pLightmapParams->lightmapLayerIndex;
        }
        else
        {
            assert(0 && "pLightMapLayerIndex must point to a value of 1 or 2. Others are invalidated");
        }
    }
    else
    {
        gu->lightmapEnable = false;
        gu->lightmapLayer = UINT8_MAX;
    }

    gu->lensFlareCullingInputCount = rasterizer->GetLensFlareCullingInputCount();
    gu->applyViewProjToLensFlares = !lensFlareVerticesInScreenSpace;

    {
        memcpy( gu->volumeViewProj_Prev, gu->volumeViewProj, 16 * sizeof( float ) );
        memcpy( gu->volumeViewProjInv_Prev, gu->volumeViewProjInv, 16 * sizeof( float ) );

        gu->volumeCameraNear = std::max( 0.001f, drawInfo.cameraNear );
        gu->volumeCameraFar =
            std::clamp( drawInfo.volumetricFar, gu->volumeCameraNear + 0.01f, drawInfo.cameraFar );

        float volumeproj[ 16 ];
        Matrix::MakeProjectionMatrix(
            volumeproj, aspect, drawInfo.fovYRadians, gu->volumeCameraNear, gu->volumeCameraFar );

        Matrix::Multiply( gu->volumeViewProj, gu->view, volumeproj );
        Matrix::Inverse( gu->volumeViewProjInv, gu->volumeViewProj );
    }

    gu->antiFireflyEnabled = !!drawInfo.forceAntiFirefly;
}

void VulkanDevice::Render(VkCommandBuffer cmd, const RgDrawFrameInfo &drawInfo)
{
    // end of "Prepare for frame" label
    EndCmdLabel(cmd);


    const uint32_t frameIndex = currentFrameState.GetFrameIndex();

    
    bool mipLodBiasUpdated = worldSamplerManager->TryChangeMipLodBias(frameIndex, renderResolution.GetMipLodBias());
    const RgFloat2D jitter = { uniform->GetData()->jitterX, uniform->GetData()->jitterY };

    textureManager->SubmitDescriptors(frameIndex, drawInfo.pTexturesParams, mipLodBiasUpdated);
    cubemapManager->SubmitDescriptors(frameIndex);


    // submit geometry and upload uniform after getting data from a scene
    scene->SubmitForFrame(cmd, frameIndex, uniform, 
                          uniform->GetData()->rayCullMaskWorld, 
                          allowGeometryWithSkyFlag, 
                          drawInfo.pReflectRefractParams ? drawInfo.pReflectRefractParams->isReflRefrAlphaTested : false,
                          drawInfo.disableRayTracedGeometry);


    framebuffers->PrepareForSize(renderResolution.GetResolutionState());
    

    if (!drawInfo.disableRasterization)
    {
        rasterizer->SubmitForFrame(cmd, frameIndex);

        // draw rasterized sky to albedo before tracing primary rays
        if (uniform->GetData()->skyType == RG_SKY_TYPE_RASTERIZED_GEOMETRY)
        {
            RgFloat3D skyViewerPosition = drawInfo.pSkyParams ? drawInfo.pSkyParams->skyViewerPosition : RgFloat3D{ 0,0,0 };

            rasterizer->DrawSkyToCubemap(cmd, frameIndex, textureManager, uniform);
            rasterizer->DrawSkyToAlbedo(cmd, frameIndex, textureManager, uniform->GetData()->view, skyViewerPosition.data, uniform->GetData()->projection, jitter, renderResolution);
        }
    }


    {
        lightGrid->Build(cmd, frameIndex, uniform, blueNoise, scene->GetLightManager());

        decalManager->SubmitForFrame(cmd, frameIndex);
        portalList->SubmitForFrame(cmd, frameIndex);

        const auto params = pathTracer->Bind( cmd,
                                              frameIndex,
                                              renderResolution.Width(),
                                              renderResolution.Height(),
                                              scene.get(),
                                              uniform.get(),
                                              textureManager.get(),
                                              framebuffers,
                                              restirBuffers.get(),
                                              blueNoise.get(),
                                              cubemapManager.get(),
                                              rasterizer->GetRenderCubemap().get(),
                                              portalList.get(),
                                              volumetric.get() );

        pathTracer->TracePrimaryRays(params);

        // draw decals on top of primary surface
        decalManager->Draw(cmd, frameIndex, uniform, framebuffers, textureManager);

        if (uniform->GetData()->reflectRefractMaxDepth > 0)
        {
            pathTracer->TraceReflectionRefractionRays(params);
        }

        scene->GetLightManager()->BarrierLightGrid(cmd, frameIndex);
        pathTracer->CalculateInitialReservoirs(params);
        pathTracer->TraceDirectllumination(params);
        pathTracer->TraceIndirectllumination(params);
        pathTracer->TraceVolumetric(params);

        pathTracer->CalculateGradientsSamples(params);
        denoiser->Denoise(cmd, frameIndex, uniform);
        volumetric->Process( cmd, frameIndex, uniform.get(), blueNoise.get() );
        tonemapping->CalculateExposure(cmd, frameIndex, uniform);
    }

    imageComposition->PrepareForRaster( cmd, frameIndex, uniform.get() );
    volumetric->BarrierToReadProcessed( cmd, frameIndex );

    if (!drawInfo.disableRasterization)
    {
        // draw rasterized geometry into the final image
        rasterizer->DrawToFinalImage(cmd, frameIndex, textureManager,
                                     uniform->GetData()->view, uniform->GetData()->projection,
                                     jitter, renderResolution,
                                     drawInfo.pLensFlareParams,
                                     drawInfo.pBloomParams ? drawInfo.pBloomParams->bloomRasterMultiplier : 0.0f);
    }

    imageComposition->Finalize(
        cmd, frameIndex, uniform.get(), tonemapping.get(), volumetric.get() );


    bool enableBloom = drawInfo.pBloomParams == nullptr || (drawInfo.pBloomParams != nullptr && drawInfo.pBloomParams->bloomIntensity > 0.0f);

    if (enableBloom)
    {
        bloom->Prepare(cmd, frameIndex, uniform, tonemapping);
    }


    FramebufferImageIndex currentResultImage = FramebufferImageIndex::FB_IMAGE_INDEX_FINAL;
    {
        // upscale finalized image
        if (renderResolution.IsNvDlssEnabled())
        {
            currentResultImage = nvDlss->Apply(cmd, frameIndex, 
                                               framebuffers, 
                                               renderResolution, 
                                               jitter);
        }
        else if (renderResolution.IsAmdFsr2Enabled())
        {
            currentResultImage = amdFsr2->Apply(cmd, frameIndex, 
                                                framebuffers, 
                                                renderResolution, 
                                                jitter, 
                                                uniform->GetData()->timeDelta, 
                                                drawInfo.cameraNear, 
                                                drawInfo.cameraFar, 
                                                drawInfo.fovYRadians );
        }

        const RgExtent2D* pixelized = drawInfo.pRenderResolutionParams
                                          ? drawInfo.pRenderResolutionParams->pPixelizedRenderSize
                                          : nullptr;

        currentResultImage = framebuffers->BlitForEffects(
            cmd, frameIndex, currentResultImage, renderResolution.GetBlitFilter(), pixelized );
    }


    const CommonnlyUsedEffectArguments args = { cmd, frameIndex, framebuffers, uniform, renderResolution.UpscaledWidth(), renderResolution.UpscaledHeight(), (float)currentFrameTime };
    {
        if (renderResolution.IsDedicatedSharpeningEnabled())
        {
            currentResultImage = sharpening->Apply(
                cmd, frameIndex, framebuffers, renderResolution.UpscaledWidth(), renderResolution.UpscaledHeight(), currentResultImage,
                renderResolution.GetSharpeningTechnique(), renderResolution.GetSharpeningIntensity());
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
        if (effectWaves->Setup(args, drawInfo.postEffectParams.pWaves))
        {
            currentResultImage = effectWaves->Apply(args, currentResultImage);
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

    textureManager->CheckForHotReload(cmd);

    if (renderResolution.Width() > 0 && renderResolution.Height() > 0)
    {
        FillUniform(uniform->GetData(), *drawInfo);
        Render(cmd, *drawInfo);
    }

    EndFrame(cmd);
    currentFrameState.OnEndFrame();
}

bool VulkanDevice::IsSuspended() const
{
    if (!swapchain)
    {
        return false;
    }

    return !swapchain->IsExtentOptimal();
}

bool RTGL1::VulkanDevice::IsRenderUpscaleTechniqueAvailable(RgRenderUpscaleTechnique technique) const
{
    switch (technique)
    {
        case RG_RENDER_UPSCALE_TECHNIQUE_NEAREST:
        case RG_RENDER_UPSCALE_TECHNIQUE_LINEAR:
        case RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2:
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

    if (uploadInfo->pVertices == nullptr || uploadInfo->vertexCount == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Incorrect vertex data");
    }

    if ((uploadInfo->pIndices == nullptr && uploadInfo->indexCount != 0) ||
        (uploadInfo->pIndices != nullptr && uploadInfo->indexCount == 0))
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

    if (uploadInfo->pPortalIndex != nullptr && uploadInfo->passThroughType != RG_GEOMETRY_PASS_THROUGH_TYPE_PORTAL)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Geometry's pPortalIndex is non-null, but geometry is not marked as portal");
    }

    if (uploadInfo->pPortalIndex == nullptr && uploadInfo->passThroughType == RG_GEOMETRY_PASS_THROUGH_TYPE_PORTAL)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Geometry is marked as portal, but pPortalIndex is null");
    }

    if (uploadInfo->pPortalIndex && *(uploadInfo->pPortalIndex) >= PORTAL_MAX_COUNT)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Geometry's portal index must be in [0, 62]");
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

    if (pUploadInfo->pVertices == nullptr || pUploadInfo->vertexCount == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Vertex data / count is null");
    }

    if ((pUploadInfo->pIndices == nullptr && pUploadInfo->indexCount != 0) ||
        (pUploadInfo->pIndices != nullptr && pUploadInfo->indexCount == 0))
    {
        throw RgException(RG_WRONG_ARGUMENT, "Index data / count must be both not null or null");
    }

    rasterizer->Upload(currentFrameState.GetFrameIndex(), *pUploadInfo, pViewProjection, pViewport);
}

void RTGL1::VulkanDevice::UploadLensFlare(const RgLensFlareUploadInfo *pUploadInfo)
{
    if (pUploadInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (pUploadInfo->indexCount == 0 || pUploadInfo->pIndices == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Lens flare index data and count must not be null");
    }

    if (pUploadInfo->vertexCount == 0 || pUploadInfo->pVertices == nullptr)
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

void RTGL1::VulkanDevice::UploadPortal(const RgPortalUploadInfo *pUploadInfo)
{
    if (pUploadInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    portalList->Upload(currentFrameState.GetFrameIndex(), *pUploadInfo);
}

void VulkanDevice::SubmitStaticGeometries()
{
    scene->SubmitStatic();
}

void VulkanDevice::StartNewStaticScene()
{
    scene->StartNewStatic();
}

void VulkanDevice::UploadDirectionalLight(const RgDirectionalLightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameState.GetFrameIndex(), *pLightInfo);
}

void VulkanDevice::UploadSphericalLight(const RgSphericalLightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameState.GetFrameIndex(), *pLightInfo);
}

void VulkanDevice::UploadSpotlight(const RgSpotLightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameState.GetFrameIndex(), *pLightInfo);
}

void RTGL1::VulkanDevice::UploadPolygonalLight(const RgPolygonalLightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameState.GetFrameIndex(), *pLightInfo);
}

void VulkanDevice::CreateMaterial(const RgMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (createInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    *result = textureManager->CreateMaterial
    (
        currentFrameState.GetCmdBufferForMaterials(cmdManager),
        currentFrameState.GetFrameIndex(),
        *createInfo
    );
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

    *result = textureManager->CreateAnimatedMaterial
    (
        currentFrameState.GetCmdBufferForMaterials(cmdManager),
        currentFrameState.GetFrameIndex(),
        *createInfo
    );
}

void VulkanDevice::ChangeAnimatedMaterialFrame(RgMaterial animatedMaterial, uint32_t frameIndex)
{
    if (!currentFrameState.WasFrameStarted())
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    bool wasChanged = textureManager->ChangeAnimatedMaterialFrame(animatedMaterial, frameIndex);
}

void VulkanDevice::UpdateMaterial(const RgMaterialUpdateInfo *updateInfo)
{
    if (!currentFrameState.WasFrameStarted())
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    if (updateInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    bool wasUpdated = textureManager->UpdateMaterial(currentFrameState.GetCmdBuffer(), *updateInfo);
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

