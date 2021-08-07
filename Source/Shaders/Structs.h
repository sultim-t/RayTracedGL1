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

struct ShTriangle
{
    mat3    positions;
    mat3    prevPositions;
    mat3    normals;
    mat3x2  layerTexCoord[3];
    vec4    materialColors[3];
    uvec3   materials[3];
    uint    materialsBlendFlags;
    vec4    tangent;
    float   geomRoughness;
    float   geomEmission;
    float   geomMetallicity;
};

struct ShPayload
{
    vec2    baryCoords;
    uint    instIdAndIndex;
    uint    geomAndPrimIndex;
};

struct ShPayloadShadow
{
    uint    isShadowed;
};

struct ShHitInfo
{
    vec3    albedo;
    float   metallic;
    vec3    normal;
    float   roughness;
    vec3    normalGeom;
    float   linearDepth;
    vec3    hitPosition;
    uint    instCustomIndex;
    float   emission;
};