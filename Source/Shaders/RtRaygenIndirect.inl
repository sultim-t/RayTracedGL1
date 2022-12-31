// Copyright (c) 2021-2022 Sultim Tsyrendashiev
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

layout (constant_id = 0) const uint maxAlbedoLayerCount = 0;
#define MATERIAL_MAX_ALBEDO_LAYERS maxAlbedoLayerCount

#define FORCE_EVALBRDF_GGX_LOOSE

#define DESC_SET_TLAS 0
#define DESC_SET_FRAMEBUFFERS 1
#define DESC_SET_GLOBAL_UNIFORM 2
#define DESC_SET_VERTEX_DATA 3
#define DESC_SET_TEXTURES 4
#define DESC_SET_RANDOM 5
#define DESC_SET_LIGHT_SOURCES 6
#define DESC_SET_CUBEMAPS 7
#define DESC_SET_RENDER_CUBEMAP 8
#define DESC_SET_RESTIR_INDIRECT 10
#define LIGHT_SAMPLE_METHOD (LIGHT_SAMPLE_METHOD_INDIR)
#include "RaygenCommon.h"
#include "ReservoirIndirect.h"



// v -- direction to viewer
// n -- surface normal
vec3 getSpecularBounce(const uint seed, uint bounceIndex,
                       const vec3 n, const float roughness, const vec3 surfSpecularColor,
                       const vec3 v, 
                       out float oneOverSourcePdf)
{
    const vec2 u = rndBlueNoise8( seed, RANDOM_SALT_SPEC_BOUNCE( bounceIndex ) ).xy;
    return sampleSmithGGX( n, v, roughness, u[ 0 ], u[ 1 ], oneOverSourcePdf );
}

// n -- surface normal
vec3 getDiffuseBounce(const uint seed, uint bounceIndex, const vec3 n, out float oneOverSourcePdf)
{
    const vec2 u = rndBlueNoise8( seed, RANDOM_SALT_DIFF_BOUNCE( bounceIndex ) ).xy;
    return sampleLambertian( n, u[ 0 ], u[ 1 ], oneOverSourcePdf );
}


#define FIRST_BOUNCE_MIP_BIAS 0
#define SECOND_BOUNCE_MIP_BIAS 32

Surface traceBounce(const vec3 originPosition, float originRoughness, uint originInstCustomIndex,
                    const vec3 bounceDir, float bounceMipBias)
{
    const ShPayload p = traceIndirectRay(originInstCustomIndex, originPosition, bounceDir); 

    if (!doesPayloadContainHitInfo(p))
    {
        Surface s;
        s.isSky = true;
        return s;
    }

    return hitInfoToSurface_Indirect(
        getHitInfoBounce(p, originPosition, originRoughness, bounceMipBias), 
        bounceDir);
}

vec3 processSecondDiffuseBounce(const uint seed, const Surface surf, const vec3 bounceDir, float oneOverPdf)
{
    const Surface hitSurf = traceBounce(surf.position + surf.normalGeom * 0.01,
                                        surf.roughness,
                                        surf.instCustomIndex,
                                        bounceDir,
                                        SECOND_BOUNCE_MIP_BIAS);

    if (hitSurf.isSky)
    {
        return getSky(bounceDir) * oneOverPdf;
    }

    // calculate direct illumination in a hit position
    const vec3 diffuse = processDirectIllumination(seed, hitSurf, 2);

    return (hitSurf.emission + diffuse) * hitSurf.albedo * oneOverPdf;
}

SampleIndirect processIndirect( const uint seed, const Surface surf, out float oneOverSourcePdf )
{
    vec3 bounceDir;

#if 1
    bounceDir = getSpecularBounce( seed,
                                   1,
                                   surf.normal,
                                   surf.roughness,
                                   surf.specularColor,
                                   surf.toViewerDir,
                                   oneOverSourcePdf );
#else
    bounceDir = getDiffuseBounce( seed, 1, surf.normalGeom, oneOverSourcePdf );
#endif

    const Surface hitSurf = traceBounce(surf.position + surf.normalGeom * 0.01, 
                                        surf.roughness, 
                                        surf.instCustomIndex, 
                                        bounceDir, 
                                        FIRST_BOUNCE_MIP_BIAS); 

    if (hitSurf.isSky)
    {
        SampleIndirect s;
        s.position  = hitSurf.position + bounceDir * MAX_RAY_LENGTH;
        s.normal    = -bounceDir;
        s.radiance  = getSky(bounceDir);
        
        return s;
    }
    
    // calculate direct diffuse illumination in a hit position
    vec3 diffuse = processDirectIllumination(seed, hitSurf, 1);

    if( globalUniform.indirSecondBounce != 0 )
    {
        float oneOverPdf_Second;
        const vec3 bounceDir_Second = getDiffuseBounce(seed, 2, hitSurf.normalGeom, oneOverPdf_Second);

        diffuse += processSecondDiffuseBounce(seed, 
                                              hitSurf,
                                              bounceDir_Second,
                                              oneOverPdf_Second);
    }
    
#if 0
    diffuse = clamp( diffuse, vec3( 0 ), vec3( 50 ) );
#endif

    SampleIndirect s;
    s.position  = hitSurf.position;
    s.normal    = hitSurf.normalGeom;
    s.radiance  = (hitSurf.emission + diffuse) * hitSurf.albedo;

    return s;
}

