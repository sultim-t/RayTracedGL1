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

#include "HaltonSequence.h"
#include "Matrix.h"
#include "RenderResolutionHelper.h"
#include "RgException.h"
#include "Utils.h"

#include "Generated/ShaderCommonC.h"

#include <algorithm>
#include <cstring>

VkCommandBuffer RTGL1::VulkanDevice::BeginFrame( const RgStartFrameInfo& info )
{
    uint32_t frameIndex = currentFrameState.IncrementFrameIndexAndGet();

    if( !waitForOutOfFrameFence )
    {
        // wait for previous cmd with the same frame index
        Utils::WaitAndResetFence( device, frameFences[ frameIndex ] );
    }
    else
    {
        Utils::WaitAndResetFences(
            device, frameFences[ frameIndex ], outOfFrameFences[ frameIndex ] );
    }

    swapchain->RequestVsync( vsync );
    swapchain->AcquireImage( imageAvailableSemaphores[ frameIndex ] );

    VkSemaphore semaphoreToWaitOnSubmit = imageAvailableSemaphores[ frameIndex ];


    // if out-of-frame cmd exist, submit it
    {
        VkCommandBuffer preFrameCmd = currentFrameState.GetPreFrameCmdAndRemove();
        if( preFrameCmd != VK_NULL_HANDLE )
        {
            // Signal inFrameSemaphore after completion.
            // Signal outOfFrameFences, but for the next frame
            // because we can't reset cmd pool with cmds (in this case
            // it's preFrameCmd) that are in use.
            cmdManager->Submit( preFrameCmd,
                                semaphoreToWaitOnSubmit,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                inFrameSemaphores[ frameIndex ],
                                outOfFrameFences[ ( frameIndex + 1 ) % MAX_FRAMES_IN_FLIGHT ] );

            // should wait other semaphore in this case
            semaphoreToWaitOnSubmit = inFrameSemaphores[ frameIndex ];

            waitForOutOfFrameFence = true;
        }
        else
        {
            waitForOutOfFrameFence = false;
        }
    }
    currentFrameState.SetSemaphore( semaphoreToWaitOnSubmit );


    if( devmode && devmode->reloadShaders )
    {
        shaderManager->ReloadShaders();
        devmode->reloadShaders = false;
    }
    sceneImportExport->PrepareForFrame();


    // reset cmds for current frame index
    cmdManager->PrepareForFrame( frameIndex );

    // clear the data that were created MAX_FRAMES_IN_FLIGHT ago
    worldSamplerManager->PrepareForFrame( frameIndex );
    genericSamplerManager->PrepareForFrame( frameIndex );
    textureManager->PrepareForFrame( frameIndex );
    cubemapManager->PrepareForFrame( frameIndex );
    rasterizer->PrepareForFrame( frameIndex );
    decalManager->PrepareForFrame( frameIndex );
    if( debugWindows )
    {
        if( !debugWindows->PrepareForFrame( frameIndex ) )
        {
            debugWindows.reset();
        }
    }
    if( devmode )
    {
        devmode->primitivesTable.clear();
    }

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();
    BeginCmdLabel( cmd, "Prepare for frame" );

    textureManager->TryHotReload( cmd, frameIndex );
    lightManager->PrepareForFrame( cmd, frameIndex );
    scene->PrepareForFrame( cmd,
                            frameIndex,
                            info.ignoreExternalGeometry ||
                                ( devmode && devmode->ignoreExternalGeometry ) );

    {
        sceneImportExport->CheckForNewScene( Utils::SafeCstr( info.pMapName ),
                                             cmd,
                                             frameIndex,
                                             *scene,
                                             *textureManager,
                                             *textureMetaManager );
        scene->SubmitStaticLights( frameIndex, *lightManager );
    }

    return cmd;
}

