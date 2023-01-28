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



// This file was originally a raygen shader. But G-buffer decals are drawn
// on primary surfaces, but not in perfect reflections/refractions. Because
// 

// Must be defined:
// - either RAYGEN_PRIMARY_SHADER or RAYGEN_REFL_REFR_SHADER
// - MATERIAL_MAX_ALBEDO_LAYERS

#if defined(RAYGEN_PRIMARY_SHADER) && defined(RAYGEN_REFL_REFR_SHADER)
    #error Only one of RAYGEN_PRIMARY_SHADER and RAYGEN_REFL_REFR_SHADER must be defined
#endif
#if !defined(RAYGEN_PRIMARY_SHADER) && !defined(RAYGEN_REFL_REFR_SHADER)
    #error RAYGEN_PRIMARY_SHADER or RAYGEN_REFL_REFR_SHADER must be defined
#endif
#ifndef MATERIAL_MAX_ALBEDO_LAYERS
    #error MATERIAL_MAX_ALBEDO_LAYERS is not defined
#endif 
#ifndef MATERIAL_LIGHTMAP_LAYER_INDEX
    #error MATERIAL_LIGHTMAP_LAYER_INDEX is not defined
#endif 


#define HITINFO_INL_CLASSIC_SHADING
bool classicShading_PRIM();


#define DESC_SET_TLAS 0
#define DESC_SET_FRAMEBUFFERS 1
#define DESC_SET_GLOBAL_UNIFORM 2
#define DESC_SET_VERTEX_DATA 3
#define DESC_SET_TEXTURES 4
#define DESC_SET_RANDOM 5
#define DESC_SET_LIGHT_SOURCES 6
#define DESC_SET_CUBEMAPS 7
#define DESC_SET_RENDER_CUBEMAP 8
#define DESC_SET_PORTALS 9
#define LIGHT_SAMPLE_METHOD (LIGHT_SAMPLE_METHOD_NONE)
#include "RaygenCommon.h"

vec2 getMotionVectorForUpscaler(const vec2 motionCurToPrev)
{
    return motionCurToPrev;
}

#define UPSCALER_REACTIVITY_REFLREFR 0.8

vec2 getMotionForInfinitePoint(const ivec2 pix)
{
    // treat as a point with .w=0, i.e. at infinite distance
    vec3 rayDir = getRayDir(getPixelUVWithJitter(pix));

    vec3 viewSpacePosCur   = mat3(globalUniform.view)     * rayDir;
    vec3 viewSpacePosPrev  = mat3(globalUniform.viewPrev) * rayDir;

    vec3 clipSpacePosCur   = mat3(globalUniform.projection)     * viewSpacePosCur;
    vec3 clipSpacePosPrev  = mat3(globalUniform.projectionPrev) * viewSpacePosPrev;

    // don't divide by .w
    vec3 ndcCur            = clipSpacePosCur.xyz;
    vec3 ndcPrev           = clipSpacePosPrev.xyz;

    vec2 screenSpaceCur    = ndcCur.xy  * 0.5 + 0.5;
    vec2 screenSpacePrev   = ndcPrev.xy * 0.5 + 0.5;

    return screenSpacePrev - screenSpaceCur;
}

void storeSky(
    const ivec2 pix, const vec3 rayDir, bool calculateSkyAndStoreToAlbedo, const vec3 throughput,
#ifdef RAYGEN_PRIMARY_SHADER
    float firstHitDepthNDC )
#else
    bool wasSplit )
