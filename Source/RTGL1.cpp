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

#include <unordered_map>

#include "VulkanDevice.h"
#include "RgException.h"

using namespace RTGL1;



#define CATCH_OR_RETURN \
    catch (RTGL1::RgException &e) \
    { \
        TryPrintError(rgInstance, e.what()); \
        return e.GetErrorCode(); \
    } \
    return RG_SUCCESS \




constexpr uint32_t MAX_DEVICE_COUNT = 8;
static std::unordered_map<RgInstance, std::unique_ptr<VulkanDevice>> G_DEVICES;

static RgInstance GetNextID()
{
    return reinterpret_cast<RgInstance>(G_DEVICES.size() + 1024);
}

static const std::unique_ptr<VulkanDevice> &GetDevice(RgInstance rgInstance)
{
    auto it = G_DEVICES.find(rgInstance); 

    if (it == G_DEVICES.end())
    {
        throw RTGL1::RgException(RG_WRONG_INSTANCE);
    }

    return it->second;
}

static void TryPrintError(RgInstance rgInstance, const char *pMessage)
{
    auto it = G_DEVICES.find(rgInstance);

    if (it != G_DEVICES.end())
    {
        it->second->Print(pMessage);
    }
}



RgResult rgCreateInstance(const RgInstanceCreateInfo *pInfo, RgInstance *pResult)
{
    *pResult = nullptr;

    if (G_DEVICES.size() >= MAX_DEVICE_COUNT)
    {
        return RG_TOO_MANY_INSTANCES;
    }

    // insert new
    const RgInstance rgInstance = GetNextID();
    assert(G_DEVICES.find(rgInstance) == G_DEVICES.end());

    try
    {
        G_DEVICES[rgInstance] = std::make_unique<VulkanDevice>(pInfo);
        *pResult = rgInstance;
    }
    // TODO: VulkanDevice must clean all the resources if initialization failed!
    // So for now exceptions should not happen. But if they did, target application must be closed.
    catch (RTGL1::RgException &e) 
    { 
        // UserPrint class probably wasn't initialized, print manually
        if (pInfo->pfnPrint != nullptr)
        {
            pInfo->pfnPrint(e.what(), pInfo->pUserPrintData);
        }

        return e.GetErrorCode(); 
    } 
    return RG_SUCCESS;
}

RgResult rgDestroyInstance(RgInstance rgInstance)
{
    if (G_DEVICES.find(rgInstance) == G_DEVICES.end())
    {
        return RG_WRONG_INSTANCE;
    }

    try
    {
        G_DEVICES.erase(rgInstance);
    }
    CATCH_OR_RETURN;
}

