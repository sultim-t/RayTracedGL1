#pragma once

#include "ASManager.h"

class Scene
{
public:
    explicit Scene(std::shared_ptr<ASManager> asManager);

    Scene(const Scene& other) = delete;
    Scene(Scene&& other) noexcept = delete;
    Scene& operator=(const Scene& other) = delete;
    Scene& operator=(Scene&& other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);
    void SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex);

    uint32_t Upload(const RgGeometryUploadInfo &uploadInfo);
    void UpdateTransform(uint32_t geomId, const RgTransform &transform);

    void SubmitStatic();
    void ClearStatic();

    std::shared_ptr<ASManager> &GetASManager();

private:
    std::shared_ptr<ASManager> asManager;

    uint32_t currentFrameIndex;
};