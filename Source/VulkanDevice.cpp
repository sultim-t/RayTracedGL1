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
#include <stdexcept>

#include "Matrix.h"
#include "RgException.h"
#include "Utils.h"
#include "Generated/ShaderCommonC.h"

using namespace RTGL1;

VulkanDevice::VulkanDevice(const RgInstanceCreateInfo *info) :
    instance(VK_NULL_HANDLE),
    device(VK_NULL_HANDLE),
    surface(VK_NULL_HANDLE),
    currentFrameIndex(MAX_FRAMES_IN_FLIGHT - 1),
    currentFrameCmd(VK_NULL_HANDLE),
    frameId(1),
    enableValidationLayer(info->enableValidationLayer == RG_TRUE),
    debugMessenger(VK_NULL_HANDLE),
    userPrint{ std::make_unique<UserPrint>(info->pfnPrint, info->pUserPrintData) },
    userFileLoad{ std::make_shared<UserFileLoad>(info->pfnOpenFile, info->pfnCloseFile, info->pUserLoadFileData) },
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

    samplerManager      = std::make_shared<SamplerManager>(device);

    framebuffers        = std::make_shared<Framebuffers>(
        device,
        memAllocator, 
        cmdManager, 
        samplerManager);

    blueNoise           = std::make_shared<BlueNoise>(
        device,
        info->pBlueNoiseFilePath,
        memAllocator,
        cmdManager, 
        samplerManager,
        userFileLoad);

    textureManager      = std::make_shared<TextureManager>(
        device, 
        memAllocator,
        samplerManager, 
        cmdManager,
        userFileLoad,
        *info);

    cubemapManager      = std::make_shared<CubemapManager>(
        device,
        memAllocator,
        samplerManager,
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
        samplerManager,
        memAllocator,
        framebuffers,
        cmdManager,
        swapchain->GetSurfaceFormat(),
        *info);

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
        uniform);

    denoiser            = std::make_shared<Denoiser>(
        device,
        framebuffers,
        shaderManager,
        uniform,
        scene->GetASManager());


    swapchain->Subscribe(rasterizer);

    shaderManager->Subscribe(denoiser);
    shaderManager->Subscribe(imageComposition);
    shaderManager->Subscribe(rasterizer);
    shaderManager->Subscribe(rtPipeline);
    shaderManager->Subscribe(tonemapping);
    shaderManager->Subscribe(scene->GetVertexPreprocessing());
    shaderManager->Subscribe(bloom);

    framebuffers->Subscribe(rasterizer);
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
    denoiser.reset();
    uniform.reset();
    scene.reset();
    shaderManager.reset();
    rtPipeline.reset();
    pathTracer.reset();
    rasterizer.reset();
    samplerManager.reset();
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
    currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    uint32_t frameIndex = currentFrameIndex;

    // wait for previous cmd with the same frame index
    Utils::WaitAndResetFence(device, frameFences[frameIndex]);

    swapchain->RequestNewSize(startInfo.surfaceSize.width, startInfo.surfaceSize.height);
    swapchain->RequestVsync(startInfo.requestVSync);
    swapchain->AcquireImage(imageAvailableSemaphores[frameIndex]);

    if (startInfo.requestShaderReload)
    {
        shaderManager->ReloadShaders();
    }

    // reset cmds for current frame index
    cmdManager->PrepareForFrame(frameIndex);

    // clear the data that were created MAX_FRAMES_IN_FLIGHT ago
    textureManager->PrepareForFrame(frameIndex);
    cubemapManager->PrepareForFrame(frameIndex);
    rasterizer->PrepareForFrame(frameIndex, startInfo.requestRasterizedSkyGeometryReuse);

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();

    BeginCmdLabel(cmd, "Prepare for frame");

    // start dynamic geometry recording to current frame
    scene->PrepareForFrame(cmd, frameIndex);

    return cmd;
}

