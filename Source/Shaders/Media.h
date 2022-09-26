// Copyright (c) 2021 Sultim Tsyrendashiev
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


#ifndef MEDIA_H_
#define MEDIA_H_


#ifndef DESC_SET_GLOBAL_UNIFORM
    #error DESC_SET_GLOBAL_UNIFORM must be defined
#endif


float getIndexOfRefraction(uint media)
{
    switch (media)
    {
        case MEDIA_TYPE_WATER:
            return globalUniform.indexOfRefractionWater;
        case MEDIA_TYPE_ACID:
            return globalUniform.indexOfRefractionWater;
        case MEDIA_TYPE_GLASS:
            return globalUniform.indexOfRefractionGlass;
        default:
            return 1.0;
    }
}

vec3 getMediaTransmittance( uint media, float distance )
{
    vec3 extinction = vec3( 0.0 );

    if( media == MEDIA_TYPE_WATER )
    {
        extinction = -log( globalUniform.waterColorAndDensity.rgb );
    }
    else if( media == MEDIA_TYPE_ACID )
    {
        extinction = -log( globalUniform.acidColorAndDensity.rgb );
        extinction *= max(1.0, sqrt( globalUniform.acidColorAndDensity.a ) );
    }

    return exp( -distance * extinction );
}

#if SHIPPING_HACK
vec3 getGlowingMediaFog( uint media, float distance )
{
    if( media != MEDIA_TYPE_ACID )
    {
        return vec3( 0 );
    }

    float density = 0.00005 * globalUniform.acidColorAndDensity.a;

    float fog = exp( -distance * density );
    fog       = clamp( 1.0 - fog, 0.0, 1.0 );

    return fog * globalUniform.acidColorAndDensity.rgb;
}
#endif


// Ray Tracing Gems II. Chapter 8: Reflection and Refraction Formulas
// Returns false, if total internal reflection
bool calcRefractionDirection(float n1, float n2, const vec3 I, const vec3 N, out vec3 T)
{
    float eta = n1 / n2; //  relative index of refraction
    float c1 = -dot(I, N); // cos(theta1)
    float w = eta * c1;
    float c2m = (w - eta) * (w + eta); // cos^2(theta2) - 1

    if (c2m < -1.0f)
    { 
        return false; // total internal reflection
    }

    T = eta * I + (w - sqrt(1.0f + c2m)) * N;
    return true;
}

uint getMediaTypeFromFlags(uint geometryInstanceFlags)
{
    if ((geometryInstanceFlags & GEOM_INST_FLAG_MEDIA_TYPE_WATER) != 0)
    {
        return MEDIA_TYPE_WATER;
    }
    else if ((geometryInstanceFlags & GEOM_INST_FLAG_MEDIA_TYPE_GLASS) != 0)
    {
        return MEDIA_TYPE_GLASS;
    }
    else if ((geometryInstanceFlags & GEOM_INST_FLAG_MEDIA_TYPE_ACID) != 0)
    {
        return MEDIA_TYPE_ACID;
    }
    else
    {
        return MEDIA_TYPE_VACUUM;
    }
}

bool isPortalFromFlags(uint geometryInstanceFlags)
{
    return (geometryInstanceFlags & GEOM_INST_FLAG_PORTAL) != 0;
}

bool isRefractFromFlags(uint geometryInstanceFlags)
{
    // if water, but water refraction is disabled, return false;
    // otherwise, check refract flag
    return 
        !(globalUniform.forceNoWaterRefraction != 0 && (geometryInstanceFlags & GEOM_INST_FLAG_MEDIA_TYPE_WATER) != 0) &&
        ((geometryInstanceFlags & GEOM_INST_FLAG_REFRACT) != 0);
}

bool isReflectFromFlags(uint geometryInstanceFlags)
{
    return (geometryInstanceFlags & GEOM_INST_FLAG_REFLECT) != 0;
}

#endif // MEDIA_H_