#endif
{
    imageStore( framebufIsSky, pix, ivec4( 1 ) );

    {
        // check if was already in G-buffer after rasterization pass
        vec3 skyColor =
            calculateSkyAndStoreToAlbedo
                ? getSkyAlbedo( rayDir )
                : imageLoad( framebufAlbedo, getRegularPixFromCheckerboardPix( pix ) ).rgb;
                
        if( !classicShading_PRIM() )
        {
            // to hdr
            skyColor = adjustSky( skyColor );
        }

        imageStore(
            framebufAlbedo, getRegularPixFromCheckerboardPix( pix ), vec4( skyColor, 0.0 ) );
    }

    vec2 m = getMotionForInfinitePoint(pix);

    imageStoreNormal(                       pix, vec3(0.0));
    imageStore(framebufMetallicRoughness,   pix, vec4(0.0));
    imageStore(framebufDepthWorld,          pix, vec4(MAX_RAY_LENGTH * 2.0));
    imageStore(framebufMotion,              pix, vec4(m, 0.0, 0.0));
    imageStore(framebufSurfacePosition,     pix, vec4(SURFACE_POSITION_INCORRECT));
    imageStore(framebufVisibilityBuffer,    pix, vec4(UINT32_MAX));
    imageStore(framebufViewDirection,       pix, vec4(rayDir, 0.0));
    imageStore(framebufScreenEmisRT,        getRegularPixFromCheckerboardPix( pix ), vec4( 0.0 ) );
    imageStore(framebufAcidFogRT,           getRegularPixFromCheckerboardPix( pix ), vec4( 0.0 ) );
#ifdef RAYGEN_PRIMARY_SHADER
    imageStore(framebufPrimaryToReflRefr,   pix, uvec4(0, 0, PORTAL_INDEX_NONE, 0));
    imageStore(framebufDepthGrad,           pix, vec4(0.0));
    imageStore(framebufDepthNdc,            getRegularPixFromCheckerboardPix(pix), vec4(clamp(firstHitDepthNDC, 0.0, 1.0)));
    imageStore(framebufMotionDlss,          getRegularPixFromCheckerboardPix(pix), vec4(getMotionVectorForUpscaler(m), 0.0, 0.0));
    imageStore(framebufThroughput,          pix, vec4(throughput, 0.0));
    imageStore(framebufReactivity,          getRegularPixFromCheckerboardPix(pix), vec4(0.0));
#else
    imageStore(framebufThroughput,          pix, vec4(throughput, wasSplit ? 1.0 : -1.0));
    imageStore(framebufReactivity,          getRegularPixFromCheckerboardPix(pix), vec4(UPSCALER_REACTIVITY_REFLREFR));
#endif
}

uint getNewRayMedia(int i, uint prevMedia, uint geometryInstanceFlags, float roughness)
{
    // if camera is not in vacuum, assume that new media is vacuum
    if (i == 0 && globalUniform.cameraMediaType != MEDIA_TYPE_VACUUM)
    {
       return MEDIA_TYPE_VACUUM;
    }

    return getMediaTypeFromFlags(geometryInstanceFlags, roughness);
}

vec3 getWaterNormal(const RayCone rayCone, const vec3 rayDir, const vec3 baseNormal, const vec3 position, bool wasPortal)
{
    const mat3 basis = getONB(baseNormal);
    const vec2 baseUV = vec2(dot(position, basis[0]), dot(position, basis[1])); 


    // how much vertical flow to apply
    float verticality = 1.0 - abs(dot(baseNormal, globalUniform.worldUpVector.xyz));

    // project basis[0] and basis[1] on up vector
    vec2 flowSpeedVertical = 10 * vec2(dot(basis[0], globalUniform.worldUpVector.xyz), 
                                       dot(basis[1], globalUniform.worldUpVector.xyz));

    vec2 flowSpeedHorizontal = vec2(1.0);


    const float uvScale = 0.05 / globalUniform.waterTextureAreaScale;
    vec2 speed0 = uvScale * mix(flowSpeedHorizontal, flowSpeedVertical, verticality) * globalUniform.waterWaveSpeed;
    vec2 speed1 = -0.9 * speed0 * mix(1.0, -0.1, verticality);


    // for texture sampling
    float derivU = globalUniform.waterTextureDerivativesMultiplier * 0.5 * uvScale * getWaterDerivU(rayCone, rayDir, baseNormal);

    // make water sharper if visible through the portal
    if (wasPortal)
    {
        derivU *= 0.1;
    }


    vec2 uv0 = uvScale * baseUV + globalUniform.time * speed0;
    vec3 n0 = getTextureSampleDerivU(globalUniform.waterNormalTextureIndex, uv0, derivU).xyz;
    n0.xy = n0.xy * 2.0 - vec2(1.0);


    vec2 uv1 = 0.8 * uvScale * baseUV + globalUniform.time * speed1;
    vec3 n1 = getTextureSampleDerivU(globalUniform.waterNormalTextureIndex, uv1, derivU).xyz;
    n1.xy = n1.xy * 2.0 - vec2(1.0);


    vec2 uv2 = 0.1 * (uvScale * baseUV + speed0 * sin(globalUniform.time * 0.5));
    vec3 n2 = getTextureSampleDerivU(globalUniform.waterNormalTextureIndex, uv2, derivU).xyz;
    n2.xy = n2.xy * 2.0 - vec2(1.0);


    const float strength = globalUniform.waterWaveStrength;

    const vec3 n = normalize(vec3(0, 0, 1) + strength * (0.25 * n0 + 0.2 * n1 + 0.1 * n2));
    return basis * n;   
}

