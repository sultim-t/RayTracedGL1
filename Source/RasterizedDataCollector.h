#pragma once

#include <array>
#include <vector>

#include <RTGL1/RTGL1.h>
#include "Buffer.h"
#include "Common.h"

class RasterizedDataCollector
{
public:
    struct DrawInfo
    {
        uint32_t vertexCount;
        uint32_t firstVertex;
        uint32_t indexCount;
        uint32_t firstIndex;
    };

public:
    explicit RasterizedDataCollector(
        VkDevice device, 
        const std::shared_ptr<PhysicalDevice> &physDevice,
        uint32_t maxVertexCount, uint32_t maxIndexCount);
    ~RasterizedDataCollector();

    RasterizedDataCollector(const RasterizedDataCollector& other) = delete;
    RasterizedDataCollector(RasterizedDataCollector&& other) noexcept = delete;
    RasterizedDataCollector& operator=(const RasterizedDataCollector& other) = delete;
    RasterizedDataCollector& operator=(RasterizedDataCollector&& other) noexcept = delete;

    void AddGeometry(const RgRasterizedGeometryUploadInfo &info);
    void Clear();

    VkBuffer GetVertexBuffer() const;
    VkBuffer GetIndexBuffer() const;
    const std::vector<DrawInfo> &GetDrawInfos() const;

    static uint32_t GetVertexStride();
    static void GetVertexLayout(std::array<VkVertexInputAttributeDescription, 4> &attrs);

private:
    struct RasterizerVertex;

private:
    VkDevice device;

    Buffer vertexBuffer;
    Buffer indexBuffer;

    RasterizerVertex *mappedVertexData;
    uint32_t *mappedIndexData;

    uint32_t curVertexCount;
    uint32_t curIndexCount;

    std::vector<DrawInfo> drawInfos;
};