void RTGL1::VulkanDevice::FillUniform( RTGL1::ShGlobalUniform* gu,
                                       const RgDrawFrameInfo&  drawInfo ) const
{
    const float IdentityMat4x4[ 16 ] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

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
        static_assert( sizeof( gu->instanceGeomInfoOffset ) ==
                       sizeof( gu->instanceGeomInfoOffsetPrev ) );
        memcpy( gu->instanceGeomInfoOffsetPrev,
                gu->instanceGeomInfoOffset,
                sizeof( gu->instanceGeomInfoOffset ) );
    }

    {
        gu->frameId   = frameId;
        gu->timeDelta = static_cast< float >(
            std::max< double >( currentFrameTime - previousFrameTime, 0.001 ) );
        gu->time = static_cast< float >( currentFrameTime );
    }

    {
        gu->renderWidth  = static_cast< float >( renderResolution.Width() );
        gu->renderHeight = static_cast< float >( renderResolution.Height() );
        // render width must be always even for checkerboarding!
        assert( ( int )gu->renderWidth % 2 == 0 );

        gu->upscaledRenderWidth  = static_cast< float >( renderResolution.UpscaledWidth() );
        gu->upscaledRenderHeight = static_cast< float >( renderResolution.UpscaledHeight() );

        RgFloat2D jitter = renderResolution.IsNvDlssEnabled()
                               ? HaltonSequence::GetJitter_Halton23( frameId )
                           : renderResolution.IsAmdFsr2Enabled()
                               ? FSR2::GetJitter( renderResolution.GetResolutionState(), frameId )
                               : RgFloat2D{ 0, 0 };

        gu->jitterX = jitter.data[ 0 ];
        gu->jitterY = jitter.data[ 1 ];
    }

    {
        const auto& params = AccessParams( drawInfo.pTonemappingParams );

        float luminanceMin = std::exp2( params.ev100Min ) * 12.5f / 100.0f;
        float luminanceMax = std::exp2( params.ev100Max ) * 12.5f / 100.0f;

        gu->stopEyeAdaptation   = params.disableEyeAdaptation;
        gu->minLogLuminance     = std::log2( luminanceMin );
        gu->maxLogLuminance     = std::log2( luminanceMax );
        gu->luminanceWhitePoint = params.luminanceWhitePoint;
    }

    {
        gu->lightCount     = lightManager->GetLightCount();
        gu->lightCountPrev = lightManager->GetLightCountPrev();

        gu->directionalLightExists = lightManager->DoesDirectionalLightExist();
    }

    {
        const auto& params = AccessParams( drawInfo.pSkyParams );

        static_assert( sizeof( gu->skyCubemapRotationTransform ) == sizeof( IdentityMat4x4 ) &&
                           sizeof( IdentityMat4x4 ) == 16 * sizeof( float ),
                       "Recheck skyCubemapRotationTransform sizes" );
        memcpy( gu->skyCubemapRotationTransform, IdentityMat4x4, 16 * sizeof( float ) );


        RG_SET_VEC3_A( gu->skyColorDefault, params.skyColorDefault.data );
        gu->skyColorMultiplier = std::max( 0.0f, params.skyColorMultiplier );
        gu->skyColorSaturation = std::max( 0.0f, params.skyColorSaturation );

        switch( params.skyType )
        {
            case RG_SKY_TYPE_COLOR: {
                gu->skyType = SKY_TYPE_COLOR;
                break;
            }
            case RG_SKY_TYPE_CUBEMAP: {
                gu->skyType = SKY_TYPE_CUBEMAP;
                break;
            }
            case RG_SKY_TYPE_RASTERIZED_GEOMETRY: {
                gu->skyType = SKY_TYPE_RASTERIZED_GEOMETRY;
                break;
            }
            default: gu->skyType = SKY_TYPE_COLOR;
        }

        gu->skyCubemapIndex =
            cubemapManager->TryGetDescriptorIndex( params.pSkyCubemapTextureName );

        if( !Utils::IsAlmostZero( params.skyCubemapRotationTransform ) )
        {
            Utils::SetMatrix3ToGLSLMat4( gu->skyCubemapRotationTransform,
                                         params.skyCubemapRotationTransform );
        }

        RgFloat3D skyViewerPosition = params.skyViewerPosition;

        for( uint32_t i = 0; i < 6; i++ )
        {
            float* viewProjDst = &gu->viewProjCubemap[ 16 * i ];

            Matrix::GetCubemapViewProjMat(
                viewProjDst, i, skyViewerPosition.data, drawInfo.cameraNear, drawInfo.cameraFar );
        }
    }

    gu->debugShowFlags = devmode ? devmode->debugShowFlags : 0;

    {
        const auto& params = AccessParams( drawInfo.pTexturesParams );

        gu->normalMapStrength      = params.normalMapStrength;
        gu->emissionMapBoost       = std::max( params.emissionMapBoost, 0.0f );
        gu->emissionMaxScreenColor = std::max( params.emissionMaxScreenColor, 0.0f );
        gu->minRoughness           = std::clamp( params.minRoughness, 0.0f, 1.0f );
    }

    {
        const auto& params = AccessParams( drawInfo.pIlluminationParams );

        gu->maxBounceShadowsLights     = params.maxBounceShadows;
        gu->polyLightSpotlightFactor   = std::max( 0.0f, params.polygonalLightSpotlightFactor );
        gu->indirSecondBounce          = !!params.enableSecondBounceForIndirect;
        gu->lightIndexIgnoreFPVShadows = lightManager->GetLightIndexForShaders(
            currentFrameState.GetFrameIndex(), params.lightUniqueIdIgnoreFirstPersonViewerShadows );
        gu->cellWorldSize       = std::max( params.cellWorldSize, 0.001f );
        gu->gradientMultDiffuse = std::clamp( params.directDiffuseSensitivityToChange, 0.0f, 1.0f );
        gu->gradientMultIndirect =
            std::clamp( params.indirectDiffuseSensitivityToChange, 0.0f, 1.0f );
        gu->gradientMultSpecular = std::clamp( params.specularSensitivityToChange, 0.0f, 1.0f );
    }

    {
        const auto& params = AccessParams( drawInfo.pBloomParams );

        gu->bloomThreshold          = std::max( params.inputThreshold, 0.0f );
        gu->bloomIntensity          = std::max( params.bloomIntensity, 0.0f );
        gu->bloomEmissionMultiplier = std::max( params.bloomEmissionMultiplier, 0.0f );
    }

    {
        const auto& params = AccessParams( drawInfo.pReflectRefractParams );

        switch( params.typeOfMediaAroundCamera )
        {
            case RG_MEDIA_TYPE_VACUUM: gu->cameraMediaType = MEDIA_TYPE_VACUUM; break;
            case RG_MEDIA_TYPE_WATER: gu->cameraMediaType = MEDIA_TYPE_WATER; break;
            case RG_MEDIA_TYPE_GLASS: gu->cameraMediaType = MEDIA_TYPE_GLASS; break;
            case RG_MEDIA_TYPE_ACID: gu->cameraMediaType = MEDIA_TYPE_ACID; break;
            default: gu->cameraMediaType = MEDIA_TYPE_VACUUM;
        }

        gu->reflectRefractMaxDepth = std::min( 4u, params.maxReflectRefractDepth );

        gu->indexOfRefractionGlass = std::max( 0.0f, params.indexOfRefractionGlass );
        gu->indexOfRefractionWater = std::max( 0.0f, params.indexOfRefractionWater );

        memcpy( gu->waterColorAndDensity, params.waterColor.data, 3 * sizeof( float ) );
        gu->waterColorAndDensity[ 3 ] = 0.0f;

        memcpy( gu->acidColorAndDensity, params.acidColor.data, 3 * sizeof( float ) );
        gu->acidColorAndDensity[ 3 ] = std::max( 0.0f, params.acidDensity );

        gu->forceNoWaterRefraction = !!params.forceNoWaterRefraction;
        gu->waterWaveSpeed         = params.waterWaveSpeed;
        gu->waterWaveStrength      = params.waterWaveNormalStrength;
        gu->waterTextureDerivativesMultiplier =
            std::max( 0.0f, params.waterWaveTextureDerivativesMultiplier );
        gu->waterTextureAreaScale =
            params.waterTextureAreaScale < 0.0001f ? 1.0f : params.waterTextureAreaScale;

        gu->noBackfaceReflForNoMediaChange = !!params.disableBackfaceReflectionsForNoMediaChange;

        gu->twirlPortalNormal = !!params.portalNormalTwirl;
    }

    gu->rayCullBackFaces  = rayCullBackFacingTriangles ? 1 : 0;
    gu->rayLength         = clamp( drawInfo.rayLength, 0.1f, float( MAX_RAY_LENGTH ) );
    gu->primaryRayMinDist = clamp( drawInfo.cameraNear, 0.001f, gu->rayLength );

    {
        gu->rayCullMaskWorld = 0;

        if( drawInfo.rayCullMaskWorld & RG_DRAW_FRAME_RAY_CULL_WORLD_0_BIT )
        {
            gu->rayCullMaskWorld |= INSTANCE_MASK_WORLD_0;
        }

        if( drawInfo.rayCullMaskWorld & RG_DRAW_FRAME_RAY_CULL_WORLD_1_BIT )
        {
            gu->rayCullMaskWorld |= INSTANCE_MASK_WORLD_1;
        }

        if( drawInfo.rayCullMaskWorld & RG_DRAW_FRAME_RAY_CULL_WORLD_2_BIT )
        {
            if( allowGeometryWithSkyFlag )
            {
                throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                                   "RG_DRAW_FRAME_RAY_CULL_WORLD_2_BIT cannot be used, as "
                                   "RgInstanceCreateInfo::allowGeometryWithSkyFlag was true" );
            }

            gu->rayCullMaskWorld |= INSTANCE_MASK_WORLD_2;
        }