void shade(const Surface surf, const SampleIndirect indir, float oneOverPdf,
           out vec3 diffuse, out vec3 specular)
{
    vec3 l = safeNormalize(indir.position - surf.position);
    float nl = dot(surf.normal, l);
    float ngl = dot(surf.normalGeom, l);

    if (nl <= 0 || ngl <= 0)
    {
        diffuse = specular = vec3(0);
        return;
    }

    diffuse  = nl * indir.radiance * evalBRDFLambertian(1.0);
    specular = nl * indir.radiance * evalBRDFSmithGGX(surf.normal, surf.toViewerDir, l, surf.roughness, surf.specularColor);

    diffuse  *= oneOverPdf;
    specular *= oneOverPdf;
}

float targetPdfForIndirectSample(const SampleIndirect s)
{
    return getLuminance(s.radiance);
}

bool testSurfaceForReuseIndirect(
    const ivec3 curChRenderArea, const ivec2 otherPix,
    float curDepth, float otherDepth,
    const vec3 curNormal, const vec3 otherNormal)
{
    const float DepthThreshold = 0.05;
    const float NormalThreshold = 0.9;

    return 
        testPixInRenderArea(otherPix, curChRenderArea) &&
        (abs(curDepth - otherDepth) / abs(curDepth) < DepthThreshold) &&
        (dot(curNormal, otherNormal) > NormalThreshold);
}

float getDiffuseWeight( float roughness )
{
    return smoothstep( MIN_GGX_ROUGHNESS,
                       FAKE_ROUGH_SPECULAR_THRESHOLD + FAKE_ROUGH_SPECULAR_LENGTH,
                       roughness );
}



#define TEMPORAL_SAMPLES_INDIR    1
#define TEMPORAL_RADIUS_INDIR_MAX 2.0

#define SPATIAL_SAMPLES_INDIR 4
#define SPATIAL_RADIUS_INDIR  32.0

#define DEBUG_TRACE_BIAS_CORRECT_RAY 0



#ifdef RT_RAYGEN_INDIRECT_INIT
void main()
{
    const ivec2 pix = ivec2(gl_LaunchIDEXT.xy);
    const uint seed = getRandomSeed(pix, globalUniform.frameId);
    uint salt = RANDOM_SALT_RESAMPLE_INDIRECT_BASE;

    Surface surf = fetchGbufferSurface(pix);
    surf.position += surf.toViewerDir * RAY_ORIGIN_LEAK_BIAS;

    if (surf.isSky)
    {
        restirIndirect_StoreInitialSample( pix, emptySampleIndirect(), 0.0 );
        return;
    }

    float          oneOverSourcePdf;
    SampleIndirect initial = processIndirect( seed, surf, oneOverSourcePdf );

    restirIndirect_StoreInitialSample( pix, initial, oneOverSourcePdf );
}
#endif // RT_RAYGEN_INDIRECT_INIT



#ifdef RT_RAYGEN_INDIRECT_FINAL
ReservoirIndirect loadInitialSampleAsReservoir( const ivec2 pix )
{
    float          oneOverSourcePdf;
    SampleIndirect s         = restirIndirect_LoadInitialSample( pix, oneOverSourcePdf );
    float          targetPdf = targetPdfForIndirectSample( s );

    ReservoirIndirect r = emptyReservoirIndirect();
    updateReservoirIndirect( r, s, targetPdf, oneOverSourcePdf, 0.5 );
    return r;
}

