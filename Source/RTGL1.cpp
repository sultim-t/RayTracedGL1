#include "VulkanDevice.h"


#define MAX_DEVICE_COUNT 8
static std::map<RgInstance, std::shared_ptr<VulkanDevice>> g_Devices;


#define CHECK_WRONG_INSTANCE_AND_GET \
    auto &iterDevice = g_Devices.find(rgInstance); \
    if (iterDevice == g_Devices.end()) { return RG_WRONG_INSTANCE; } \
    iterDevice->second


#define CHECK_WRONG_INSTANCE_AND_RETURN_GET \
    auto &iterDevice = g_Devices.find(rgInstance); \
    if (iterDevice == g_Devices.end()) { return RG_WRONG_INSTANCE; } \
    return iterDevice->second


RgResult rgCreateInstance(const RgInstanceCreateInfo *info, RgInstance *result)
{
    if (g_Devices.size() >= MAX_DEVICE_COUNT)
    {
        *result = nullptr;
        return RgResult::RG_TOO_MANY_ISTANCES;
    }

    // insert new
    const RgInstance id = reinterpret_cast<RgInstance>(g_Devices.size() + 1);
    g_Devices[id] = std::make_shared<VulkanDevice>(info);

    *result = id;

    return RgResult::RG_SUCCESS;
}


RgResult rgDestroyInstance(RgInstance rgInstance)
{
    CHECK_WRONG_INSTANCE_AND_GET.reset();
    return RgResult::RG_SUCCESS;
}

RgResult rgCreateGeometry(RgInstance rgInstance, const RgGeometryCreateInfo *createInfo, RgGeometry *result)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->CreateGeometry(createInfo, result);
}

RgResult rgUpdateGeometryTransform(RgInstance rgInstance, const RgUpdateTransformInfo* updateInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UpdateGeometryTransform(updateInfo);
}

RgResult rgUploadRasterizedGeometry(RgInstance rgInstance, RgRasterizedGeometryUploadInfo *uploadInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->UploadRasterizedGeometry(uploadInfo);
}

RgResult rgCreateStaticTexture(RgInstance rgInstance, const RgStaticTextureCreateInfo* createInfo,
    RgStaticTexture* result)
{
    assert(0);
    return RgResult();
}

RgResult rgCreateAnimatedTexture(RgInstance rgInstance, const RgAnimatedTextureCreateInfo* createInfo,
    RgAnimatedTexture* result)
{
    assert(0);
    return RgResult();
}

RgResult rgChangeAnimatedTextureFrame(RgAnimatedTexture animatedTexture, uint32_t frameIndex)
{
    assert(0);
    return RgResult();
}

RgResult rgCreateDynamicTexture(RgInstance rgInstance, const RgDynamicTextureInfo* dynamicTextureInfo,
    RgDynamicTexture* result)
{
    assert(0);
    return RgResult();
}

RgResult rgUpdateDynamicTexture(RgInstance rgInstance, RgDynamicTexture dynamicTexture,
    const RgDynamicTextureInfo* updateInfo)
{
    assert(0);
    return RgResult();
}

RgResult rgDrawFrame(RgInstance rgInstance, const RgDrawFrameInfo* frameInfo)
{
    CHECK_WRONG_INSTANCE_AND_RETURN_GET->DrawFrame(frameInfo);
}