#if RAYCULLMASK_SKY_IS_WORLD2
        if( drawInfo.rayCullMaskWorld & RG_DRAW_FRAME_RAY_CULL_SKY_BIT )
        {
            if( !allowGeometryWithSkyFlag )
            {
                throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                                   "RG_DRAW_FRAME_RAY_CULL_SKY_BIT cannot be used, as "
                                   "RgInstanceCreateInfo::allowGeometryWithSkyFlag was false" );
            }

            gu->rayCullMaskWorld |= INSTANCE_MASK_WORLD_2;
        }
#else
    #error Handle RG_DRAW_FRAME_RAY_CULL_SKY_BIT, if there is no WORLD_2
#endif


        if( allowGeometryWithSkyFlag )
        {
            gu->rayCullMaskWorld_Shadow = gu->rayCullMaskWorld & ( ~INSTANCE_MASK_WORLD_2 );
        }
        else
        {
            gu->rayCullMaskWorld_Shadow = gu->rayCullMaskWorld;
        }
    }

    gu->waterNormalTextureIndex = textureManager->GetWaterNormalTextureIndex();
    gu->dirtMaskTextureIndex    = textureManager->GetDirtMaskTextureIndex();

    gu->cameraRayConeSpreadAngle = atanf( ( 2.0f * tanf( drawInfo.fovYRadians * 0.5f ) ) /
                                          float( renderResolution.Height() ) );

    RG_SET_VEC3_A( gu->worldUpVector, sceneImportExport->GetWorldUp().data );

    {
        const auto& params = AccessParams( drawInfo.pLightmapParams );
        
        gu->lightmapScreenCoverage = params.lightmapScreenCoverage < 0.01f
                                         ? 0
                                         : std::clamp( params.lightmapScreenCoverage, 0.0f, 1.0f );
    }

    {
        const auto& params = AccessParams( drawInfo.pVolumetricParams );

        gu->volumeCameraNear = std::max( drawInfo.cameraNear, 0.001f );
        gu->volumeCameraFar  = std::min( drawInfo.cameraFar, params.volumetricFar );

        {
            if( params.enable )
            {
                gu->volumeEnableType =
                    params.useSimpleDepthBased ? VOLUME_ENABLE_SIMPLE : VOLUME_ENABLE_VOLUMETRIC;
            }
            else
            {
                gu->volumeEnableType = VOLUME_ENABLE_NONE;
            }
            gu->volumeScattering = params.scaterring;
            gu->volumeAsymmetry  = std::clamp( params.assymetry, -1.0f, 1.0f );

            RG_SET_VEC3_A( gu->volumeAmbient, params.ambientColor.data );
            RG_MAX_VEC3( gu->volumeAmbient, 0.0f );

            gu->illumVolumeEnable = params.useIlluminationVolume;

            if( auto sunUniqueId = scene->TryGetStaticSun() )
            {
                gu->volumeLightSourceIndex = lightManager->GetLightIndexForShaders(
                    currentFrameState.GetFrameIndex(), &sunUniqueId.value() );
            }
            else
            {
                gu->volumeLightSourceIndex = lightManager->GetLightIndexForShaders(
                    currentFrameState.GetFrameIndex(), params.lightUniqueId );
            }

            RG_SET_VEC3_A( gu->volumeFallbackSrcColor, params.fallbackSourceColor.data );
            RG_MAX_VEC3( gu->volumeFallbackSrcColor, 0.0f );

            RG_SET_VEC3_A( gu->volumeFallbackSrcDirection, params.fallbackSourceDirection.data );

            gu->volumeFallbackSrcExists = Utils::TryNormalize( gu->volumeFallbackSrcDirection ) &&
                                          ( gu->volumeFallbackSrcColor[ 0 ] > 0.01f &&
                                            gu->volumeFallbackSrcColor[ 1 ] > 0.01f &&
                                            gu->volumeFallbackSrcColor[ 2 ] > 0.01f );

             gu->volumeLightMult = std::max( 0.0f, params.lightMultiplier );
        }

        if( gu->volumeEnableType != VOLUME_ENABLE_NONE )
        {
            memcpy( gu->volumeViewProj_Prev, gu->volumeViewProj, 16 * sizeof( float ) );
            memcpy( gu->volumeViewProjInv_Prev, gu->volumeViewProjInv, 16 * sizeof( float ) );

            float volumeproj[ 16 ];
            Matrix::MakeProjectionMatrix( volumeproj,
                                          aspect,
                                          drawInfo.fovYRadians,
                                          gu->volumeCameraNear,
                                          gu->volumeCameraFar );

            Matrix::Multiply( gu->volumeViewProj, gu->view, volumeproj );
            Matrix::Inverse( gu->volumeViewProjInv, gu->volumeViewProj );
        }
    }

    gu->antiFireflyEnabled = devmode ? devmode->antiFirefly : true;
}