mat3 lookAt(const vec3 forward, const vec3 worldUp)
{
    vec3 right = cross(forward, worldUp);
    vec3 up = cross(right, forward);

    return mat3(right, up, forward);
}

vec3 getPortalNormal(const vec3 baseNormal, const vec3 inWorldOffset)
{
    if (globalUniform.twirlPortalNormal == 0)
    {
        return -baseNormal;
    }

    float phaseScale = 3;
    float timeScale = 3;
    float waveScale = 0.01;
    float tm = mod(timeScale * globalUniform.time, M_PI * 2);

    const mat3 inLookAt_Plain = lookAt(-baseNormal, globalUniform.worldUpVector.xyz);
    const vec2 localOffset_Plain = vec2(dot(inWorldOffset, inLookAt_Plain[0]), 
                                        dot(inWorldOffset, inLookAt_Plain[1]));

    float distance = length(localOffset_Plain);
    float angle = atan(localOffset_Plain.y, localOffset_Plain.x);

    float phase = sin(phaseScale * sqrt(distance) + angle + tm) + 1.0;
    phase *= waveScale;
    // less weight around center
    phase *= clamp(distance / 20, 0, 1); 

    vec3 localN = { phase, phase, 1.0 };

    return inLookAt_Plain * normalize(localN);
}

bool isBackface( const vec3 normal, const vec3 rayDir )
{
    return dot( normal, -rayDir ) < 0.0;
}

vec3 getNormal( const vec3    position,
                vec3          normal,
                const RayCone rayCone,
                const vec3    rayDir,
                bool          isWater,
                bool          wasPortal )
{
    if( isWater )
    {
        if( isBackface( normal, rayDir ) )
        {
            normal *= -1;
        }

        return getWaterNormal( rayCone, rayDir, normal, position, wasPortal );
    }
    else
    {
        if( isBackface( normal, rayDir ) )
        {
            normal *= -1;
        }

        return normalize( normal );
    }
}

bool classicShading_PRIM()
{
    const ivec2 regularPix = ivec2( gl_LaunchIDEXT.xy );
    return classicShading( regularPix );
}