void VulkanDevice::FillUniform(ShGlobalUniform *gu, const RgDrawFrameInfo &drawInfo) const
{
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
        gu->renderWidth = (float)drawInfo.renderSize.width;
        gu->renderHeight = (float)drawInfo.renderSize.height;
        gu->frameId = frameId;
        gu->timeDelta = (float)std::max<double>(currentFrameTime - previousFrameTime, 0.001);
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
            gu->minLogLuminance = -2.0f;
            gu->maxLogLuminance = 10.0f;
            gu->luminanceWhitePoint = 1.5f;
        }
    }

    {
        gu->lightCountSpherical = scene->GetLightManager()->GetSphericalLightCount();
        gu->lightCountDirectional = scene->GetLightManager()->GetDirectionalLightCount();
        gu->lightCountSphericalPrev = scene->GetLightManager()->GetSphericalLightCountPrev();
        gu->lightCountDirectionalPrev = scene->GetLightManager()->GetDirectionalLightCountPrev();

        // if there are no spotlights, set incorrect values
        // TODO: add spotlight count to global uniform
        if (scene->GetLightManager()->GetSpotlightCount() == 0)
        {
            gu->spotlightRadius = -1;
            gu->spotlightCosAngleOuter = -1;
            gu->spotlightCosAngleInner = -1;
            gu->spotlightFalloffDistance = -1;
        }
    }

    {
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

    if (drawInfo.pDebugParams != nullptr)
    {
        gu->dbgShowGradients = !!drawInfo.pDebugParams->showGradients;
        gu->dbgShowMotionVectors = !!drawInfo.pDebugParams->showMotionVectors;
    }
    else
    {
        gu->dbgShowGradients = false;
        gu->dbgShowMotionVectors = false;
    }

    if (drawInfo.pOverridenTexturesParams != nullptr)
    {
        gu->normalMapStrength = drawInfo.pOverridenTexturesParams->normalMapStrength;
        gu->emissionMapBoost = std::max(drawInfo.pOverridenTexturesParams->emissionMapBoost, 0.0f);
        gu->emissionMaxScreenColor = std::max(drawInfo.pOverridenTexturesParams->emissionMaxScreenColor, 0.0f);
    }
    else
    {
        gu->normalMapStrength = 1.0f;
        gu->emissionMapBoost = 100.0f;
        gu->emissionMaxScreenColor = 1.5f;
    }

    if (drawInfo.pShadowParams != nullptr)
    {
        gu->maxBounceShadowsDirectionalLights = drawInfo.pShadowParams->maxBounceShadowsDirectionalLights;
        gu->maxBounceShadowsSphereLights = drawInfo.pShadowParams->maxBounceShadowsSphereLights;
        gu->maxBounceShadowsSpotlights = drawInfo.pShadowParams->maxBounceShadowsSpotlights;
    }
    else
    {
        gu->maxBounceShadowsDirectionalLights = 8;
        gu->maxBounceShadowsSphereLights = 1;
        gu->maxBounceShadowsSpotlights = 2;
    }

    if (drawInfo.pBloomParams != nullptr)
    {
        gu->bloomThreshold          = std::max(drawInfo.pBloomParams->inputThreshold, 0.0f);
        gu->bloomThresholdLength    = std::max(drawInfo.pBloomParams->inputThresholdLength, 0.0f);
        gu->bloomUpsampleRadius     = std::max(drawInfo.pBloomParams->upsampleRadius, 0.0f);
        gu->bloomIntensity          = std::max(drawInfo.pBloomParams->bloomIntensity, 0.0f);
        gu->bloomEmissionMultiplier = std::max(drawInfo.pBloomParams->bloomEmissionMultiplier, 0.0f);
        gu->bloomSkyMultiplier      = std::max(drawInfo.pBloomParams->bloomSkyMultiplier, 0.0f);
    }
    else
    {
        gu->bloomThreshold = 0.5f;
        gu->bloomThresholdLength = 0.25f;
        gu->bloomUpsampleRadius = 1.0f;
        gu->bloomIntensity = 1.0f;
        gu->bloomEmissionMultiplier = 64.0f;
        gu->bloomSkyMultiplier = 0.05f;
    }

    static_assert(
        RG_MEDIA_TYPE_VACUUM == MEDIA_TYPE_VACUUM &&
        RG_MEDIA_TYPE_WATER == MEDIA_TYPE_WATER &&
        RG_MEDIA_TYPE_GLASS == MEDIA_TYPE_GLASS, 
        "Interface and GLSL constants must be identical");

    static_assert(
        sizeof(gu->portalOutputOffsetFromCamera) >= sizeof(drawInfo.pReflectRefractParams->portalOutputOffsetFromCamera.data),
        "Recheck uniform member and interface member sizes");

    if (drawInfo.pReflectRefractParams != nullptr)
    {
        if (drawInfo.pReflectRefractParams->typeOfMediaAroundCamera < 0 ||
            drawInfo.pReflectRefractParams->typeOfMediaAroundCamera >= MEDIA_TYPE_COUNT - 1)
        {
            gu->cameraMediaType = drawInfo.pReflectRefractParams->typeOfMediaAroundCamera;
        }
        else
        {
            gu->cameraMediaType = MEDIA_TYPE_VACUUM;
        }

        gu->reflectRefractMaxDepth = std::min(4u, drawInfo.pReflectRefractParams->maxReflectRefractDepth);
        memcpy(gu->portalOutputOffsetFromCamera, drawInfo.pReflectRefractParams->portalOutputOffsetFromCamera.data, sizeof(drawInfo.pReflectRefractParams->portalOutputOffsetFromCamera.data));
    }
    else
    {
        gu->cameraMediaType = MEDIA_TYPE_VACUUM;
        gu->reflectRefractMaxDepth = 2;
        gu->portalOutputOffsetFromCamera[0] = gu->portalOutputOffsetFromCamera[1] = gu->portalOutputOffsetFromCamera[2] = 0.0f;
    }

    gu->rayCullMaskWorld = std::min((uint32_t)INSTANCE_MASK_WORLD_ALL, std::max((uint32_t)INSTANCE_MASK_WORLD_MIN, drawInfo.rayCullMaskWorld));
    gu->rayLength = std::min((float)MAX_RAY_LENGTH, std::max(0.1f, drawInfo.rayLength));
}