void RTGL1::VulkanDevice::Render( VkCommandBuffer cmd, const RgDrawFrameInfo& drawInfo )
{
    // end of "Prepare for frame" label
    EndCmdLabel( cmd );


    const uint32_t frameIndex = currentFrameState.GetFrameIndex();


    sceneImportExport->TryExport( *textureManager );


    bool mipLodBiasUpdated =
        worldSamplerManager->TryChangeMipLodBias( frameIndex, renderResolution.GetMipLodBias() );
    const RgFloat2D jitter = { uniform->GetData()->jitterX, uniform->GetData()->jitterY };

    textureManager->SubmitDescriptors(
        frameIndex, AccessParams( drawInfo.pTexturesParams ), mipLodBiasUpdated );
    cubemapManager->SubmitDescriptors( frameIndex );

    lightManager->SetLightstyles( AccessParams( drawInfo.pIlluminationParams ) );
    lightManager->SubmitForFrame( cmd, frameIndex );

    // submit geometry and upload uniform after getting data from a scene
    scene->SubmitForFrame( cmd,
                           frameIndex,
                           uniform,
                           uniform->GetData()->rayCullMaskWorld,
                           allowGeometryWithSkyFlag,
                           drawInfo.disableRayTracedGeometry );


    framebuffers->PrepareForSize( renderResolution.GetResolutionState() );


    if( !drawInfo.disableRasterization )
    {
        rasterizer->SubmitForFrame( cmd, frameIndex );

        // draw rasterized sky to albedo before tracing primary rays
        if( uniform->GetData()->skyType == RG_SKY_TYPE_RASTERIZED_GEOMETRY )
        {
            RgFloat3D skyViewerPosition = AccessParams( drawInfo.pSkyParams ).skyViewerPosition;

            rasterizer->DrawSkyToCubemap( cmd, frameIndex, *textureManager, *uniform );
            rasterizer->DrawSkyToAlbedo( cmd,
                                         frameIndex,
                                         *textureManager,
                                         uniform->GetData()->view,
                                         skyViewerPosition.data,
                                         uniform->GetData()->projection,
                                         jitter,
                                         renderResolution );
        }
    }


    {
        lightGrid->Build( cmd, frameIndex, uniform, blueNoise, lightManager );

        decalManager->SubmitForFrame( cmd, frameIndex );
        portalList->SubmitForFrame( cmd, frameIndex );

        float volumetricMaxHistoryLen =
            AccessParams( drawInfo.pRenderResolutionParams ).resetUpscalerHistory
                ? 0
                : AccessParams( drawInfo.pVolumetricParams ).maxHistoryLength;

        const auto params = pathTracer->Bind( cmd,
                                              frameIndex,
                                              renderResolution.Width(),
                                              renderResolution.Height(),
                                              *scene,
                                              *uniform,
                                              *textureManager,
                                              framebuffers,
                                              restirBuffers,
                                              *blueNoise,
                                              *lightManager,
                                              *cubemapManager,
                                              *rasterizer->GetRenderCubemap(),
                                              *portalList,
                                              *volumetric );

        pathTracer->TracePrimaryRays( params );

        // draw decals on top of primary surface
        decalManager->Draw( cmd, frameIndex, uniform, framebuffers, textureManager );

        if( uniform->GetData()->reflectRefractMaxDepth > 0 )
        {
            pathTracer->TraceReflectionRefractionRays( params );
        }

        lightManager->BarrierLightGrid( cmd, frameIndex );
        pathTracer->CalculateInitialReservoirs( params );
        pathTracer->TraceDirectllumination( params );
        pathTracer->TraceIndirectllumination( params );
        pathTracer->TraceVolumetric( params );

        pathTracer->CalculateGradientsSamples( params );
        denoiser->Denoise( cmd, frameIndex, uniform );
        volumetric->ProcessScattering(
            cmd, frameIndex, *uniform, *blueNoise, *framebuffers, volumetricMaxHistoryLen );
        tonemapping->CalculateExposure( cmd, frameIndex, uniform );
    }

    imageComposition->PrepareForRaster( cmd, frameIndex, uniform.get() );
    volumetric->BarrierToReadIllumination( cmd );

    if( !drawInfo.disableRasterization )
    {
        // draw rasterized geometry into the final image
        rasterizer->DrawToFinalImage( cmd,
                                      frameIndex,
                                      *textureManager,
                                      *uniform,
                                      *tonemapping,
                                      *volumetric,
                                      uniform->GetData()->view,
                                      uniform->GetData()->projection,
                                      jitter,
                                      renderResolution );
    }

    imageComposition->Finalize( cmd,
                                frameIndex,
                                *uniform,
                                *tonemapping,
                                AccessParams( drawInfo.pTonemappingParams ) );


    bool enableBloom = AccessParams( drawInfo.pBloomParams ).bloomIntensity > 0.0f;
    if( enableBloom )
    {
        bloom->Prepare( cmd, frameIndex, *uniform, *tonemapping );
    }


    FramebufferImageIndex accum = FramebufferImageIndex::FB_IMAGE_INDEX_FINAL;
    {
        // upscale finalized image
        if( renderResolution.IsNvDlssEnabled() )
        {
            accum = nvDlss->Apply(
                cmd,
                frameIndex,
                framebuffers,
                renderResolution,
                jitter,
                AccessParams( drawInfo.pRenderResolutionParams ).resetUpscalerHistory );
        }
        else if( renderResolution.IsAmdFsr2Enabled() )
        {
            accum = amdFsr2->Apply(
                cmd,
                frameIndex,
                framebuffers,
                renderResolution,
                jitter,
                uniform->GetData()->timeDelta,
                drawInfo.cameraNear,
                drawInfo.cameraFar,
                drawInfo.fovYRadians,
                AccessParams( drawInfo.pRenderResolutionParams ).resetUpscalerHistory );
        }

        const RgExtent2D* pixelized =
            AccessParams( drawInfo.pRenderResolutionParams ).pPixelizedRenderSize;

        accum = framebuffers->BlitForEffects(
            cmd, frameIndex, accum, renderResolution.GetBlitFilter(), pixelized );
    }


    const CommonnlyUsedEffectArguments args = { cmd,
                                                frameIndex,
                                                framebuffers,
                                                uniform,
                                                renderResolution.UpscaledWidth(),
                                                renderResolution.UpscaledHeight(),
                                                ( float )currentFrameTime };
    {
        if( renderResolution.IsDedicatedSharpeningEnabled() )
        {
            accum = sharpening->Apply( cmd,
                                       frameIndex,
                                       framebuffers,
                                       renderResolution.UpscaledWidth(),
                                       renderResolution.UpscaledHeight(),
                                       accum,
                                       renderResolution.GetSharpeningTechnique(),
                                       renderResolution.GetSharpeningIntensity() );
        }
        if( enableBloom )
        {
            accum = bloom->Apply( cmd,
                                  frameIndex,
                                  *uniform,
                                  *textureManager,
                                  renderResolution.UpscaledWidth(),
                                  renderResolution.UpscaledHeight(),
                                  accum );
        }
        if( effectColorTint->Setup( args, drawInfo.postEffectParams.pColorTint ) )
        {
            accum = effectColorTint->Apply( args, accum );
        }
        if( effectInverseBW->Setup( args, drawInfo.postEffectParams.pInverseBlackAndWhite ) )
        {
            accum = effectInverseBW->Apply( args, accum );
        }
        if( effectHueShift->Setup( args, drawInfo.postEffectParams.pHueShift ) )
        {
            accum = effectHueShift->Apply( args, accum );
        }
        if( effectChromaticAberration->Setup( args,
                                              drawInfo.postEffectParams.pChromaticAberration ) )
        {
            accum = effectChromaticAberration->Apply( args, accum );
        }
        if( effectDistortedSides->Setup( args, drawInfo.postEffectParams.pDistortedSides ) )
        {
            accum = effectDistortedSides->Apply( args, accum );
        }
        if( effectWaves->Setup( args, drawInfo.postEffectParams.pWaves ) )
        {
            accum = effectWaves->Apply( args, accum );
        }
        if( effectRadialBlur->Setup( args, drawInfo.postEffectParams.pRadialBlur ) )
        {
            accum = effectRadialBlur->Apply( args, accum );
        }
    }

    // draw geometry such as HUD into an upscaled framebuf
    if( !drawInfo.disableRasterization )
    {
        rasterizer->DrawToSwapchain( cmd,
                                     frameIndex,
                                     accum,
                                     *textureManager,
                                     uniform->GetData()->view,
                                     uniform->GetData()->projection,
                                     renderResolution.UpscaledWidth(),
                                     renderResolution.UpscaledHeight() );
    }

    // post-effect that work on swapchain geometry too
    {
        if( effectWipe->Setup( args, drawInfo.postEffectParams.pWipe, swapchain, frameId ) )
        {
            accum = effectWipe->Apply( args, blueNoise, accum );
        }
        if( drawInfo.postEffectParams.pCRT != nullptr && drawInfo.postEffectParams.pCRT->isActive )
        {
            effectCrtDemodulateEncode->Setup( args );
            accum = effectCrtDemodulateEncode->Apply( args, accum );

            effectCrtDecode->Setup( args );
            accum = effectCrtDecode->Apply( args, accum );
        }
    }

    // blit result image to present on a surface
    framebuffers->PresentToSwapchain( cmd, frameIndex, swapchain, accum, VK_FILTER_NEAREST );

    if( debugWindows )
    {
        debugWindows->SubmitForFrame( cmd, frameIndex );
    }
}