void main()
{
    const ivec2 pix  = ivec2( gl_LaunchIDEXT.xy );
    const uint  seed = getRandomSeed( pix, globalUniform.frameId );
    uint        salt = RANDOM_SALT_RESAMPLE_INDIRECT_BASE;

    Surface surf = fetchGbufferSurface( pix );
    surf.position += surf.toViewerDir * RAY_ORIGIN_LEAK_BIAS;

    if( surf.isSky )
    {
        return;
    }


    ReservoirIndirect combined = loadInitialSampleAsReservoir( pix );


    // assuming that pix is checkerboarded
    const ivec3 chRenderArea = getCheckerboardedRenderArea( pix );
    const float motionZ           = texelFetch( framebufMotion_Sampler, pix, 0 ).z;
    const float depthCur          = texelFetch( framebufDepthWorld_Sampler, pix, 0 ).r;
    const vec2  posPrev           = getPrevScreenPos( framebufMotion_Sampler, pix );

    int spatialSamplesCount = int( SPATIAL_SAMPLES_INDIR * getDiffuseWeight( surf.roughness ) );


    for( int pixIndex = 0; pixIndex < TEMPORAL_SAMPLES_INDIR; pixIndex++ )
    {
        // TODO: need low discrepancy noise
        ivec2 pp;
        {
            vec2 rndOffset = rnd8_4( seed, salt++ ).xy * 2.0 - 1.0;
            rndOffset *= square( getDiffuseWeight( surf.roughness ) );

            pp = ivec2( floor( posPrev + rndOffset * TEMPORAL_RADIUS_INDIR_MAX ) );
        }

        {
            if( isSkyPix( pp ) )
            {
                continue;
            }
        }
        {
            const float depthPrev  = texelFetch( framebufDepthWorld_Prev_Sampler, pp, 0 ).r;
            const vec3  normalPrev = texelFetchNormal_Prev( pp );

            if( !testSurfaceForReuseIndirect(
                    chRenderArea, pp, depthCur, depthPrev - motionZ, surf.normal, normalPrev ) )
            {
                continue;
            }
        }
        {
            const float antilagAlpha_Indir = texelFetch(
                framebufDISGradientHistory_Sampler, pp / COMPUTE_ASVGF_STRATA_SIZE, 0 )[ 1 ];

            // if there's too much difference, don't use a temporal sample
            if( antilagAlpha_Indir > 0.25 )
            {
                spatialSamplesCount++;
                continue;
            }
        }

        ReservoirIndirect temporal = restirIndirect_LoadReservoir_Prev( pp );
        // renormalize to prevent precision problems
        normalizeReservoirIndirect( temporal, 20 );

        float rnd = rnd16( seed, salt++ );
        updateCombinedReservoirIndirect( combined, temporal, rnd );
    }



    {
        uint nobiasM = combined.M; 

        for( int pixIndex = 0; pixIndex < spatialSamplesCount; pixIndex++ )
        {
            // TODO: need low discrepancy noise
            ivec2 pp;
            {
                vec2 rndOffset = rnd8_4( seed, salt++ ).xy * 2.0 - 1.0;
                pp = pix + ivec2( rndOffset * SPATIAL_RADIUS_INDIR );
            }

            {
                if( isSkyPix( pp ) )
                {
                    continue;
                }

                const float depthOther  = texelFetch( framebufDepthWorld_Sampler, pp, 0 ).r;
                const vec3  normalOther = texelFetchNormal( pp );

                if( !testSurfaceForReuseIndirect(
                        chRenderArea, pp, depthCur, depthOther, surf.normal, normalOther ) )
                {
                    continue;
                }
            }

            ReservoirIndirect reservoir_q = loadInitialSampleAsReservoir( pp );

            // TODO: acceptable jacobian values
            float oneOverJacobian = 1.0;
            #if 0
            {
                const vec3 x1_r = surf.position;
                const vec3 x1_q = texelFetch( framebufSurfacePosition_Sampler, pp, 0 ).xyz;

                const vec3 x2_q = reservoir_q.selected.position;
                const vec3 n2_q = reservoir_q.selected.normal;

                const DirectionAndLength phi_r = calcDirectionAndLengthSafe( x2_q, x1_r );
                const DirectionAndLength phi_q = calcDirectionAndLengthSafe( x2_q, x1_q );

                oneOverJacobian =
                    safePositiveRcp( getGeometryFactorClamped( n2_q, phi_r.dir, phi_r.len ) ) *
                    getGeometryFactorClamped( n2_q, phi_q.dir, phi_q.len );
            }
            #endif

            float targetPdf_curSurf = 0.0;

#if DEBUG_TRACE_BIAS_CORRECT_RAY
            if( !traceShadowRay(
                    surf.instCustomIndex, surf.position, reservoir_q.selected.position, false ) )
#endif
            {
                targetPdf_curSurf =
                    targetPdfForIndirectSample( reservoir_q.selected ) * oneOverJacobian;
            }

            float rnd = rnd16( seed, salt++ );
            updateCombinedReservoirIndirect_newSurf(
                combined, reservoir_q, targetPdf_curSurf, rnd );

            if( targetPdf_curSurf > 0.0 )
            {
                nobiasM += reservoir_q.M;
            }
        }

        combined.M = nobiasM;
    }

    restirIndirect_StoreReservoir( pix, combined );



    vec3 diffuse, specular;
    shade( surf, combined.selected, calcSelectedSampleWeightIndirect( combined ), diffuse, specular );

    const vec3 surfToHitPoint = combined.selected.position - surf.position;

    {
        const vec3 direct = texelFetchUnfilteredSpecular( pix );
        // save indirect hit distance, if brighter than the direct light
        if( getLuminance( direct ) < getLuminance( specular ) )
        {
            imageStore(
                framebufViewDirection, pix, vec4( -surf.toViewerDir, length( surfToHitPoint ) ) );
        }


        // demodulate for denoising
        imageStoreUnfilteredSpecular( pix,
                                      direct + demodulateSpecular( specular, surf.specularColor ) );
    }

    {
        imageStoreUnfilteredIndirectSH(
            pix, irradianceToSH( diffuse, safeNormalize( surfToHitPoint ) ) );
    }
}
#endif // RT_RAYGEN_INDIRECT_FINAL