RgResult rgUploadGeometry(RgInstance rgInstance, const RgGeometryUploadInfo *pUploadInfo)
{
    try
    {
        GetDevice(rgInstance)->UploadGeometry(pUploadInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgUpdateGeometryTransform(RgInstance rgInstance, const RgUpdateTransformInfo* pUpdateInfo)
{
    try
    {
        GetDevice(rgInstance)->UpdateGeometryTransform(pUpdateInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgUpdateGeometryTexCoords(RgInstance rgInstance, const RgUpdateTexCoordsInfo *pUpdateInfo)
{
    try
    {
        GetDevice(rgInstance)->UpdateGeometryTexCoords(pUpdateInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgUploadRasterizedGeometry(RgInstance rgInstance, const RgRasterizedGeometryUploadInfo *pUploadInfo, 
                                    const float *pViewProjection, const RgViewport *pViewport)
{
    try
    {
        GetDevice(rgInstance)->UploadRasterizedGeometry(pUploadInfo, pViewProjection, pViewport);
    }
    CATCH_OR_RETURN;
}

RgResult rgUploadLensFlare(RgInstance rgInstance, const RgLensFlareUploadInfo *pUploadInfo)
{
    try
    {
        GetDevice(rgInstance)->UploadLensFlare(pUploadInfo);
    }
    CATCH_OR_RETURN;
}


RgResult rgSubmitStaticGeometries(RgInstance rgInstance)
{
    try
    {
        GetDevice(rgInstance)->SubmitStaticGeometries();
    }
    CATCH_OR_RETURN;
}

RgResult rgStartNewScene(RgInstance rgInstance)
{
    try
    {
        GetDevice(rgInstance)->StartNewStaticScene();
    }
    CATCH_OR_RETURN;
}

RgResult rgUploadDirectionalLight(RgInstance rgInstance, RgDirectionalLightUploadInfo *pLightInfo)
{
    try
    {
        GetDevice(rgInstance)->UploadLight(pLightInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgUploadSphericalLight(RgInstance rgInstance, RgSphericalLightUploadInfo *pLightInfo)
{
    try
    {
        GetDevice(rgInstance)->UploadLight(pLightInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgUploadSpotlightLight(RgInstance rgInstance, RgSpotlightUploadInfo *pLightInfo)
{
    try
    {
        GetDevice(rgInstance)->UploadLight(pLightInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgUploadPolygonalLight(RgInstance rgInstance, RgPolygonalLightUploadInfo *pLightInfo)
{
    try
    {
        GetDevice(rgInstance)->UploadLight(pLightInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgCreateStaticMaterial(RgInstance rgInstance, const RgStaticMaterialCreateInfo *pCreateInfo,
                               RgMaterial *pResult)
{
    try
    {
        GetDevice(rgInstance)->CreateStaticMaterial(pCreateInfo, pResult);
    }
    CATCH_OR_RETURN;
}

RgResult rgCreateAnimatedMaterial(RgInstance rgInstance, const RgAnimatedMaterialCreateInfo *pCreateInfo,
                                 RgMaterial *pResult)
{
    try
    {
        GetDevice(rgInstance)->CreateAnimatedMaterial(pCreateInfo, pResult);
    }
    CATCH_OR_RETURN;
}

RgResult rgChangeAnimatedMaterialFrame(RgInstance rgInstance, RgMaterial animatedMaterial, uint32_t frameIndex)
{
    try
    {
        GetDevice(rgInstance)->ChangeAnimatedMaterialFrame(animatedMaterial, frameIndex);
    }
    CATCH_OR_RETURN;
}

RgResult rgCreateDynamicMaterial(RgInstance rgInstance, const RgDynamicMaterialCreateInfo *pCreateInfo,
                                RgMaterial *pResult)
{
    try
    {
        GetDevice(rgInstance)->CreateDynamicMaterial(pCreateInfo, pResult);
    }
    CATCH_OR_RETURN;
}

RgResult rgUpdateDynamicMaterial(RgInstance rgInstance, const RgDynamicMaterialUpdateInfo *pUpdateInfo)
{
    try
    {
        GetDevice(rgInstance)->UpdateDynamicMaterial(pUpdateInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgDestroyMaterial(RgInstance rgInstance, RgMaterial material)
{
    try
    {
        GetDevice(rgInstance)->DestroyMaterial(material);
    }
    CATCH_OR_RETURN;
}

RgResult rgCreateCubemap(RgInstance rgInstance, const RgCubemapCreateInfo *pCreateInfo, RgCubemap *pResult)
{
    try
    {
        GetDevice(rgInstance)->CreateSkyboxCubemap(pCreateInfo, pResult);
    }
    CATCH_OR_RETURN;
}

RgResult rgDestroyCubemap(RgInstance rgInstance, RgCubemap cubemap)
{
    try
    {
        GetDevice(rgInstance)->DestroyCubemap(cubemap);
    }
    CATCH_OR_RETURN;
}

RgResult rgStartFrame(RgInstance rgInstance, const RgStartFrameInfo *pStartInfo)
{
    try
    {
        GetDevice(rgInstance)->StartFrame(pStartInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgDrawFrame(RgInstance rgInstance, const RgDrawFrameInfo *pDrawInfo)
{
    try
    {
        GetDevice(rgInstance)->DrawFrame(pDrawInfo);
    }
    CATCH_OR_RETURN;
}

RgResult rgIsRenderUpscaleTechniqueAvailable(RgInstance rgInstance, RgRenderUpscaleTechnique technique, RgBool32 *pOutResult)
{
    try
    {
        *pOutResult = GetDevice(rgInstance)->IsRenderUpscaleTechniqueAvailable(technique);
    }
    CATCH_OR_RETURN;
}

RgResult rgSetPotentialVisibility(RgInstance rgInstance, uint32_t sectorID_A, uint32_t sectorID_B)
{
    try
    {
        GetDevice(rgInstance)->SetPotentialVisibility(RTGL1::SectorID{ sectorID_A }, RTGL1::SectorID{ sectorID_B });
    }
    CATCH_OR_RETURN;
}
