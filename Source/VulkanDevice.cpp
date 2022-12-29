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

#include <imgui.h>

#include <algorithm>
#include <cstring>

VkCommandBuffer RTGL1::VulkanDevice::BeginFrame( const char* pMapName )
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


    if( debugData.reloadShaders )
    {
        shaderManager->ReloadShaders();
        debugData.reloadShaders = false;
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
    debugData.primitivesTable.clear();
    debugData.nonworldTable.clear();

    VkCommandBuffer cmd = cmdManager->StartGraphicsCmd();
    BeginCmdLabel( cmd, "Prepare for frame" );

    textureManager->TryHotReload( cmd, frameIndex );
    lightManager->PrepareForFrame( cmd, frameIndex );
    scene->PrepareForFrame( cmd, frameIndex );

    {
        sceneImportExport->CheckForNewScene( Utils::SafeCstr( pMapName ),
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
        gu->stopEyeAdaptation = drawInfo.disableEyeAdaptation;

        if( drawInfo.pTonemappingParams != nullptr )
        {
            gu->minLogLuminance     = drawInfo.pTonemappingParams->minLogLuminance;
            gu->maxLogLuminance     = drawInfo.pTonemappingParams->maxLogLuminance;
            gu->luminanceWhitePoint = drawInfo.pTonemappingParams->luminanceWhitePoint;
        }
        else
        {
            gu->minLogLuminance     = -3;
            gu->maxLogLuminance     = 10;
            gu->luminanceWhitePoint = 10.0f;
        }
    }

    {
        gu->lightCount     = lightManager->GetLightCount();
        gu->lightCountPrev = lightManager->GetLightCountPrev();

        gu->directionalLightExists = lightManager->DoesDirectionalLightExist();
    }

    {
        static_assert( sizeof( gu->skyCubemapRotationTransform ) == sizeof( IdentityMat4x4 ) &&
                           sizeof( IdentityMat4x4 ) == 16 * sizeof( float ),
                       "Recheck skyCubemapRotationTransform sizes" );
        memcpy( gu->skyCubemapRotationTransform, IdentityMat4x4, 16 * sizeof( float ) );

        if( drawInfo.pSkyParams != nullptr )
        {
            const auto& sp = *drawInfo.pSkyParams;

            RG_SET_VEC3_A( gu->skyColorDefault, sp.skyColorDefault.data );
            gu->skyColorMultiplier = sp.skyColorMultiplier;
            gu->skyColorSaturation = std::max( sp.skyColorSaturation, 0.0f );

            gu->skyType = sp.skyType == RG_SKY_TYPE_CUBEMAP ? SKY_TYPE_CUBEMAP
                          : sp.skyType == RG_SKY_TYPE_RASTERIZED_GEOMETRY
                              ? SKY_TYPE_RASTERIZED_GEOMETRY
                              : SKY_TYPE_COLOR;

            gu->skyCubemapIndex =
                sp.pSkyCubemapTextureName
                    ? cubemapManager->TryGetDescriptorIndex( sp.pSkyCubemapTextureName )
                    : MATERIAL_NO_TEXTURE;

            if( !Utils::IsAlmostZero( drawInfo.pSkyParams->skyCubemapRotationTransform ) )
            {
                Utils::SetMatrix3ToGLSLMat4( gu->skyCubemapRotationTransform,
                                             drawInfo.pSkyParams->skyCubemapRotationTransform );
            }
        }
        else
        {
            RG_SET_VEC3( gu->skyColorDefault, 1.0f, 1.0f, 1.0f );
            gu->skyColorMultiplier = 1.0f;
            gu->skyColorSaturation = 1.0f;
            gu->skyType            = SKY_TYPE_COLOR;
            gu->skyCubemapIndex    = MATERIAL_NO_TEXTURE;
        }

        RgFloat3D skyViewerPosition =
            drawInfo.pSkyParams ? drawInfo.pSkyParams->skyViewerPosition : RgFloat3D{ 0, 0, 0 };

        for( uint32_t i = 0; i < 6; i++ )
        {
            float* viewProjDst = &gu->viewProjCubemap[ 16 * i ];

            Matrix::GetCubemapViewProjMat(
                viewProjDst, i, skyViewerPosition.data, drawInfo.cameraNear, drawInfo.cameraFar );
        }
    }

    gu->debugShowFlags = debugData.debugShowFlags;

    if( drawInfo.pTexturesParams != nullptr )
    {
        gu->normalMapStrength = drawInfo.pTexturesParams->normalMapStrength;
        gu->emissionMapBoost  = std::max( drawInfo.pTexturesParams->emissionMapBoost, 0.0f );
        gu->emissionMaxScreenColor =
            std::max( drawInfo.pTexturesParams->emissionMaxScreenColor, 0.0f );
        gu->minRoughness = std::clamp( drawInfo.pTexturesParams->minRoughness, 0.0f, 1.0f );
    }
    else
    {
        gu->normalMapStrength      = 1.0f;
        gu->emissionMapBoost       = 100.0f;
        gu->emissionMaxScreenColor = 1.5f;
        gu->minRoughness           = 0.0f;
    }

    if( drawInfo.pIlluminationParams != nullptr )
    {
        gu->maxBounceShadowsLights = drawInfo.pIlluminationParams->maxBounceShadows;
        gu->polyLightSpotlightFactor =
            std::max( 0.0f, drawInfo.pIlluminationParams->polygonalLightSpotlightFactor );
        gu->indirSecondBounce = !!drawInfo.pIlluminationParams->enableSecondBounceForIndirect;
        gu->lightIndexIgnoreFPVShadows = lightManager->GetLightIndexIgnoreFPVShadows(
            currentFrameState.GetFrameIndex(),
            drawInfo.pIlluminationParams->lightUniqueIdIgnoreFirstPersonViewerShadows );
        gu->cellWorldSize       = std::max( drawInfo.pIlluminationParams->cellWorldSize, 0.001f );
        gu->gradientMultDiffuse = std::clamp(
            drawInfo.pIlluminationParams->directDiffuseSensitivityToChange, 0.0f, 1.0f );
        gu->gradientMultIndirect = std::clamp(
            drawInfo.pIlluminationParams->indirectDiffuseSensitivityToChange, 0.0f, 1.0f );
        gu->gradientMultSpecular =
            std::clamp( drawInfo.pIlluminationParams->specularSensitivityToChange, 0.0f, 1.0f );
    }
    else
    {
        gu->maxBounceShadowsLights     = 2;
        gu->polyLightSpotlightFactor   = 2.0f;
        gu->indirSecondBounce          = true;
        gu->lightIndexIgnoreFPVShadows = LIGHT_INDEX_NONE;
        gu->cellWorldSize              = 1.0f;
        gu->gradientMultDiffuse        = 0.5f;
        gu->gradientMultIndirect       = 0.2f;
        gu->gradientMultSpecular       = 0.5f;
    }

    if( drawInfo.pBloomParams != nullptr )
    {
        gu->bloomThreshold = std::max( drawInfo.pBloomParams->inputThreshold, 0.0f );
        gu->bloomIntensity = std::max( drawInfo.pBloomParams->bloomIntensity, 0.0f );
        gu->bloomEmissionMultiplier =
            std::max( drawInfo.pBloomParams->bloomEmissionMultiplier, 0.0f );
    }
    else
    {
        gu->bloomThreshold          = 4.0f;
        gu->bloomIntensity          = 1.0f;
        gu->bloomEmissionMultiplier = 16.0f;
    }

    static_assert(
        RG_MEDIA_TYPE_VACUUM == MEDIA_TYPE_VACUUM && RG_MEDIA_TYPE_WATER == MEDIA_TYPE_WATER &&
            RG_MEDIA_TYPE_GLASS == MEDIA_TYPE_GLASS && RG_MEDIA_TYPE_ACID == MEDIA_TYPE_ACID,
        "Interface and GLSL constants must be identical" );

    if( drawInfo.pReflectRefractParams != nullptr )
    {
        const auto& rr = *drawInfo.pReflectRefractParams;

        if( rr.typeOfMediaAroundCamera >= 0 && rr.typeOfMediaAroundCamera < MEDIA_TYPE_COUNT )
        {
            gu->cameraMediaType = rr.typeOfMediaAroundCamera;
        }
        else
        {
            gu->cameraMediaType = MEDIA_TYPE_VACUUM;
        }

        gu->reflectRefractMaxDepth = std::min( 4u, rr.maxReflectRefractDepth );

        gu->indexOfRefractionGlass = std::max( 0.0f, rr.indexOfRefractionGlass );
        gu->indexOfRefractionWater = std::max( 0.0f, rr.indexOfRefractionWater );

        memcpy( gu->waterColorAndDensity, rr.waterColor.data, 3 * sizeof( float ) );
        gu->waterColorAndDensity[ 3 ] = 0.0f;

        memcpy( gu->acidColorAndDensity, rr.acidColor.data, 3 * sizeof( float ) );
        gu->acidColorAndDensity[ 3 ] = rr.acidDensity;

        gu->forceNoWaterRefraction = !!rr.forceNoWaterRefraction;
        gu->waterWaveSpeed         = rr.waterWaveSpeed;
        gu->waterWaveStrength      = rr.waterWaveNormalStrength;
        gu->waterTextureDerivativesMultiplier =
            std::max( 0.0f, rr.waterWaveTextureDerivativesMultiplier );
        if( rr.waterTextureAreaScale < 0.0001f )
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
        gu->cameraMediaType        = MEDIA_TYPE_VACUUM;
        gu->reflectRefractMaxDepth = 2;

        gu->indexOfRefractionGlass = 1.52f;
        gu->indexOfRefractionWater = 1.33f;

        RG_SET_VEC3( gu->waterColorAndDensity, 0.3f, 0.73f, 0.63f );
        gu->waterColorAndDensity[ 3 ] = 0.0f;

        RG_SET_VEC3( gu->acidColorAndDensity, 0.0f, 0.66f, 0.55f );
        gu->acidColorAndDensity[ 3 ] = 10.0f;

        gu->forceNoWaterRefraction            = false;
        gu->waterWaveSpeed                    = 1.0f;
        gu->waterWaveStrength                 = 1.0f;
        gu->waterTextureDerivativesMultiplier = 1.0f;
        gu->waterTextureAreaScale             = 1.0f;

        gu->noBackfaceReflForNoMediaChange = false;

        gu->twirlPortalNormal = false;
    }

    gu->rayCullBackFaces  = rayCullBackFacingTriangles ? 1 : 0;
    gu->rayLength         = clamp( drawInfo.rayLength, 0.1f, ( float )MAX_RAY_LENGTH );
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

    gu->cameraRayConeSpreadAngle = atanf( ( 2.0f * tanf( drawInfo.fovYRadians * 0.5f ) ) /
                                          ( float )renderResolution.Height() );

    RG_SET_VEC3_A( gu->worldUpVector, sceneImportExport->GetWorldUp().data );

    if( drawInfo.pLightmapParams != nullptr )
    {
        gu->lightmapEnable = !!drawInfo.pLightmapParams->enableLightmaps;

        if( drawInfo.pLightmapParams->lightmapLayerIndex == 1 ||
            drawInfo.pLightmapParams->lightmapLayerIndex == 2 )
        {
            gu->lightmapLayer = drawInfo.pLightmapParams->lightmapLayerIndex;
        }
        else
        {
            assert( 0 &&
                    "pLightMapLayerIndex must point to a value of 1 or 2. Others are invalidated" );
        }
    }
    else
    {
        gu->lightmapEnable = false;
        gu->lightmapLayer  = UINT8_MAX;
    }

    gu->lensFlareCullingInputCount = 0;
    gu->applyViewProjToLensFlares  = false;

    {
        gu->volumeCameraNear = std::max( drawInfo.cameraNear, 0.001f );
        gu->volumeCameraFar  = std::min(
            drawInfo.cameraFar,
            drawInfo.pVolumetricParams ? drawInfo.pVolumetricParams->volumetricFar : 100.0f );

        if( drawInfo.pVolumetricParams )
        {
            if( drawInfo.pVolumetricParams->enable )
            {
                gu->volumeEnableType = drawInfo.pVolumetricParams->useSimpleDepthBased
                                           ? VOLUME_ENABLE_SIMPLE
                                           : VOLUME_ENABLE_VOLUMETRIC;
            }
            else
            {
                gu->volumeEnableType = VOLUME_ENABLE_NONE;
            }
            gu->volumeScattering = drawInfo.pVolumetricParams->scaterring;
            gu->volumeSourceAsymmetry =
                std::clamp( drawInfo.pVolumetricParams->sourceAssymetry, -1.0f, 1.0f );

            RG_SET_VEC3_A( gu->volumeAmbient, drawInfo.pVolumetricParams->ambientColor.data );
            RG_MAX_VEC3( gu->volumeAmbient, 0.0f );

            RG_SET_VEC3_A( gu->volumeSourceColor, drawInfo.pVolumetricParams->sourceColor.data );
            RG_MAX_VEC3( gu->volumeSourceColor, 0.0f );

            RG_SET_VEC3_A( gu->volumeDirToSource,
                           drawInfo.pVolumetricParams->sourceDirection.data );
            Utils::Negate( gu->volumeDirToSource );
            Utils::Normalize( gu->volumeDirToSource );
        }
        else
        {
            gu->volumeEnableType      = VOLUME_ENABLE_VOLUMETRIC;
            gu->volumeScattering      = 0.2f;
            gu->volumeSourceAsymmetry = 0.4f;
            RG_SET_VEC3( gu->volumeAmbient, 0.8f, 0.85f, 1.0f );
            RG_SET_VEC3( gu->volumeSourceColor, 0, 0, 0 );
            RG_SET_VEC3( gu->volumeDirToSource, 0, 1, 0 );
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

    gu->antiFireflyEnabled = !!drawInfo.forceAntiFirefly;
}

void RTGL1::VulkanDevice::DrawDebugWindows() const
{
    if( !debugWindows )
    {
        return;
    }

    if( ImGui::Begin( "General" ) )
    {
        ImGui::Checkbox( "Always on top", &debugData.debugWindowOnTop );
        debugWindows->SetAlwaysOnTop( debugData.debugWindowOnTop );

        ImGui::Text( "%.3f ms/frame (%.1f FPS)",
                     1000.0f / ImGui::GetIO().Framerate,
                     ImGui::GetIO().Framerate );
    }
    ImGui::End();

    if( ImGui::Begin( "Primitives", nullptr, ImGuiWindowFlags_HorizontalScrollbar ) )
    {
        ImGui::RadioButton( "Disable", &debugData.primitivesTableEnable, 0 );
        ImGui::SameLine();
        ImGui::RadioButton( "Record rasterized", &debugData.primitivesTableEnable, 1 );
        ImGui::SameLine();
        ImGui::RadioButton( "Record ray-traced", &debugData.primitivesTableEnable, 2 );

        ImGui::TextUnformatted(
            "Red    - if exportable, but not found in GLTF, so uploading as dynamic" );
        ImGui::TextUnformatted( "Green  - if exportable was found in GLTF" );

        if( ImGui::BeginTable( "Primitives table",
                               6,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders ) )
        {
            {
                ImGui::TableSetupColumn( "Call" );
                ImGui::TableSetupColumn( "Object ID" );
                ImGui::TableSetupColumn( "Mesh name" );
                ImGui::TableSetupColumn( "Primitive index" );
                ImGui::TableSetupColumn( "Primitive name" );
                ImGui::TableSetupColumn( "Texture" );
                ImGui::TableHeadersRow();
            }

            if( ImGuiTableSortSpecs* sortspecs = ImGui::TableGetSortSpecs() )
            {
                sortspecs->SpecsDirty = true;

                std::ranges::sort(
                    debugData.primitivesTable,
                    [ sortspecs ]( const DebugPrim& a, const DebugPrim& b ) -> bool {
                        for( int n = 0; n < sortspecs->SpecsCount; n++ )
                        {
                            const ImGuiTableColumnSortSpecs* srt = &sortspecs->Specs[ n ];

                            std::strong_ordering ord{ 0 };
                            switch( srt->ColumnIndex )
                            {
                                case 0: ord = ( a.callIndex <=> b.callIndex ); break;
                                case 1: ord = ( a.objectId <=> b.objectId ); break;
                                case 2: ord = ( a.meshName <=> b.meshName ); break;
                                case 3: ord = ( a.primitiveIndex <=> b.primitiveIndex ); break;
                                case 4: ord = ( a.primitiveName <=> b.primitiveName ); break;
                                case 5: ord = ( a.textureName <=> b.textureName ); break;
                                default: assert( 0 ); return false;
                            }

                            if( std::is_gt( ord ) )
                            {
                                return srt->SortDirection != ImGuiSortDirection_Ascending;
                            }

                            if( std::is_lt( ord ) )
                            {
                                return srt->SortDirection == ImGuiSortDirection_Ascending;
                            }
                        }

                        return a.callIndex < b.callIndex;
                    } );
            }

            ImGuiListClipper clipper;
            clipper.Begin( int( debugData.primitivesTable.size() ) );
            while( clipper.Step() )
            {
                for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ )
                {
                    const auto& prim = debugData.primitivesTable[ i ];
                    ImGui::TableNextRow();

                    if( prim.result == UploadResult::ExportableStatic )
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0,
                                                IM_COL32( 0, 128, 0, 64 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1,
                                                IM_COL32( 0, 128, 0, 128 ) );
                    }
                    else if( prim.result == UploadResult::ExportableDynamic )
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0,
                                                IM_COL32( 128, 0, 0, 64 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1,
                                                IM_COL32( 128, 0, 0, 128 ) );
                    }
                    else
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, IM_COL32( 0, 0, 0, 1 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, IM_COL32( 0, 0, 0, 1 ) );
                    }


                    ImGui::TableNextColumn();
                    if( prim.result != UploadResult::Fail )
                    {
                        ImGui::Text( "%u", prim.callIndex );
                    }
                    else
                    {
                        ImGui::TextUnformatted( "fail" );
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text( "%u", prim.objectId );
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( prim.meshName.c_str() );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%u", prim.primitiveIndex );
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( prim.primitiveName.c_str() );
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( prim.textureName.c_str() );
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();

    if( ImGui::Begin( "Non-world Primitives", nullptr, ImGuiWindowFlags_HorizontalScrollbar ) )
    {
        ImGui::Checkbox( "Record", &debugData.nonworldTableEnable );

        if( ImGui::BeginTable( "Non-world Primitives table",
                               2,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders ) )
        {
            {
                ImGui::TableSetupColumn( "Call" );
                ImGui::TableSetupColumn( "Texture" );
                ImGui::TableHeadersRow();
            }

            for( const auto& prim : debugData.nonworldTable )
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text( "%u", prim.callIndex );
                ImGui::TableNextColumn();
                ImGui::TextUnformatted( prim.textureName.c_str() );
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();

    if( ImGui::Begin( "Log", nullptr, ImGuiWindowFlags_HorizontalScrollbar ) )
    {
        ImGui::CheckboxFlags( "Errors", &debugData.logFlags, RG_MESSAGE_SEVERITY_ERROR );
        ImGui::SameLine();
        ImGui::CheckboxFlags( "Warnings", &debugData.logFlags, RG_MESSAGE_SEVERITY_WARNING );
        ImGui::SameLine();
        ImGui::CheckboxFlags( "Info", &debugData.logFlags, RG_MESSAGE_SEVERITY_INFO );
        ImGui::SameLine();
        ImGui::CheckboxFlags( "Verbose", &debugData.logFlags, RG_MESSAGE_SEVERITY_VERBOSE );

        if( ImGui::Button( "Clear" ) )
        {
            debugData.logs.clear();
        }

        for( const auto& [ severity, msg ] : debugData.logs )
        {
            RgMessageSeverityFlags filtered = severity & debugData.logFlags;

            ImU32 color;
            if( filtered & RG_MESSAGE_SEVERITY_ERROR )
            {
                color = IM_COL32( 255, 0, 0, 255 );
            }
            else if( filtered & RG_MESSAGE_SEVERITY_WARNING )
            {
                color = IM_COL32( 255, 255, 0, 255 );
            }
            else if( filtered & RG_MESSAGE_SEVERITY_INFO )
            {
                color = IM_COL32( 255, 255, 255, 255 );
            }
            else if( filtered & RG_MESSAGE_SEVERITY_VERBOSE )
            {
                color = IM_COL32( 255, 255, 255, 255 );
            }
            else
            {
                assert( filtered == 0 );
                continue;
            }
            ImGui::PushStyleColor( ImGuiCol_Text, color );
            ImGui::TextUnformatted( msg.c_str() );
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();


    if( ImGui::Begin( "Import/Export" ) )
    {
        auto& dev = sceneImportExport->dev;
        if( !dev.exportName.enable )
        {
            dev.exportName.SetDefaults( *sceneImportExport );
        }
        if( !dev.importName.enable )
        {
            dev.importName.SetDefaults( *sceneImportExport );
        }
        if( !dev.worldTransform.enable )
        {
            dev.worldTransform.SetDefaults( *sceneImportExport );
        }

        {
            ImGui::Text( "Resource folder: %s",
                         std::filesystem::absolute( ovrdFolder ).string().c_str() );
        }
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 16 ) );
        {
            if( ImGui::Button( "Reimport GLTF", { -1, 80 } ) )
            {
                sceneImportExport->RequestReimport();
            }

            ImGui::Text( "Import path: %s",
                         sceneImportExport->MakeGltfPath( sceneImportExport->GetImportMapName() )
                             .string()
                             .c_str() );
            ImGui::BeginDisabled( !dev.importName.enable );
            {
                ImGui::InputText(
                    "Import map name", dev.importName.value, std::size( dev.importName.value ) );
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Checkbox( "Custom##import", &dev.importName.enable );
        }
        ImGui::Dummy( ImVec2( 0, 16 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 16 ) );
        {
            ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.98f, 0.59f, 0.26f, 0.40f ) );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.98f, 0.59f, 0.26f, 1.00f ) );
            ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.98f, 0.53f, 0.06f, 1.00f ) );
            if( ImGui::Button( "Export frame geometry", { -1, 80 } ) )
            {
                sceneImportExport->RequestExport();
            }
            ImGui::PopStyleColor( 3 );

            ImGui::Text( "Export path: %s",
                         sceneImportExport->MakeGltfPath( sceneImportExport->GetExportMapName() )
                             .string()
                             .c_str() );
            ImGui::BeginDisabled( !dev.exportName.enable );
            {
                ImGui::InputText(
                    "Export map name", dev.exportName.value, std::size( dev.exportName.value ) );
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Checkbox( "Custom##export", &dev.exportName.enable );
        }
        ImGui::Dummy( ImVec2( 0, 16 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 16 ) );
        {
            ImGui::Checkbox( "Custom import/export world space", &dev.worldTransform.enable );
            ImGui::BeginDisabled( !dev.worldTransform.enable );
            {
                ImGui::SliderFloat3( "World Up vector", dev.worldTransform.up.data, -1.0f, 1.0f );
                ImGui::SliderFloat3(
                    "World Forward vector", dev.worldTransform.forward.data, -1.0f, 1.0f );
                ImGui::InputFloat(
                    std::format( "1 unit = {} meters", dev.worldTransform.scale ).c_str(),
                    &dev.worldTransform.scale );
            }
            ImGui::EndDisabled();
        }
    }
    ImGui::End();


    if( ImGui::Begin( "Textures" ) )
    {
        if( ImGui::Button( "Export original textures", { -1, 80 } ) )
        {
            textureManager->ExportOriginalMaterialTextures( ovrdFolder /
                                                            TEXTURES_FOLDER_ORIGINALS );
        }
        ImGui::Text( "Export path: %s",
                     ( ovrdFolder / TEXTURES_FOLDER_ORIGINALS ).string().c_str() );
        ImGui::Dummy( ImVec2( 0, 16 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 16 ) );

        ImGui::Checkbox( "Record", &debugData.materialsTableEnable );
        ImGui::TextUnformatted( "Blue - if material is non-original (i.e. was loaded from GLTF)" );
        if( ImGui::BeginTable( "Materials table",
                               1,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders ) )
        {
            auto materialInfos = debugData.materialsTableEnable
                                     ? textureManager->Debug_GetMaterials()
                                     : std::vector< TextureManager::Debug_MaterialInfo >{};
            {
                ImGui::TableSetupColumn( "Material name" );
                ImGui::TableHeadersRow();
            }

            if( ImGuiTableSortSpecs* sortspecs = ImGui::TableGetSortSpecs() )
            {
                sortspecs->SpecsDirty = true;

                std::ranges::sort(
                    materialInfos,
                    [ sortspecs ]( const TextureManager::Debug_MaterialInfo& a,
                                   const TextureManager::Debug_MaterialInfo& b ) -> bool {
                        for( int n = 0; n < sortspecs->SpecsCount; n++ )
                        {
                            const ImGuiTableColumnSortSpecs* srt = &sortspecs->Specs[ n ];

                            std::strong_ordering ord{ 0 };
                            switch( srt->ColumnIndex )
                            {
                                case 0: ord = ( a.materialName <=> b.materialName ); break;
                                default: assert( 0 ); return false;
                            }

                            if( std::is_gt( ord ) )
                            {
                                return srt->SortDirection != ImGuiSortDirection_Ascending;
                            }

                            if( std::is_lt( ord ) )
                            {
                                return srt->SortDirection == ImGuiSortDirection_Ascending;
                            }
                        }

                        return a.materialName < b.materialName;
                    } );
            }

            ImGuiListClipper clipper;
            clipper.Begin( int( materialInfos.size() ) );
            while( clipper.Step() )
            {
                for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ )
                {
                    const auto& mat = materialInfos[ i ];
                    ImGui::TableNextRow();

                    if( mat.isOriginal )
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0,
                                                IM_COL32( 0, 0, 128, 64 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1,
                                                IM_COL32( 0, 0, 128, 128 ) );
                    }
                    else
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, IM_COL32( 0, 0, 0, 1 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, IM_COL32( 0, 0, 0, 1 ) );
                    }

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( mat.materialName.c_str() );
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
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

    textureManager->SubmitDescriptors( frameIndex, drawInfo.pTexturesParams, mipLodBiasUpdated );
    cubemapManager->SubmitDescriptors( frameIndex );

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
            RgFloat3D skyViewerPosition =
                drawInfo.pSkyParams ? drawInfo.pSkyParams->skyViewerPosition : RgFloat3D{ 0, 0, 0 };

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
        volumetric->ProcessScattering( cmd, frameIndex, uniform.get(), blueNoise.get() );
        tonemapping->CalculateExposure( cmd, frameIndex, uniform );
    }

    imageComposition->PrepareForRaster( cmd, frameIndex, uniform.get() );
    volumetric->BarrierToReadScattering( cmd, frameIndex );
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

    imageComposition->Finalize(
        cmd, frameIndex, uniform.get(), tonemapping.get(), volumetric.get() );


    bool enableBloom =
        drawInfo.pBloomParams == nullptr ||
        ( drawInfo.pBloomParams != nullptr && drawInfo.pBloomParams->bloomIntensity > 0.0f );

    if( enableBloom )
    {
        bloom->Prepare( cmd, frameIndex, uniform, tonemapping );
    }


    FramebufferImageIndex accum = FramebufferImageIndex::FB_IMAGE_INDEX_FINAL;
    {
        // upscale finalized image
        if( renderResolution.IsNvDlssEnabled() )
        {
            accum = nvDlss->Apply( cmd, frameIndex, framebuffers, renderResolution, jitter );
        }
        else if( renderResolution.IsAmdFsr2Enabled() )
        {
            accum = amdFsr2->Apply( cmd,
                                    frameIndex,
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
                                  uniform,
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
    uint32_t swapchainCount = debugWindows ? 2 : 1;

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



void RTGL1::VulkanDevice::StartFrame( const char* pMapName )
{
    if( currentFrameState.WasFrameStarted() )
    {
        throw RgException( RG_RESULT_FRAME_WASNT_ENDED );
    }

    VkCommandBuffer newFrameCmd = BeginFrame( pMapName );
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

    // override if requested
    if( debugWindows )
    {
        if( ImGui::Begin( "Frame" ) )
        {
            debugData.reloadShaders = ImGui::Button( "Reload shaders", { -1, 96 } );
            ImGui::Separator();
            if( ImGui::TreeNode( "Override" ) )
            {
                ImGui::Checkbox( "Enable", &debugData.overrideDrawInfo );
                ImGui::BeginDisabled( !debugData.overrideDrawInfo );

                ImGui::Checkbox( "Vsync", &debugData.ovrdVsync );

                ImGui::EndDisabled();
                ImGui::TreePop();
            }
            if( ImGui::TreeNode( "Debug show" ) )
            {
                std::pair< const char*, uint32_t > fs[] = {
                    { "Unfiltered diffuse direct", DEBUG_SHOW_FLAG_UNFILTERED_DIFFUSE },
                    { "Unfiltered diffuse indirect", DEBUG_SHOW_FLAG_UNFILTERED_INDIRECT },
                    { "Unfiltered specular", DEBUG_SHOW_FLAG_UNFILTERED_SPECULAR },
                    { "Diffuse direct", DEBUG_SHOW_FLAG_ONLY_DIRECT_DIFFUSE },
                    { "Diffuse indirect", DEBUG_SHOW_FLAG_ONLY_INDIRECT_DIFFUSE },
                    { "Specular", DEBUG_SHOW_FLAG_ONLY_SPECULAR },
                    { "Albedo white", DEBUG_SHOW_FLAG_ALBEDO_WHITE },
                    { "Motion vectors", DEBUG_SHOW_FLAG_MOTION_VECTORS },
                    { "Gradients", DEBUG_SHOW_FLAG_GRADIENTS },
                    { "Light grid", DEBUG_SHOW_FLAG_LIGHT_GRID },
                };
                for( const auto [ name, f ] : fs )
                {
                    ImGui::CheckboxFlags( name, &debugData.debugShowFlags, f );
                }
                ImGui::TreePop();
            }
        }
        ImGui::End();
    }

    VkCommandBuffer cmd = currentFrameState.GetCmdBuffer();

    previousFrameTime = currentFrameTime;
    currentFrameTime  = pInfo->currentTime;

    renderResolution.Setup(
        pInfo->pRenderResolutionParams, swapchain->GetWidth(), swapchain->GetHeight(), nvDlss );

    if( observer )
    {
        observer->RecheckFiles();
    }

    if( renderResolution.Width() > 0 && renderResolution.Height() > 0 )
    {
        FillUniform( uniform->GetData(), *pInfo );
        DrawDebugWindows();
        Render( cmd, *pInfo );
    }

    EndFrame( cmd );
    currentFrameState.OnEndFrame();

    // process in next frame
    vsync = debugData.overrideDrawInfo ? debugData.ovrdVsync : pInfo->vsync;
}

namespace RTGL1
{
namespace
{
    bool IsRasterized( const RgMeshInfo& mesh, const RgMeshPrimitiveInfo& primitive )
    {
        if( primitive.flags & RG_MESH_PRIMITIVE_TRANSLUCENT )
        {
            return true;
        }

        if( primitive.flags & RG_MESH_PRIMITIVE_SKY )
        {
            return true;
        }

        if( Utils::UnpackAlphaFromPacked32( primitive.color ) < MESH_TRANSLUCENT_ALPHA_THRESHOLD )
        {
            return true;
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

    // copy to modify
    RgMeshPrimitiveInfo prim       = *pPrimitive;
    RgEditorInfo        primEditor = prim.pEditorInfo ? *prim.pEditorInfo : RgEditorInfo{};
    prim.pEditorInfo               = &primEditor;

    textureMetaManager->Modify( prim, primEditor, false );

    if( IsRasterized( *pMesh, prim ) )
    {
        rasterizer->Upload( currentFrameState.GetFrameIndex(),
                            prim.flags & RG_MESH_PRIMITIVE_SKY ? GeometryRasterType::SKY
                                                               : GeometryRasterType::WORLD,
                            pMesh->transform,
                            prim,
                            nullptr,
                            nullptr );

        if( debugWindows && debugData.primitivesTableEnable == 1 )
        {
            debugData.primitivesTable.push_back( DebugPrim{
                .result         = UploadResult::Dynamic,
                .callIndex      = uint32_t( debugData.primitivesTable.size() ),
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

        if( debugWindows && debugData.primitivesTableEnable == 2 )
        {
            debugData.primitivesTable.push_back( DebugPrim{
                .result         = r,
                .callIndex      = uint32_t( debugData.primitivesTable.size() ),
                .objectId       = pMesh->uniqueObjectID,
                .meshName       = Utils::SafeCstr( pMesh->pMeshName ),
                .primitiveIndex = prim.primitiveIndexInMesh,
                .primitiveName  = Utils::SafeCstr( prim.pPrimitiveNameInMesh ),
                .textureName    = Utils::SafeCstr( prim.pTextureName ),
            } );
        }

        if( pMesh->isExportable && r != UploadResult::Fail )
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

    rasterizer->Upload( currentFrameState.GetFrameIndex(),
                        GeometryRasterType::SWAPCHAIN,
                        RG_TRANSFORM_IDENTITY,
                        *pPrimitive,
                        pViewProjection,
                        pViewport );
    if( debugWindows && debugData.nonworldTableEnable )
    {
        debugData.nonworldTable.push_back( DebugNonWorld{
            .callIndex   = uint32_t( debugData.nonworldTable.size() ),
            .textureName = pPrimitive->pTextureName ? pPrimitive->pTextureName : "",
        } );
    }
}

void RTGL1::VulkanDevice::UploadDecal( const RgDecalUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    decalManager->Upload( currentFrameState.GetFrameIndex(), *pInfo, textureManager );
}

void RTGL1::VulkanDevice::UploadDirectionalLight( const RgDirectionalLightUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    scene->UploadLight( currentFrameState.GetFrameIndex(), pInfo, lightManager.get(), false );
}

void RTGL1::VulkanDevice::UploadSphericalLight( const RgSphericalLightUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    scene->UploadLight( currentFrameState.GetFrameIndex(), pInfo, lightManager.get(), false );
}

void RTGL1::VulkanDevice::UploadSpotlight( const RgSpotLightUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    scene->UploadLight( currentFrameState.GetFrameIndex(), pInfo, lightManager.get(), false );
}

void RTGL1::VulkanDevice::UploadPolygonalLight( const RgPolygonalLightUploadInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

    scene->UploadLight( currentFrameState.GetFrameIndex(), pInfo, lightManager.get(), false );
}

void RTGL1::VulkanDevice::ProvideOriginalTexture( const RgOriginalTextureInfo* pInfo )
{
    if( pInfo == nullptr )
    {
        throw RgException( RG_RESULT_WRONG_FUNCTION_ARGUMENT, "Argument is null" );
    }

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

void RTGL1::VulkanDevice::ScratchGetIndices( RgUtilImScratchTopology topology,
                                             uint32_t                vertexCount,
                                             const uint32_t**        ppOutIndices,
                                             uint32_t*               pOutIndexCount )
{
    const auto indices = scratchImmediate.GetIndices( topology, vertexCount );

    *ppOutIndices   = indices.data();
    *pOutIndexCount = uint32_t( indices.size() );
}

void RTGL1::VulkanDevice::ImScratchClear()
{
    scratchImmediate.Clear();
}

void RTGL1::VulkanDevice::ImScratchStart( RgUtilImScratchTopology topology )
{
    scratchImmediate.StartPrimitive( topology );
}

void RTGL1::VulkanDevice::ImScratchEnd()
{
    scratchImmediate.EndPrimitive();
}

void RTGL1::VulkanDevice::ImScratchVertex( const float& x, const float& y, const float& z )
{
    scratchImmediate.Vertex( x, y, z );
}

void RTGL1::VulkanDevice::ImScratchTexCoord( const float& u, const float& v )
{
    scratchImmediate.TexCoord( u, v );
}

void RTGL1::VulkanDevice::ImScratchColor( const RgColor4DPacked32& color )
{
    scratchImmediate.Color( color );
}

void RTGL1::VulkanDevice::ImScratchSetToPrimitive( RgMeshPrimitiveInfo* pTarget )
{
    scratchImmediate.SetToPrimitive( pTarget );
}

void RTGL1::VulkanDevice::Print( std::string_view msg, RgMessageSeverityFlags severity ) const
{
    if( debugWindows )
    {
        debugData.logs.emplace_back( severity, msg );
    }

    if( userPrint )
    {
        userPrint->Print( msg.data(), severity );
    }
}