void VulkanDevice::Render(VkCommandBuffer cmd, const RgDrawFrameInfo &drawInfo)
{
    // end of "Prepare for frame" label
    EndCmdLabel(cmd);


    const uint32_t frameIndex = currentFrameIndex;

    textureManager->SubmitDescriptors(frameIndex);
    cubemapManager->SubmitDescriptors(frameIndex);

    const uint32_t renderWidth  = drawInfo.renderSize.width;
    const uint32_t renderHeight = drawInfo.renderSize.height;
    assert(renderWidth > 0 && renderHeight > 0);

    // submit geometry and upload uniform after getting data from a scene
    const bool sceneNotEmpty = scene->SubmitForFrame(cmd, frameIndex, uniform);

    framebuffers->PrepareForSize(renderWidth, renderHeight);

    bool werePrimaryTraced = false;

    if (!drawInfo.disableRasterization)
    {
        rasterizer->SubmitForFrame(cmd, frameIndex);

        // draw rasterized sky to albedo before tracing primary rays
        if (uniform->GetData()->skyType == RG_SKY_TYPE_RASTERIZED_GEOMETRY)
        {
            rasterizer->DrawSkyToCubemap(cmd, frameIndex, textureManager, uniform);
            rasterizer->DrawSkyToAlbedo(cmd, frameIndex, textureManager, uniform->GetData()->view, uniform->GetData()->skyViewerPosition, uniform->GetData()->projection);
        }
    }

    bool traceRays = sceneNotEmpty && !drawInfo.disableRayTracing;

    if (traceRays)
    {
        pathTracer->Bind(
            cmd, frameIndex, 
            scene, uniform, textureManager, 
            framebuffers, blueNoise, cubemapManager, rasterizer->GetRenderCubemap());

        pathTracer->TracePrimaryRays(cmd, frameIndex, renderWidth, renderHeight);

        werePrimaryTraced = true;

        // save and merge samples from previous illumination results
        denoiser->MergeSamples(cmd, frameIndex, uniform, scene->GetASManager());

        // update the illumination
        pathTracer->PrepareForTracingIllumination(cmd, frameIndex, framebuffers);
        pathTracer->TraceDirectllumination(cmd, frameIndex, renderWidth, renderHeight);
        pathTracer->TraceIndirectllumination(cmd, frameIndex, renderWidth, renderHeight);

        denoiser->Denoise(cmd, frameIndex, uniform);

        // tonemapping
        tonemapping->Tonemap(cmd, frameIndex, uniform);
    }


    if (drawInfo.pBloomParams == nullptr || (drawInfo.pBloomParams != nullptr && drawInfo.pBloomParams->bloomIntensity > 0.0f))
    {
        bloom->Apply(cmd, frameIndex, uniform, !traceRays);
    }


    // final image composition
    imageComposition->Compose(cmd, frameIndex, uniform, tonemapping, !traceRays);

    framebuffers->BarrierOne(cmd, frameIndex, FramebufferImageIndex::FB_IMAGE_INDEX_FINAL);

    if (!drawInfo.disableRasterization)
    {
        // draw rasterized geometry into the final image
        rasterizer->DrawToFinalImage(cmd, frameIndex, textureManager,
                                     uniform->GetData()->view, uniform->GetData()->projection,
                                     werePrimaryTraced);
    }

    // blit result image to present on a surface
    framebuffers->PresentToSwapchain(
        cmd, frameIndex, swapchain, FramebufferImageIndex::FB_IMAGE_INDEX_FINAL,
        renderWidth, renderHeight, VK_IMAGE_LAYOUT_GENERAL);

    if (!drawInfo.disableRasterization)
    {
        rasterizer->DrawToSwapchain(cmd, frameIndex, swapchain->GetCurrentImageIndex(), textureManager, 
                                    uniform->GetData()->view, uniform->GetData()->projection);
    }
}

