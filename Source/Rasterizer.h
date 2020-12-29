#pragma once
#include "Common.h"

// This class provides rasterization functionality
class Rasterizer
{
public:
    explicit Rasterizer(VkDevice device);

    Rasterizer(const Rasterizer& other) = delete;
    Rasterizer(Rasterizer&& other) noexcept = delete;
    Rasterizer& operator=(const Rasterizer& other) = delete;
    Rasterizer& operator=(Rasterizer&& other) noexcept = delete;

    void Upload(const RgRasterizedGeometryUploadInfo &uploadInfo) {}
    void Draw(VkCommandBuffer cmd) {}

private:

};