#ifdef RAYGEN_PRIMARY_SHADER
void main() 
{
    const ivec2 regularPix = ivec2(gl_LaunchIDEXT.xy);
    const ivec2 pix = getCheckerboardPix(regularPix);
    const vec2 inUV = getPixelUVWithJitter(regularPix);

    const vec3 cameraOrigin = globalUniform.cameraPosition.xyz;
    const vec3 cameraRayDir = getRayDir(inUV);
    const vec3 cameraRayDirAX = getRayDirAX(inUV);
    const vec3 cameraRayDirAY = getRayDirAY(inUV);

    const uint randomSeed = getRandomSeed(pix, globalUniform.frameId);
    
    
    const ShPayload primaryPayload = tracePrimaryRay(cameraOrigin, cameraRayDir);


    const uint currentRayMedia = globalUniform.cameraMediaType;


    // was no hit
    if (!doesPayloadContainHitInfo(primaryPayload))
    {
        vec3 throughput = vec3(1.0);
        // throughput *= getMediaTransmittance(currentRayMedia, pow(abs(dot(cameraRayDir, globalUniform.worldUpVector.xyz)), -3));

        // if sky is a rasterized geometry, it was already rendered to albedo framebuf 
        storeSky(pix, cameraRayDir, globalUniform.skyType != SKY_TYPE_RASTERIZED_GEOMETRY, throughput, MAX_RAY_LENGTH * 2.0);
        return;
    }


    vec2 motionCurToPrev;
    float motionDepthLinearCurToPrev;
    vec2 gradDepth;
    float firstHitDepthNDC;
    float firstHitDepthLinear;
    vec3 screenEmission;
    const ShHitInfo h = getHitInfoPrimaryRay(primaryPayload, cameraOrigin, cameraRayDirAX, cameraRayDirAY, motionCurToPrev, motionDepthLinearCurToPrev, gradDepth, firstHitDepthNDC, firstHitDepthLinear, screenEmission);


    vec3 throughput = vec3(1.0);
    throughput *= getMediaTransmittance(currentRayMedia, firstHitDepthLinear);


    imageStore(framebufIsSky,               pix, ivec4(0));
    imageStore(framebufAlbedo,              getRegularPixFromCheckerboardPix(pix), vec4(h.albedo, 0.0));
    imageStore(framebufScreenEmisRT,        getRegularPixFromCheckerboardPix(pix), vec4(screenEmission * throughput , 0.0));
    imageStore(framebufAcidFogRT,           getRegularPixFromCheckerboardPix(pix), vec4(getGlowingMediaFog(currentRayMedia, firstHitDepthLinear), 0));
    imageStoreNormal(                       pix, h.normal);
    imageStore(framebufMetallicRoughness,   pix, vec4(h.metallic, h.roughness, 0, 0));
    imageStore(framebufDepthWorld,          pix, vec4(firstHitDepthLinear));
    // depth gradients is not 2d, to remove vertical/horizontal artifacts
    imageStore(framebufDepthGrad,           pix, vec4(length(gradDepth)));
    imageStore(framebufMotion,              pix, vec4(motionCurToPrev, motionDepthLinearCurToPrev, 0.0));
    imageStore(framebufSurfacePosition,     pix, vec4(h.hitPosition, uintBitsToFloat(h.instCustomIndex)));
    imageStore(framebufVisibilityBuffer,    pix, packVisibilityBuffer(primaryPayload));
    imageStore(framebufViewDirection,       pix, vec4(cameraRayDir, 0.0));
    imageStore(framebufThroughput,          pix, vec4(throughput, 0.0));

    // save some info for refl/refr shader
    imageStore(framebufPrimaryToReflRefr,   pix, uvec4(h.geometryInstanceFlags, primaryPayload.instIdAndIndex, h.portalIndex, 0));

    // save info for rasterization and upscalers (FSR/DLSS), but only about primary surface,
    // as reflections/refraction only may be losely represented via rasterization
    imageStore(framebufDepthNdc,            getRegularPixFromCheckerboardPix(pix), vec4(clamp(firstHitDepthNDC, 0.0, 1.0)));
    imageStore(framebufMotionDlss,          getRegularPixFromCheckerboardPix(pix), vec4(getMotionVectorForUpscaler(motionCurToPrev), 0.0, 0.0));
    imageStore(framebufReactivity,          getRegularPixFromCheckerboardPix(pix), vec4(0.0));
}
#endif