void RTGL1::VulkanDevice::EndFrame( VkCommandBuffer cmd )
{
    uint32_t frameIndex     = currentFrameState.GetFrameIndex();
    uint32_t swapchainCount = debugWindows && !debugWindows->IsMinimized() ? 2 : 1;

    VkSwapchainKHR swapchains[] = {
        swapchain->GetHandle(),
        debugWindows ? debugWindows->GetSwapchainHandle() : VK_NULL_HANDLE,
    };
    uint32_t swapchainIndices[] = {
        swapchain->GetCurrentImageIndex(),
        debugWindows ? debugWindows->GetSwapchainCurrentImageIndex() : 0,
    };
    VkSemaphore semaphoresToWait[] = {
        currentFrameState.GetSemaphoreForWaitAndRemove(),
        debugWindows ? debugWindows->GetSwapchainImageAvailableSemaphore( frameIndex )
                     : VK_NULL_HANDLE,
    };
    VkPipelineStageFlags stagesToWait[] = {
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    };
    VkResult results[ 2 ] = {};

    // submit command buffer, but wait until presentation engine has completed using image
    cmdManager->Submit( cmd,
                        semaphoresToWait,
                        stagesToWait,
                        swapchainCount,
                        renderFinishedSemaphores[ frameIndex ],
                        frameFences[ frameIndex ] );

    // present to surfaces after finishing the rendering
    VkPresentInfoKHR presentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &renderFinishedSemaphores[ frameIndex ],
        .swapchainCount     = swapchainCount,
        .pSwapchains        = swapchains,
        .pImageIndices      = swapchainIndices,
        .pResults           = results,
    };

    VkResult r = vkQueuePresentKHR( queues->GetGraphics(), &presentInfo );

    swapchain->OnQueuePresent( results[ 0 ] );
    if( debugWindows )
    {
        debugWindows->OnQueuePresent( results[ 1 ] );
    }

    frameId++;
}



