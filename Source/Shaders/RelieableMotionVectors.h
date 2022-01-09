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


vec2 projectPointToScreenWithPrev(vec3 posPrev)
{
    const vec4 viewSpacePosPrev  = globalUniform.viewPrev * vec4(posPrev, 1.0);
    const vec4 clipSpacePosPrev  = globalUniform.projectionPrev * viewSpacePosPrev;
    const vec3 ndcPrev           = clipSpacePosPrev.xyz / clipSpacePosPrev.w;
    const vec2 screenSpacePrev   = ndcPrev.xy * 0.5 + 0.5;

    return screenSpacePrev;
}


bool rayPlaneIntersect(vec3 rayOrigin, vec3 rayDirection, vec3 planePoint, vec3 planeNormal, out vec3 result)
{
    float d = dot(planeNormal, rayDirection);

    if (d < 0.0001)
    {
        float t = dot(planePoint - rayOrigin, planeNormal) / d;
        
        result = rayOrigin + t * rayDirection;
        return true;
    }

    return false;
}


void getSurfaceInfo(
    ivec2 pix, 
    out vec3 surfPos,    out vec3 surfPos_Prev,
    out vec3 surfNormal, out vec3 surfNormal_Prev)
{
    const vec4 visBuf = texelFetch(framebufVisibilityBuffer_Sampler, pix, 0);
    
    int instanceId, instCustomIndex;
    int localGeomIndex, primIndex;
    vec2 bary;
    unpackVisibilityBuffer(visBuf, instanceId, instCustomIndex, localGeomIndex, primIndex, bary);

    getTriangle_PositionAndNormal(
        instanceId, instCustomIndex, localGeomIndex, primIndex, bary,
        surfPos, surfPos_Prev,
        surfNormal, surfNormal_Prev);
}


void getBlockerInfo(const ShPayload blockerPl, out vec3 blockerPos, out vec3 blockerPos_Prev)
{
    int instanceId, instCustomIndex;
    int geomIndex, primIndex;

    unpackInstanceIdAndCustomIndex(blockerPl.instIdAndIndex, instanceId, instCustomIndex);
    unpackGeometryAndPrimitiveIndex(blockerPl.geomAndPrimIndex, geomIndex, primIndex);

    const ShTriangle tr = getTriangle(instanceId, instCustomIndex, geomIndex, primIndex);

    const vec2 b = blockerPl.baryCoords;
    const vec3 baryCoords = vec3(1.0f - b.x - b.y, b.x, b.y);
    
    blockerPos      = tr.positions     * baryCoords;
    blockerPos_Prev = tr.prevPositions * baryCoords;
}


// Ray Tracing Gems II. Chapter 25. TEMPORALLY RELIABLE MOTION VECTORS FOR BETTER USE OF TEMPORAL INFORMATION
vec2 getShadowMotionVector(
    ivec2 pix, 
    const ShPayload blocker, 
    vec3 lightPos, vec3 lightPos_Prev)
{
    // For each pixel X_i in shadow (in image space) ...
    vec2 X = pix + vec2(0.5f, 0.5f);

    // TODO: surfPos, surfNormal?
    vec3 surfPos, surfPos_Prev, surfNormal, surfNormal_Prev;
    getSurfaceInfo(pix, surfPos, surfPos_Prev, surfNormal, surfNormal_Prev);

    vec3 blockerPos, blockerPos_Prev;
    getBlockerInfo(blocker, blockerPos, blockerPos_Prev);    



    // Next, find the intersection of the virtual plane 
    // (defined by S_i and its normal in the previous frame) 
    // and the ray (from L_(i-1) to B_(i-1)).
    vec3 origin = lightPos_Prev;
    vec3 direction = blockerPos_Prev - lightPos_Prev;
   
    // The custom function rayPlaneIntersect(...) finds the intersection between 
    // a ray (defined by origin and direction) and a plane ( defined by point and normal).
    vec3 intersection;
    if (!rayPlaneIntersect(origin, direction, surfPos_Prev, surfNormal_Prev, intersection))
    {
        return vec2(0.0);
    }
    
    // Finally, project it to screen space to find X^(V)_(i-1).
    // The custom function toScreen(...) projects 
    // a given world-space point to screen space 
    // to get corresponding the image-space pixel.
    vec2 prevX = projectPointToScreenWithPrev(intersection);

    return prevX - X;
}