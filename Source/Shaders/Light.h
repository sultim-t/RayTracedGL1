// Copyright (c) 2022 Sultim Tsyrendashiev
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

#ifndef LIGHT_H_
#define LIGHT_H_

struct DirectionalLight
{
    vec3 direction;
    float angularRadius;
    vec3 color;
};

struct SphereLight
{
    vec3 center;
    float radius;
    vec3 color;
};

struct TriangleLight
{
    vec3 position[3];
    vec3 normal;
    float area;
    vec3 color;
};

struct SpotLight
{
    vec3 center;
    float radius;
    vec3 direction;
    float cosAngleInner;
    vec3 color;
    float cosAngleOuter;
};

DirectionalLight decodeAsDirectionalLight(const ShLightEncoded encoded)
{
    DirectionalLight l;
    l.direction = encoded.data_0.xyz;
    l.angularRadius = encoded.data_0.w;
    l.color = encoded.color;

    return l;
}

SphereLight decodeAsSphereLight(const ShLightEncoded encoded)
{
    SphereLight l;
    l.center = encoded.data_0.xyz;
    l.radius = encoded.data_0.w;
    l.color = encoded.color;

    return l;
}

TriangleLight decodeAsTriangleLight(const ShLightEncoded encoded)
{
    TriangleLight l;
    l.position[0] = encoded.data_0.xyz;
    l.position[1] = encoded.data_1.xyz;
    l.position[2] = encoded.data_2.xyz;
    l.color = encoded.color;

    l.normal = vec3(
        encoded.data_0.w, 
        encoded.data_1.w, 
        encoded.data_2.w
    );
    // len is guaranteed to be > 0.0
    float len = length(l.normal);
    l.normal /= len;
    l.area = len * 0.5;

    return l;
}

SpotLight decodeAsSpotLight(const ShLightEncoded encoded)
{
    SpotLight l;
    l.center = encoded.data_0.xyz;
    l.radius = encoded.data_0.w;
    l.direction = encoded.data_1.xyz;
    l.color = encoded.color;
    l.cosAngleInner = encoded.data_2.x;
    l.cosAngleOuter = encoded.data_2.y;
    
    return l;
}

float getPolySpotFactor(const vec3 lightNormal, const vec3 lightToSurf)
{
    float ll = max(dot(lightNormal, lightToSurf), 0.0);
    return pow(ll, globalUniform.polyLightSpotlightFactor);
}

float getSpotFactor(float cosA, float cosAInner, float cosAOuter)
{
    return square(smoothstep(cosAOuter, cosAInner, cosA));
}



struct DirectionAndLength { vec3 dir; float len; };

DirectionAndLength calcDirectionAndLength(const vec3 start, const vec3 end)
{
    DirectionAndLength r;
    r.dir = end - start;
    r.len = length(r.dir);
    r.dir /= r.len;

    return r;
}

DirectionAndLength calcDirectionAndLengthSafe(const vec3 start, const vec3 end)
{
    DirectionAndLength r;
    r.dir = end - start;
    r.len = max(length(r.dir), 0.001);
    r.dir /= r.len;

    return r;
}



// Veach, E. Robust Monte Carlo Methods for Light Transport Simulation
// The change of variables from solid angle measure to area integration measure
// Note: but without |dot(surfNormal, surfaceToLight)|
float getGeometryFactor(const vec3 lightNormal, const vec3 surfaceToLight, float surfaceToLightDistance)
{
    return abs(dot(lightNormal, -surfaceToLight)) / square(surfaceToLightDistance);
}

float safeSolidAngle(float a)
{
    return a > 0.0 && !isnan(a) && !isinf(a) ? clamp(a, 0.0, 4.0 * M_PI) : 0.0;
}

float calcSolidAngleForSphere(float sphereRadius, float distanceToSphereCenter)
{
    // solid angle here is the spherical cap area on a unit sphere
    float sinTheta = sphereRadius / max(sphereRadius, distanceToSphereCenter);
    float cosTheta = sqrt(1.0 - sinTheta * sinTheta);
    return safeSolidAngle(2 * M_PI * (1.0 - cosTheta));
}

float calcSolidAngleForArea(float area, const vec3 areaPosition, const vec3 areaNormal, const vec3 surfPosition)
{
    const DirectionAndLength surfToAreaLight = calcDirectionAndLength(surfPosition, areaPosition);
    // from area measure to solid angle measure
    return safeSolidAngle(area * getGeometryFactor(areaNormal, surfToAreaLight.dir, surfToAreaLight.len));
}



float getSphereLightWeight(const SphereLight l, const vec3 surfPosition)
{
    return 
        calcSolidAngleForSphere(l.radius, length(l.center - surfPosition)) * 
        getLuminance(l.color);
}

float getTriangleLightWeight(const TriangleLight l, const vec3 surfPosition, const vec2 pointRnd)
{
    const vec3 pointOnLight = sampleTriangle(l.position[0], l.position[1], l.position[2], pointRnd[0], pointRnd[1]);

    return
        calcSolidAngleForArea(l.area, pointOnLight, l.normal, surfPosition) * 
        getLuminance(l.color) *
        getPolySpotFactor(l.normal, normalize(surfPosition - pointOnLight));
}