// Interface implementation



void RTGL1::VulkanDevice::StartFrame( const RgStartFrameInfo* pInfo )
{
    if( currentFrameState.WasFrameStarted() )
    {
        throw RgException( RG_RESULT_FRAME_WASNT_ENDED );
    }

    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    VkCommandBuffer newFrameCmd = BeginFrame( *pInfo );
    currentFrameState.OnBeginFrame( newFrameCmd );
}

void RTGL1::VulkanDevice::DrawFrame( const RgDrawFrameInfo* pInfo )
{
    if( !currentFrameState.WasFrameStarted() )
    {
        throw RgException( RG_RESULT_FRAME_WASNT_STARTED );
    }

    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    const RgDrawFrameInfo& info = Dev_Override( *pInfo );

    VkCommandBuffer cmd = currentFrameState.GetCmdBuffer();

    previousFrameTime = currentFrameTime;
    currentFrameTime  = info.currentTime;

    renderResolution.Setup( AccessParams( info.pRenderResolutionParams ),
                            swapchain->GetWidth(),
                            swapchain->GetHeight(),
                            nvDlss );

    if( observer )
    {
        observer->RecheckFiles();
    }

    if( renderResolution.Width() > 0 && renderResolution.Height() > 0 )
    {
        FillUniform( uniform->GetData(), info );
        Dev_Draw();
        Render( cmd, info );
    }

    EndFrame( cmd );
    currentFrameState.OnEndFrame();

    // process in next frame
    vsync = info.vsync;
}