void VulkanDevice::EndFrame(VkCommandBuffer cmd)
{
    uint32_t frameIndex = currentFrameIndex;

    // submit command buffer, but wait until presentation engine has completed using image
    cmdManager->Submit(
        cmd, 
        imageAvailableSemaphores[frameIndex],
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
    if (currentFrameCmd != VK_NULL_HANDLE)
    {
        throw RgException(RG_FRAME_WASNT_ENDED);
    }

    currentFrameCmd = BeginFrame(*startInfo);
}

void VulkanDevice::DrawFrame(const RgDrawFrameInfo *drawInfo)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    previousFrameTime = currentFrameTime;
    currentFrameTime = drawInfo->currentTime;

    if (drawInfo->renderSize.width > 0 && drawInfo->renderSize.height > 0)
    {
        FillUniform(uniform->GetData(), *drawInfo);
        Render(currentFrameCmd, *drawInfo);
    }

    EndFrame(currentFrameCmd);

    currentFrameCmd = VK_NULL_HANDLE;
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
        uploadInfo->visibilityType != RG_GEOMETRY_VISIBILITY_TYPE_FIRST_PERSON_VIEWER)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Incorrect type of ray traced geometry");
    }

    if (scene->DoesUniqueIDExist(uploadInfo->uniqueID))
    {
        throw RgException(RG_WRONG_ARGUMENT, "Geometry with ID="s + std::to_string(uploadInfo->uniqueID) + " already exists");
    }

    scene->Upload(currentFrameIndex, *uploadInfo);
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

void VulkanDevice::UploadRasterizedGeometry(const RgRasterizedGeometryUploadInfo *uploadInfo,
                                                const float *viewProjection, const RgViewport *viewport)
{
    if (uploadInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (uploadInfo->renderType != RG_RASTERIZED_GEOMETRY_RENDER_TYPE_DEFAULT &&
        uploadInfo->renderType != RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SWAPCHAIN &&
        uploadInfo->renderType != RG_RASTERIZED_GEOMETRY_RENDER_TYPE_SKY)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Incorrect render type of rasterized geometry");
    }

    if ((uploadInfo->pArrays != nullptr && uploadInfo->pStructs != nullptr) || 
        (uploadInfo->pArrays == nullptr && uploadInfo->pStructs == nullptr))
    {
        throw RgException(RG_WRONG_ARGUMENT, "Exactly one of pArrays and pStructs must be not null");
    }

    if ((uploadInfo->pIndexData == nullptr && uploadInfo->indexCount != 0) ||
        (uploadInfo->pIndexData != nullptr && uploadInfo->indexCount == 0))
    {
        throw RgException(RG_WRONG_ARGUMENT, "Incorrect index data");
    }

    rasterizer->Upload(currentFrameIndex, *uploadInfo, viewProjection, viewport);
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

    scene->UploadLight(currentFrameIndex, *pLightInfo);
}

void VulkanDevice::UploadLight(const RgSphericalLightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameIndex, *pLightInfo);
}

