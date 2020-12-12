#pragma once

#include "Common.h"
#include "ASManager.h"

class Scene
{
public:
    explicit Scene();

    Scene(const Scene& other) = delete;
    Scene(Scene&& other) noexcept = delete;
    Scene& operator=(const Scene& other) = delete;
    Scene& operator=(Scene&& other) noexcept = delete;

    uint32_t Upload(const RgGeometryUploadInfo &uploadInfo);
    void UpdateTransform(uint32_t geomId, const RgTransform &transform);


private:
    std::shared_ptr<ASManager> asManager;
};