namespace RTGL1
{
namespace
{
    bool IsRasterized( const RgMeshInfo& mesh, const RgMeshPrimitiveInfo& primitive )
    {
        if( primitive.flags & RG_MESH_PRIMITIVE_SKY )
        {
            return true;
        }

        if( !( primitive.flags & RG_MESH_PRIMITIVE_GLASS ) &&
            !( primitive.flags & RG_MESH_PRIMITIVE_WATER ) )
        {
            if( primitive.flags & RG_MESH_PRIMITIVE_TRANSLUCENT )
            {
                return true;
            }

            if( Utils::UnpackAlphaFromPacked32( primitive.color ) <
                MESH_TRANSLUCENT_ALPHA_THRESHOLD )
            {
                return true;
            }
        }

        return false;
    }
}
}

void RTGL1::VulkanDevice::UploadMeshPrimitive( const RgMeshInfo*          pMesh,
                                               const RgMeshPrimitiveInfo* pPrimitive )
{
    if( pMesh == nullptr || pPrimitive == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    if( pPrimitive->vertexCount == 0 || pPrimitive->pVertices == nullptr )
    {
        return;
    }
    Dev_TryBreak( pPrimitive->pTextureName, false );


    // copy to modify
    RgMeshPrimitiveInfo prim       = *pPrimitive;
    RgEditorInfo        primEditor = prim.pEditorInfo ? *prim.pEditorInfo : RgEditorInfo{};
    prim.pEditorInfo               = &primEditor;
    textureMetaManager->Modify( prim, primEditor, false );


    if( primEditor.attachedLightExists )
    {
        primEditor.attachedLight.intensity = Utils::IntensityFromNonMetric(
            primEditor.attachedLight.intensity, sceneImportExport->GetWorldScale() );
    }


    if( IsRasterized( *pMesh, prim ) )
    {
        rasterizer->Upload( currentFrameState.GetFrameIndex(),
                            prim.flags & RG_MESH_PRIMITIVE_SKY ? GeometryRasterType::SKY
                                                               : GeometryRasterType::WORLD,
                            pMesh->transform,
                            prim,
                            nullptr,
                            nullptr );

        if( devmode && devmode->primitivesTableMode == Devmode::DebugPrimMode::Rasterized )
        {
            devmode->primitivesTable.push_back( Devmode::DebugPrim{
                .result         = UploadResult::Dynamic,
                .callIndex      = uint32_t( devmode->primitivesTable.size() ),
                .objectId       = pMesh->uniqueObjectID,
                .meshName       = Utils::SafeCstr( pMesh->pMeshName ),
                .primitiveIndex = prim.primitiveIndexInMesh,
                .primitiveName  = Utils::SafeCstr( prim.pPrimitiveNameInMesh ),
                .textureName    = Utils::SafeCstr( prim.pTextureName ),
            } );
        }
    }
    else
    {
        UploadResult r = scene->UploadPrimitive(
            currentFrameState.GetFrameIndex(), *pMesh, prim, *textureManager, false );

        if( devmode && devmode->primitivesTableMode == Devmode::DebugPrimMode::RayTraced )
        {
            devmode->primitivesTable.push_back( Devmode::DebugPrim{
                .result         = r,
                .callIndex      = uint32_t( devmode->primitivesTable.size() ),
                .objectId       = pMesh->uniqueObjectID,
                .meshName       = Utils::SafeCstr( pMesh->pMeshName ),
                .primitiveIndex = prim.primitiveIndexInMesh,
                .primitiveName  = Utils::SafeCstr( prim.pPrimitiveNameInMesh ),
                .textureName    = Utils::SafeCstr( prim.pTextureName ),
            } );
        }

        if( r == UploadResult::ExportableDynamic || r == UploadResult::ExportableStatic )
        {
            if( auto e = sceneImportExport->TryGetExporter() )
            {
                e->AddPrimitive( *pMesh, prim );
            }
        }
    }
}

void RTGL1::VulkanDevice::UploadNonWorldPrimitive( const RgMeshPrimitiveInfo* pPrimitive,
                                                   const float*               pViewProjection,
                                                   const RgViewport*          pViewport )
{
    if( pPrimitive == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }
    Dev_TryBreak( pPrimitive->pTextureName, false );

    rasterizer->Upload( currentFrameState.GetFrameIndex(),
                        GeometryRasterType::SWAPCHAIN,
                        RG_TRANSFORM_IDENTITY,
                        *pPrimitive,
                        pViewProjection,
                        pViewport );

    if( devmode && devmode->primitivesTableMode == Devmode::DebugPrimMode::NonWorld )
    {
        devmode->primitivesTable.push_back( Devmode::DebugPrim{
            .result         = UploadResult::Dynamic,
            .callIndex      = uint32_t( devmode->primitivesTable.size() ),
            .objectId       = 0,
            .meshName       = {},
            .primitiveIndex = pPrimitive->primitiveIndexInMesh,
            .primitiveName  = Utils::SafeCstr( pPrimitive->pPrimitiveNameInMesh ),
            .textureName    = Utils::SafeCstr( pPrimitive->pTextureName ),
        } );
    }
}

void RTGL1::VulkanDevice::UploadDecal( const RgDecalUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }
    Dev_TryBreak( pInfo->pTextureName, false );

    decalManager->Upload( currentFrameState.GetFrameIndex(), *pInfo, textureManager );

    if( devmode && devmode->primitivesTableMode == Devmode::DebugPrimMode::Decal )
    {
        devmode->primitivesTable.push_back( Devmode::DebugPrim{
            .result         = UploadResult::Dynamic,
            .callIndex      = uint32_t( devmode->primitivesTable.size() ),
            .objectId       = 0,
            .meshName       = {},
            .primitiveIndex = 0,
            .primitiveName  = {},
            .textureName    = Utils::SafeCstr( pInfo->pTextureName ),
        } );
    }
}