void VulkanDevice::UploadLight(const RgSpotlightUploadInfo *pLightInfo)
{
    if (pLightInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    scene->UploadLight(currentFrameIndex, uniform, *pLightInfo);
}

void VulkanDevice::CreateStaticMaterial(const RgStaticMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    if (createInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    *result = textureManager->CreateStaticMaterial(currentFrameCmd, currentFrameIndex, *createInfo);
}

void VulkanDevice::CreateAnimatedMaterial(const RgAnimatedMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    if (createInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (createInfo->frameCount == 0)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Animated materials must have non-zero amount of frames");
    }

    *result = textureManager->CreateAnimatedMaterial(currentFrameCmd, currentFrameIndex, *createInfo);
}

void VulkanDevice::ChangeAnimatedMaterialFrame(RgMaterial animatedMaterial, uint32_t frameIndex)
{
    bool wasChanged = textureManager->ChangeAnimatedMaterialFrame(animatedMaterial, frameIndex);
}

void VulkanDevice::CreateDynamicMaterial(const RgDynamicMaterialCreateInfo *createInfo, RgMaterial *result)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    if (createInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    if (createInfo->size.width == 0 || createInfo->size.height == 0)
    {
        throw RgException(RG_WRONG_MATERIAL_PARAMETER, "Dynamic materials must have non-zero width and height, but given size is (" +
                          std::to_string(createInfo->size.width) + ", " + std::to_string(createInfo->size.height) + ")");
    }
    
    *result = textureManager->CreateDynamicMaterial(currentFrameCmd, currentFrameIndex, *createInfo);
}

void VulkanDevice::UpdateDynamicMaterial(const RgDynamicMaterialUpdateInfo *updateInfo)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    if (updateInfo == nullptr)
    {
        throw RgException(RG_WRONG_ARGUMENT, "Argument is null");
    }

    bool wasUpdated = textureManager->UpdateDynamicMaterial(currentFrameCmd, *updateInfo);
}

void VulkanDevice::DestroyMaterial(RgMaterial material)
{
    textureManager->DestroyMaterial(currentFrameIndex, material);
}
void VulkanDevice::CreateSkyboxCubemap(const RgCubemapCreateInfo *createInfo, RgCubemap *result)
{
    if (currentFrameCmd == VK_NULL_HANDLE)
    {
        throw RgException(RG_FRAME_WASNT_STARTED);
    }

    *result = cubemapManager->CreateCubemap(currentFrameCmd, currentFrameIndex, *createInfo);
}
void VulkanDevice::DestroyCubemap(RgCubemap cubemap)
{
    cubemapManager->DestroyCubemap(currentFrameIndex, cubemap);
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


    VkApplicationInfo appInfo = {};
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = info.pName;

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
    features.tessellationShader = 1;
    features.sampleRateShading = 0;
    features.dualSrcBlend = 1;
    features.logicOp = 1;
    features.multiDrawIndirect = 1;
    features.drawIndirectFirstInstance = 1;
    features.depthClamp = 1;
    features.depthBiasClamp = 1;
    features.fillModeNonSolid = 0;
    features.depthBounds = 1;
    features.wideLines = 0;
    features.largePoints = 0;
    features.alphaToOne = 1;
    features.multiViewport = 0;
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
    features.sparseBinding = 1;
    features.sparseResidencyBuffer = 1;
    features.sparseResidencyImage2D = 1;
    features.sparseResidencyImage3D = 1;
    features.sparseResidency2Samples = 1;
    features.sparseResidency4Samples = 1;
    features.sparseResidency8Samples = 1;
    features.sparseResidency16Samples = 1;
    features.sparseResidencyAliased = 1;
    features.variableMultisampleRate = 0;
    features.inheritedQueries = 1;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexingFeatures.runtimeDescriptorArray = 1;
    indexingFeatures.shaderSampledImageArrayNonUniformIndexing = 1;
    indexingFeatures.shaderStorageBufferArrayNonUniformIndexing = 1;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferAddressFeatures = {};
    bufferAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferAddressFeatures.pNext = &indexingFeatures;
    bufferAddressFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.pNext = &bufferAddressFeatures;
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.pNext = &rtPipelineFeatures;
    asFeatures.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
    physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalDeviceFeatures2.pNext = &asFeatures;
    physicalDeviceFeatures2.features = features;

    std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_MULTIVIEW_EXTENSION_NAME
    };

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

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
        VK_CHECKERROR(r);
        r = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]);
        VK_CHECKERROR(r);

        r = vkCreateFence(device, &fenceInfo, nullptr, &frameFences[i]);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, imageAvailableSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, "Image available semaphore");
        SET_DEBUG_NAME(device, renderFinishedSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, "Render finished semaphore");
        SET_DEBUG_NAME(device, frameFences[i], VK_OBJECT_TYPE_FENCE, "Frame fence");
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

        r = (instance, &, nullptr, &surface);
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

        r = (instance, &, nullptr, &surface);
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

        vkDestroyFence(device, frameFences[i], nullptr);
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
