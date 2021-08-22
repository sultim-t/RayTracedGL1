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



// Ray Tracing Gems 2. Chapter 7: Texture Coordinate Gradients Estimation for Ray Cones

struct RayCone
{
    float width;
    float spreadAngle;
};

void propagateRayCone(inout RayCone c, float rayLength)
{
    // new cone width should increase by 2*RayLength*tan(SpreadAngle/2), but RayLength*SpreadAngle is a close approximation
    c.width	+= c.spreadAngle * rayLength;
    c.spreadAngle *= 2;
}

vec4 getUVDerivativesFromRayCone(
    const RayCone rayCone, 
    const vec3 rayDir, 
    const vec3 worldNormal, 
    const vec3 vertWorldPositions[3], 
    const vec2 vertTexCoords[3])
{
    const vec2 uv10 = vertTexCoords[1] - vertTexCoords[0];
    const vec2 uv20 = vertTexCoords[2] - vertTexCoords[0];
    float quadUVArea = abs(uv10.x * uv20.y - uv20.x * uv10.y);

    const vec3 edge10 = vertWorldPositions[1] - vertWorldPositions[0];
    const vec3 edge20 = vertWorldPositions[2] - vertWorldPositions[0];
    const vec3 faceNormal = cross(edge10, edge20);
    float quadArea = length(faceNormal);

    float normalTerm = abs(dot(rayDir, worldNormal));
    float projectedConeWidth = rayCone.width / normalTerm;
    float visibleAreaRatio = (projectedConeWidth * projectedConeWidth) / quadArea;

    float visibleUVArea = quadUVArea * visibleAreaRatio;
    float ULength = sqrt(visibleUVArea);

    return vec4(ULength, 0, 0, ULength);
}