void RTGL1::VulkanDevice::UploadLensFlare( const RgLensFlareUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    rasterizer->UploadLensFlare( currentFrameState.GetFrameIndex(), *pInfo, *textureManager );
}

void RTGL1::VulkanDevice::UploadLight( const GenericLightPtr& light )
{
    UploadResult r =
        scene->UploadLight( currentFrameState.GetFrameIndex(), light, lightManager.get(), false );

    if( r == UploadResult::ExportableDynamic || r == UploadResult::ExportableStatic )
    {
        if( auto e = sceneImportExport->TryGetExporter() )
        {
            e->AddLight( light );
        }
    }
}

void RTGL1::VulkanDevice::UploadDirectionalLight( const RgDirectionalLightUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    UploadLight( pInfo );
}

void RTGL1::VulkanDevice::UploadSphericalLight( const RgSphericalLightUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    const_cast< RgSphericalLightUploadInfo* >( pInfo )->intensity =
        Utils::IntensityFromNonMetric( pInfo->intensity, sceneImportExport->GetWorldScale() );

    UploadLight( pInfo );
}

void RTGL1::VulkanDevice::UploadSpotlight( const RgSpotLightUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    const_cast< RgSpotLightUploadInfo* >( pInfo )->intensity =
        Utils::IntensityFromNonMetric( pInfo->intensity, sceneImportExport->GetWorldScale() );

    UploadLight( pInfo );
}

void RTGL1::VulkanDevice::UploadPolygonalLight( const RgPolygonalLightUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    const_cast< RgPolygonalLightUploadInfo* >( pInfo )->intensity =
        Utils::IntensityFromNonMetric( pInfo->intensity, sceneImportExport->GetWorldScale() );

    UploadLight( pInfo );
}

void RTGL1::VulkanDevice::ProvideOriginalTexture( const RgOriginalTextureInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }
    Dev_TryBreak( pInfo->pTextureName, true );

    textureManager->TryCreateMaterial( currentFrameState.GetCmdBufferForMaterials( cmdManager ),
                                       currentFrameState.GetFrameIndex(),
                                       *pInfo,
                                       libconfig.developerMode ? ovrdFolder / TEXTURES_FOLDER_DEV
                                                               : ovrdFolder / TEXTURES_FOLDER );
}

void RTGL1::VulkanDevice::ProvideOriginalCubemapTexture( const RgOriginalCubemapInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }
    Dev_TryBreak( pInfo->pTextureName, true );

    cubemapManager->TryCreateCubemap( currentFrameState.GetCmdBufferForMaterials( cmdManager ),
                                      currentFrameState.GetFrameIndex(),
                                      *pInfo,
                                      libconfig.developerMode ? ovrdFolder / TEXTURES_FOLDER_DEV
                                                              : ovrdFolder / TEXTURES_FOLDER );
}

void RTGL1::VulkanDevice::MarkOriginalTextureAsDeleted( const char* pTextureName )
{
    textureManager->TryDestroyMaterial( currentFrameState.GetFrameIndex(), pTextureName );
    cubemapManager->TryDestroyCubemap( currentFrameState.GetFrameIndex(), pTextureName );
}

bool RTGL1::VulkanDevice::IsSuspended() const
{
    if( !swapchain )
    {
        return false;
    }

    if( currentFrameState.WasFrameStarted() )
    {
        return false;
    }

    return !swapchain->IsExtentOptimal();
}

bool RTGL1::VulkanDevice::IsUpscaleTechniqueAvailable( RgRenderUpscaleTechnique technique ) const
{
    switch( technique )
    {
        case RG_RENDER_UPSCALE_TECHNIQUE_NEAREST:
        case RG_RENDER_UPSCALE_TECHNIQUE_LINEAR:
        case RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2: return true;

        case RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS: return nvDlss->IsDlssAvailable();

        default:
            throw RgException(
                RG_RESULT_WRONG_FUNCTION_ARGUMENT,
                "Incorrect technique was passed to rgIsRenderUpscaleTechniqueAvailable" );
    }
}

RgPrimitiveVertex* RTGL1::VulkanDevice::ScratchAllocForVertices( uint32_t vertexCount )
{
    // TODO: scratch allocator
    return new RgPrimitiveVertex[ vertexCount ];
}

void RTGL1::VulkanDevice::ScratchFree( const RgPrimitiveVertex* pPointer )
{
    // TODO: scratch allocator
    delete[] pPointer;
}

void RTGL1::VulkanDevice::Print( std::string_view msg, RgMessageSeverityFlags severity ) const
{
    if( devmode )
    {
        devmode->logs.emplace_back( severity, msg, std::hash< std::string_view >{}( msg ) );
    }

    if( userPrint )
    {
        userPrint->Print( msg.data(), severity );
    }
}