float getSpotLightWeight(const SpotLight l, const vec3 surfPosition)
{
    const DirectionAndLength toLightCenter = calcDirectionAndLength(surfPosition, l.center);
    const float cosA = max(dot(l.direction, -toLightCenter.dir), 0.0);

    return 
        calcSolidAngleForSphere(l.radius, toLightCenter.len) * 
        getLuminance(l.color) *
        getSpotFactor(cosA, l.cosAngleInner, l.cosAngleOuter);
}




struct LightSample
{
    vec3 position;
    vec3 color;
    float dw;
};

LightSample emptyLightSample()
{
    LightSample r;
    r.position = vec3(0);
    r.color = vec3(0);
    r.dw = 0;
    return r;
}

LightSample sampleDirectionalLight(const DirectionalLight l, const vec3 surfPosition, const vec2 pointRnd)
{
    vec3 lightNormal;
    {
        const float diskRadiusAtUnit = sin(max(0.01, l.angularRadius));
        const vec2 disk = sampleDisk(diskRadiusAtUnit, pointRnd.x, pointRnd.y);
        const mat3 basis = getONB(l.direction);

        lightNormal = normalize(l.direction + basis[0] * disk.x + basis[1] * disk.y);
    }

    LightSample r;
    r.position = surfPosition - lightNormal * MAX_RAY_LENGTH;
    r.color = l.color;
    r.dw = 1.0;
    
    return r;
}

LightSample sampleSphereLight(const SphereLight l, const vec3 surfPosition, const vec2 pointRnd)
{
    const DirectionAndLength toLightCenter = calcDirectionAndLength(surfPosition, l.center);

    // sample hemisphere visible to the surface point
    float ltHsOneOverPdf;
    const vec3 lightNormal = sampleOrientedHemisphere(-toLightCenter.dir, pointRnd.x, pointRnd.y, ltHsOneOverPdf);

    LightSample r;
    r.position = l.center + lightNormal * l.radius;
    r.color = l.color;
    r.dw = calcSolidAngleForSphere(l.radius, toLightCenter.len);

    return r;
}

LightSample sampleTriangleLight(const TriangleLight l, const vec3 surfPosition, const vec2 pointRnd)
{
    LightSample r;
    r.position = sampleTriangle(l.position[0], l.position[1], l.position[2], pointRnd.x, pointRnd.y);
    r.color = l.color * getPolySpotFactor(l.normal, normalize(surfPosition - r.position));
    r.dw = calcSolidAngleForArea(l.area, r.position, l.normal, surfPosition);

    return r;
}

LightSample sampleSpotLight(const SpotLight l, const vec3 surfPosition, const vec2 pointRnd)
{
    LightSample r;
    {
        const vec2 disk = sampleDisk(l.radius, pointRnd.x, pointRnd.y);
        const mat3 basis = getONB(l.direction);

        r.position = l.center + basis[0] * disk.x + basis[1] * disk.y;
    }

    const DirectionAndLength toLightCenter = calcDirectionAndLength(surfPosition, l.center);
    const float cosA = max(dot(l.direction, -toLightCenter.dir), 0.0);
    
    r.color = l.color * getSpotFactor(cosA, l.cosAngleInner, l.cosAngleOuter);
    r.dw = calcSolidAngleForSphere(l.radius, toLightCenter.len);

    return r;
}



float getLightWeight(const ShLightEncoded encoded, const vec3 surfPosition, const vec2 pointRnd)
{
    switch (encoded.lightType)
    {
        case LIGHT_TYPE_DIRECTIONAL:    return 1.0; // TODO: make directional light choosing with others?
        case LIGHT_TYPE_SPHERE:         return getSphereLightWeight     (decodeAsSphereLight        (encoded), surfPosition);
        case LIGHT_TYPE_TRIANGLE:       return getTriangleLightWeight   (decodeAsTriangleLight      (encoded), surfPosition, pointRnd);
        case LIGHT_TYPE_SPOT:           return getSpotLightWeight       (decodeAsSpotLight          (encoded), surfPosition);
        default:                        return 0.0;
    }
}

LightSample sampleLight(const ShLightEncoded encoded, const vec3 surfPosition, const vec2 pointRnd)
{
    switch (encoded.lightType)
    {
        case LIGHT_TYPE_DIRECTIONAL:    return sampleDirectionalLight   (decodeAsDirectionalLight   (encoded), surfPosition, pointRnd);
        case LIGHT_TYPE_SPHERE:         return sampleSphereLight        (decodeAsSphereLight        (encoded), surfPosition, pointRnd);
        case LIGHT_TYPE_TRIANGLE:       return sampleTriangleLight      (decodeAsTriangleLight      (encoded), surfPosition, pointRnd);
        case LIGHT_TYPE_SPOT:           return sampleSpotLight          (decodeAsSpotLight          (encoded), surfPosition, pointRnd);
        default:                        return emptyLightSample();
    }
}

#endif // LIGHT_H_
