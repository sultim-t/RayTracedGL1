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
#include "RgException.h"

using namespace RTGL1;

constexpr uint32_t MAX_DEVICE_COUNT = 8;
static rgl::unordered_map<RgInstance, std::unique_ptr<VulkanDevice>> G_DEVICES;

static RgInstance GetNextID()
{
    return reinterpret_cast<RgInstance>(G_DEVICES.size() + 1024);
}

static VulkanDevice &GetDevice(RgInstance rgInstance)
{
    auto it = G_DEVICES.find(rgInstance); 

    if (it == G_DEVICES.end())
    {
        throw RTGL1::RgException(RG_WRONG_INSTANCE);
    }

    return *(it->second);
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
    catch (RTGL1::RgException &e) 
    { 
        TryPrintError(rgInstance, e.what()); 
        return e.GetErrorCode(); 
    } 
    return RG_SUCCESS;
}

template<typename Func, typename... Args> requires (
    std::is_same_v< std::invoke_result_t<Func, VulkanDevice, Args...>, void>
    )
static auto Call(RgInstance rgInstance, Func f, Args&&... args)
{
    try
    {
        VulkanDevice &dev = GetDevice(rgInstance);

        if (dev.IsSuspended())
        {
            return RG_SUCCESS;
        }

        (dev.*f)(std::forward<Args>(args)...);
    }
    catch (RTGL1::RgException &e) 
    {
        TryPrintError(rgInstance, e.what());
        return e.GetErrorCode();
    }
    return RG_SUCCESS;
}

template<typename Func, typename... Args> requires (
    !std::is_same_v< std::invoke_result_t<Func, VulkanDevice, Args...>, void> &&
    std::is_default_constructible_v< std::invoke_result_t<Func, VulkanDevice, Args...> >
    )
static auto Call(RgInstance rgInstance, Func f, Args&&... args)
{
    using ReturnType = std::invoke_result_t<Func, VulkanDevice, Args...>;

    try
    {
        VulkanDevice &dev = GetDevice(rgInstance);

        if (!dev.IsSuspended())
        {
            return (dev.*f)(std::forward<Args>(args)...);
        }
    }
    catch (RTGL1::RgException &e)
    {
        TryPrintError(rgInstance, e.what());
    }
    return ReturnType{};
}

RgResult rgUploadGeometry(RgInstance rgInstance, const RgGeometryUploadInfo *pUploadInfo)
{
    return Call(rgInstance, &VulkanDevice::UploadGeometry, pUploadInfo );
}

RgResult rgUpdateGeometryTransform(RgInstance rgInstance, const RgUpdateTransformInfo* pUpdateInfo)
{
    return Call(rgInstance, &VulkanDevice::UpdateGeometryTransform, pUpdateInfo);
}

RgResult rgUpdateGeometryTexCoords(RgInstance rgInstance, const RgUpdateTexCoordsInfo *pUpdateInfo)
{
    return Call(rgInstance, &VulkanDevice::UpdateGeometryTexCoords, pUpdateInfo);
}

RgResult rgUploadRasterizedGeometry(RgInstance rgInstance, const RgRasterizedGeometryUploadInfo *pUploadInfo, 
                                    const float *pViewProjection, const RgViewport *pViewport)
{
    return Call(rgInstance, &VulkanDevice::UploadRasterizedGeometry, pUploadInfo, pViewProjection, pViewport);
}

RgResult rgUploadLensFlare(RgInstance rgInstance, const RgLensFlareUploadInfo *pUploadInfo)
{
    return Call(rgInstance, &VulkanDevice::UploadLensFlare, pUploadInfo);
}

RgResult rgUploadDecal(RgInstance rgInstance, const RgDecalUploadInfo *pUploadInfo)
{
    return Call(rgInstance, &VulkanDevice::UploadDecal, pUploadInfo);
}

RgResult rgBeginStaticGeometries(RgInstance rgInstance)
{
    return Call(rgInstance, &VulkanDevice::StartNewStaticScene);
}

RgResult rgSubmitStaticGeometries(RgInstance rgInstance)
{
    return Call(rgInstance, &VulkanDevice::SubmitStaticGeometries);
}

