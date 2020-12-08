#include <assert.h>
#include <vector>
#include "VulkanDevice.h"

#define MAX_DEVICE_COUNT 8
static std::vector<VulkanDevice *> *g_Devices = nullptr;

static uint32_t GetDeviceIndex(RgInstance rgInstance)
{
    uint64_t index = (uint64_t) rgInstance;
    return index - 1;
}

static VulkanDevice *GetDevice(RgInstance rgInstance)
{
    if (g_Devices == nullptr)
    {
        return nullptr;
    }

    auto &devices = *g_Devices;

    uint32_t index = GetDeviceIndex(rgInstance);
    assert(index < devices.size());
    if (index >= devices.size())
    {
        return nullptr;
    }

    return devices[index];
}

RgResult rgCreateInstance(const RgInstanceCreateInfo *info, RgInstance *result)
{
    if (g_Devices == nullptr)
    {
        g_Devices = new std::vector<VulkanDevice *>();
    }

    auto &devices = *g_Devices;
    if (devices.size() >= MAX_DEVICE_COUNT)
    {
        *result = nullptr;
        return RgResult::RG_TOO_MANY_ISTANCES;
    }

    devices.push_back(new VulkanDevice(info));

    *result = (RgInstance)devices.size();

    return RgResult::RG_SUCCESS;
}


RgResult rgDestroyInstance(RgInstance rgInstance)
{
    if (g_Devices == nullptr)
    {
        return RgResult::RG_NULL_INSTANCE;
    }

    auto &devices = *g_Devices;

    uint32_t index = GetDeviceIndex(rgInstance);
    assert(index < devices.size());
    if (index >= devices.size())
    {
        return RgResult::RG_SUCCESS;
    }

    delete devices[index];
    devices[index] = nullptr;

    bool devicePresent = false;
    for (auto *d : devices)
    {
        if (d != nullptr)
        {
            devicePresent = true;
            break;
        }
    }

    if (!devicePresent)
    {
        delete g_Devices;
        g_Devices = nullptr;
    }

    return RgResult::RG_SUCCESS;
}

RgResult rgCreateGeometry(RgInstance rgInstance, const RgGeometryCreateInfo *createInfo, RgGeometry *result)
{
    VulkanDevice *device = GetDevice(rgInstance);
    if (device == nullptr)
    {
        return RgResult::RG_NULL_INSTANCE;
    }

    return device->CreateGeometry(createInfo, result);
}

RgResult rgUpdateGeometryTransform(RgInstance rgInstance, const RgUpdateTransformInfo* updateInfo)
{
    VulkanDevice *device = GetDevice(rgInstance);
    if (device == nullptr)
    {
        return RgResult::RG_NULL_INSTANCE;
    }

    return device->UpdateGeometryTransform(updateInfo);
}

RgResult rgUploadRasterizedGeometry(RgInstance rgInstance, RgRasterizedGeometryUploadInfo *uploadInfo)
{
    VulkanDevice *device = GetDevice(rgInstance);
    if (device == nullptr)
    {
        return RgResult::RG_NULL_INSTANCE;
    }

    return device->UploadRasterizedGeometry(uploadInfo);
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
    VulkanDevice *device = GetDevice(rgInstance);
    if (device == nullptr)
    {
        return RgResult::RG_NULL_INSTANCE;
    }

    return device->DrawFrame(frameInfo);
}