#ifdef RAYGEN_REFL_REFR_SHADER
void main() 
{
    if (globalUniform.reflectRefractMaxDepth == 0)
    {
        return;
    }


    const ivec2 regularPix = ivec2(gl_LaunchIDEXT.xy);
    const ivec2 pix = getCheckerboardPix(regularPix);
    const vec2 inUV = getPixelUVWithJitter(regularPix);

    const vec3 cameraRayDir = getRayDir(inUV);
    
    if (isSkyPix(pix))
    {
        return;
    }



    // restore state from primary shader
    const uvec3 primaryToReflRefrBuf        = texelFetch(framebufPrimaryToReflRefr_Sampler, pix, 0).rgb;
    ShHitInfo h;
    h.albedo                                = texelFetch(framebufAlbedo_Sampler, getRegularPixFromCheckerboardPix(pix), 0).rgb;
    h.hitPosition                           = texelFetch(framebufSurfacePosition_Sampler, pix, 0).xyz;
    h.geometryInstanceFlags                 = primaryToReflRefrBuf.r;
    h.portalIndex                           = primaryToReflRefrBuf.b;
    h.normal                                = texelFetchNormal(pix);
    {
        vec2 mr                             = texelFetch( framebufMetallicRoughness_Sampler, pix, 0 ).rg;
        h.metallic                          = mr.r;
        h.roughness                         = mr.g;
    }
    const vec3  motionBuf                   = texelFetch(framebufMotion_Sampler, pix, 0).rgb;
    vec2        motionCurToPrev             = motionBuf.rg;
    float       motionDepthLinearCurToPrev  = motionBuf.b;
    float       firstHitDepthLinear         = texelFetch(framebufDepthWorld_Sampler, pix, 0).r;
    vec3        screenEmission              = texelFetch(framebufScreenEmisRT_Sampler, getRegularPixFromCheckerboardPix(pix), 0).rgb;
    vec3        acidFog                     = texelFetch(framebufAcidFogRT_Sampler, getRegularPixFromCheckerboardPix(pix), 0).rgb;
    vec3        throughput                  = texelFetch(framebufThroughput_Sampler, pix, 0).rgb;
    ShPayload currentPayload;
    currentPayload.instIdAndIndex           = primaryToReflRefrBuf.g;



    RayCone rayCone;
    rayCone.width = 0;
    rayCone.spreadAngle = globalUniform.cameraRayConeSpreadAngle;

    float fullPathLength = firstHitDepthLinear;
    vec3 prevHitPosition = h.hitPosition;
    bool wasSplit = false;
    bool wasPortal = false;
    vec3 virtualPos = h.hitPosition;
    vec3 rayDir = cameraRayDir;
    uint currentRayMedia = globalUniform.cameraMediaType;
    // if there was no hitinfo from refl/refr, preserve primary hitinfo
    bool hitInfoWasOverwritten = false;


    propagateRayCone(rayCone, firstHitDepthLinear);



    for (int i = 0; i < globalUniform.reflectRefractMaxDepth; i++)
    {
        const uint instIndex = unpackInstanceIdAndCustomIndex(currentPayload.instIdAndIndex).y;


        bool isPixOdd = isCheckerboardPixOdd(pix) != 0;


        uint newRayMedia =
            getNewRayMedia( i, currentRayMedia, h.geometryInstanceFlags, h.roughness );

        bool isPortal =
            isPortalFromFlags( h.geometryInstanceFlags ) && h.portalIndex != PORTAL_INDEX_NONE;
        bool toRefract = isRefractFromFlags( h.geometryInstanceFlags, h.roughness );
        bool toReflect = isReflectFromFlags( h.geometryInstanceFlags, h.roughness );


        if (!toReflect && !toRefract && !isPortal)
        {
            break;
        }


        const float curIndexOfRefraction = getIndexOfRefraction(currentRayMedia);
        const float newIndexOfRefraction = getIndexOfRefraction(newRayMedia);

        const bool isWater =
            !isPortal && ( newRayMedia == MEDIA_TYPE_WATER || currentRayMedia == MEDIA_TYPE_WATER ||
                           newRayMedia == MEDIA_TYPE_ACID || currentRayMedia == MEDIA_TYPE_ACID );

        const vec3 normal =
            getNormal( h.hitPosition, h.normal, rayCone, rayDir, isWater, wasPortal );


        bool delaySplitOnNextTime = false;
            
        if ((h.geometryInstanceFlags & GEOM_INST_FLAG_NO_MEDIA_CHANGE) != 0)
        {
            // apply small new media transmittance, and ignore the media (but not the refraction indices)
            throughput *= getMediaTransmittance(newRayMedia, 1.0);
            newRayMedia = currentRayMedia;
            
            // if reflections are disabled if viewing from inside of NO_MEDIA_CHANGE geometry
            delaySplitOnNextTime = (globalUniform.noBackfaceReflForNoMediaChange != 0) && isBackface(h.normal, rayDir);
        }

           

        vec3 rayOrigin = h.hitPosition;
        bool doSplit = !wasSplit;
        bool doRefraction;
        vec3 refractionDir;
        float F;

        if (delaySplitOnNextTime)
        {
            doSplit = false;
            // force refraction for all pixels
            toRefract = true;
            isPixOdd = true;
        }
        
        if (toRefract && calcRefractionDirection(curIndexOfRefraction, newIndexOfRefraction, rayDir, normal, refractionDir))
        {
            doRefraction = isPixOdd;
            F = getFresnelSchlick(curIndexOfRefraction, newIndexOfRefraction, -rayDir, normal);
        }
        else
        {
            // total internal reflection
            doRefraction = false;
            doSplit = false;
            F = 1.0;
        }
        
        if (doRefraction)
        {
            rayDir = refractionDir;
            throughput *= (1 - F);

            // change media
            currentRayMedia = newRayMedia;
        }
        else if (isPortal)
        {
            const ShPortalInstance portal = g_portals[h.portalIndex];

            const vec3 inCenter = portal.inPosition.xyz;
            const vec3 inWorldOffset = h.hitPosition - inCenter;

            mat3 inLookAt = lookAt(getPortalNormal(normal, inWorldOffset), globalUniform.worldUpVector.xyz);

            const vec3 outCenter = portal.outPosition.xyz;
            const mat3 outLookAt = lookAt(portal.outDirection.xyz, 
                                          portal.outUp.xyz);

            // to local space; then to world space but at portal output
            rayDir = outLookAt * (transpose(inLookAt) * rayDir);

            const vec2 localOffset = vec2(dot(inWorldOffset, inLookAt[0]), 
                                          dot(inWorldOffset, inLookAt[1]));

            rayOrigin = outCenter + localOffset.x * outLookAt[0] + localOffset.y * outLookAt[1];

            wasPortal = true;
        }
        else
        {
            rayDir = reflect(rayDir, normal);

            if( !isWater )
            {
                throughput *= getFresnelSchlick( max( 0, dot( normal, rayDir ) ),
                                                 getSpecularColor( h.albedo, h.metallic ) );
            }
            else
            {
                throughput *= F;
            }
        }

        if (doSplit)
        {
            throughput *= 2;
            wasSplit = true;
        }


        currentPayload = traceReflectionRefractionRay(rayOrigin, rayDir, instIndex, h.geometryInstanceFlags, doRefraction);

        
        if (!doesPayloadContainHitInfo(currentPayload))
        {
            throughput *= getMediaTransmittance(currentRayMedia, pow(abs(dot(rayDir, globalUniform.worldUpVector.xyz)), -3));

            storeSky(pix, rayDir, true, throughput, wasSplit);
            return;  
        }

        float rayLen;
        vec3 scrEmis;

        h = getHitInfoWithRayCone_ReflectionRefraction(
            currentPayload, rayCone, 
            rayOrigin, rayDir, cameraRayDir, 
            virtualPos, 
            rayLen, 
            motionCurToPrev, motionDepthLinearCurToPrev,
            scrEmis
        );


        hitInfoWasOverwritten = true;
        throughput *= getMediaTransmittance(currentRayMedia, rayLen);
        propagateRayCone(rayCone, rayLen);
        fullPathLength += rayLen;
        prevHitPosition = h.hitPosition;
        screenEmission += scrEmis * throughput;
        acidFog += getGlowingMediaFog(currentRayMedia, rayLen) * (doSplit ? 2.0 : 1.0);
    }


    if (!hitInfoWasOverwritten)
    {
        return;
    }


    imageStore(framebufIsSky,               pix, ivec4(0));
    imageStore(framebufAlbedo,              getRegularPixFromCheckerboardPix(pix), vec4(h.albedo, 0.0));
    imageStore(framebufScreenEmisRT,        getRegularPixFromCheckerboardPix(pix), vec4(screenEmission + ( globalUniform.cameraMediaType != MEDIA_TYPE_ACID ? acidFog * 0.05 : vec3( 0.0 ) ), 0.0));
    imageStore(framebufAcidFogRT,           getRegularPixFromCheckerboardPix(pix), vec4(acidFog, 0));
    imageStoreNormal(                       pix, h.normal);
    imageStore(framebufMetallicRoughness,   pix, vec4(h.metallic, h.roughness, 0, 0));
    imageStore(framebufDepthWorld,          pix, vec4(fullPathLength));
    imageStore(framebufMotion,              pix, vec4(motionCurToPrev, motionDepthLinearCurToPrev, 0.0));
    imageStore(framebufSurfacePosition,     pix, vec4(h.hitPosition, uintBitsToFloat(h.instCustomIndex)));
    imageStore(framebufVisibilityBuffer,    pix, packVisibilityBuffer(currentPayload));
    imageStore(framebufViewDirection,       pix, vec4(rayDir, 0.0));
    imageStore(framebufThroughput,          pix, vec4(throughput, wasSplit ? 1.0 : -1.0));
    imageStore(framebufReactivity,          getRegularPixFromCheckerboardPix(pix), vec4(UPSCALER_REACTIVITY_REFLREFR));
}
#endif