RgResult rgUploadDirectionalLight(RgInstance rgInstance, const RgDirectionalLightUploadInfo *pUploadInfo)
{
    return Call(rgInstance, &VulkanDevice::UploadDirectionalLight, pUploadInfo);
}

RgResult rgUploadSphericalLight(RgInstance rgInstance, const RgSphericalLightUploadInfo *pUploadInfo)
{
    return Call(rgInstance, &VulkanDevice::UploadSphericalLight, pUploadInfo);
}

RgResult rgUploadSpotLight(RgInstance rgInstance, const RgSpotLightUploadInfo *pUploadInfo)
{
    return Call(rgInstance, &VulkanDevice::UploadSpotlight, pUploadInfo);
}

RgResult rgUploadPolygonalLight(RgInstance rgInstance, const RgPolygonalLightUploadInfo *pUploadInfo)
{
    return Call(rgInstance, &VulkanDevice::UploadPolygonalLight, pUploadInfo);
}

RgResult rgCreateStaticMaterial(RgInstance rgInstance, const RgStaticMaterialCreateInfo *pCreateInfo,
                               RgMaterial *pResult)
{
    *pResult = RG_NO_MATERIAL;
    return Call(rgInstance, &VulkanDevice::CreateStaticMaterial, pCreateInfo, pResult);
}

RgResult rgCreateAnimatedMaterial(RgInstance rgInstance, const RgAnimatedMaterialCreateInfo *pCreateInfo,
                                 RgMaterial *pResult)
{
    *pResult = RG_NO_MATERIAL;
    return Call(rgInstance, &VulkanDevice::CreateAnimatedMaterial, pCreateInfo, pResult);
}

RgResult rgChangeAnimatedMaterialFrame(RgInstance rgInstance, RgMaterial animatedMaterial, uint32_t frameIndex)
{
    return Call(rgInstance, &VulkanDevice::ChangeAnimatedMaterialFrame, animatedMaterial, frameIndex);
}

RgResult rgCreateDynamicMaterial(RgInstance rgInstance, const RgDynamicMaterialCreateInfo *pCreateInfo,
                                RgMaterial *pResult)
{
    *pResult = RG_NO_MATERIAL;
    return Call(rgInstance, &VulkanDevice::CreateDynamicMaterial, pCreateInfo, pResult);
}

RgResult rgUpdateDynamicMaterial(RgInstance rgInstance, const RgDynamicMaterialUpdateInfo *pUpdateInfo)
{
    return Call(rgInstance, &VulkanDevice::UpdateDynamicMaterial, pUpdateInfo);
}

RgResult rgDestroyMaterial(RgInstance rgInstance, RgMaterial material)
{
    return Call(rgInstance, &VulkanDevice::DestroyMaterial, material);
}

RgResult rgCreateCubemap(RgInstance rgInstance, const RgCubemapCreateInfo *pCreateInfo, RgCubemap *pResult)
{
    *pResult = RG_EMPTY_CUBEMAP;
    return Call(rgInstance, &VulkanDevice::CreateSkyboxCubemap, pCreateInfo, pResult);
}

RgResult rgDestroyCubemap(RgInstance rgInstance, RgCubemap cubemap)
{
    return Call(rgInstance, &VulkanDevice::DestroyCubemap, cubemap);
}

RgResult rgStartFrame(RgInstance rgInstance, const RgStartFrameInfo *pStartInfo)
{
    return Call(rgInstance, &VulkanDevice::StartFrame, pStartInfo);
}

RgResult rgDrawFrame(RgInstance rgInstance, const RgDrawFrameInfo *pDrawInfo)
{
    return Call(rgInstance, &VulkanDevice::DrawFrame, pDrawInfo);
}

RgBool32 rgIsRenderUpscaleTechniqueAvailable(RgInstance rgInstance, RgRenderUpscaleTechnique technique)
{
    return Call(rgInstance, &VulkanDevice::IsRenderUpscaleTechniqueAvailable, technique);
}


const char *rgGetResultDescription(RgResult result)
{
    return RgException::GetRgResultName(result);
}

RgResult rgSetPotentialVisibility(RgInstance rgInstance, uint32_t sectorID_A, uint32_t sectorID_B)
{
    return Call(rgInstance, &VulkanDevice::SetPotentialVisibility, RTGL1::SectorID{ sectorID_A }, RTGL1::SectorID{ sectorID_B });
}
