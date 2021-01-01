#pragma once

#include "ASManager.h"

class Scene
{
public:
    explicit Scene(std::shared_ptr<ASManager> asManager);
    ~Scene();

    Scene(const Scene& other) = delete;
    Scene(Scene&& other) noexcept = delete;
    Scene& operator=(const Scene& other) = delete;
    Scene& operator=(Scene&& other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);
    // Return true if TLAS was built
    bool SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex);

    uint32_t Upload(const RgGeometryUploadInfo &uploadInfo);
    void UpdateTransform(uint32_t geomId, const RgTransform &transform);

    void SubmitStatic();
    void StartNewStatic();

    std::shared_ptr<ASManager> &GetASManager();

private:
    std::shared_ptr<ASManager> asManager;

    std::vector<uint32_t> movableGeomIds;
    bool toResubmitMovable;

    uint32_t currentFrameIndex;
    bool isRecordingStatic;
};