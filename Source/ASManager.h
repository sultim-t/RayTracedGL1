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

#pragma once

#include "ASBuilder.h"
#include "CommandBufferManager.h"
#include "ScratchBuffer.h"
#include "TextureManager.h"
#include "VertexBufferProperties.h"
#include "VertexCollector.h"

class ASManager
{
public:
    ASManager(VkDevice device, std::shared_ptr<MemoryAllocator> allocator,
              std::shared_ptr<CommandBufferManager> cmdManager,
              std::shared_ptr<TextureManager> textureMgr,
              const VertexBufferProperties &properties);
    ~ASManager();

    ASManager(const ASManager& other) = delete;
    ASManager(ASManager&& other) noexcept = delete;
    ASManager& operator=(const ASManager& other) = delete;
    ASManager& operator=(ASManager&& other) noexcept = delete;

    // Static geometry recording is frameIndex-agnostic
    void BeginStaticGeometry();
    uint32_t AddStaticGeometry(const RgGeometryUploadInfo &info);
    // Submitting static geometry to the building is a heavy operation
    // with waiting for it to complete.
    void SubmitStaticGeometry();
    // If all the added geometries must be removed, call this function before submitting
    void ResetStaticGeometry();

    void BeginDynamicGeometry(uint32_t frameIndex);
    uint32_t AddDynamicGeometry(const RgGeometryUploadInfo &info, uint32_t frameIndex);
    void SubmitDynamicGeometry(VkCommandBuffer cmd, uint32_t frameIndex);

    // Update transform for static movable geometry
    void UpdateStaticMovableTransform(uint32_t geomIndex, const RgTransform &transform);
    // After updating transforms, acceleration structures should be rebuilt
    void ResubmitStaticMovable(VkCommandBuffer cmd);

    bool TryBuildTLAS(VkCommandBuffer cmd, uint32_t frameIndex);
    VkDescriptorSet GetBuffersDescSet(uint32_t frameIndex) const;
    VkDescriptorSet GetTLASDescSet(uint32_t frameIndex) const;

    VkDescriptorSetLayout GetBuffersDescSetLayout() const;
    VkDescriptorSetLayout GetTLASDescSetLayout() const;

private:
    struct AccelerationStructure
    {
        VkAccelerationStructureKHR as = VK_NULL_HANDLE;
        Buffer buffer = {};
    };

private:
    void CreateDescriptors();
    void UpdateBufferDescriptors(uint32_t frameIndex);
    void UpdateASDescriptors(uint32_t frameIndex);

    void SetupBLAS(
        AccelerationStructure &as,
        const std::vector<VkAccelerationStructureGeometryKHR> &geoms,
        const std::vector<VkAccelerationStructureBuildRangeInfoKHR> &ranges,
        const std::vector<uint32_t> &primCounts);
    void CreateASBuffer(AccelerationStructure &as, VkDeviceSize size);
    void DestroyAS(AccelerationStructure &as, bool withBuffer = true);
    VkDeviceAddress GetASAddress(const AccelerationStructure &as);
    VkDeviceAddress GetASAddress(VkAccelerationStructureKHR as);

private:
    VkDevice device;
    std::shared_ptr<MemoryAllocator> allocator;

    VkFence staticCopyFence;

    // for filling buffers
    std::shared_ptr<VertexCollector> collectorStatic;
    std::shared_ptr<VertexCollector> collectorDynamic[MAX_FRAMES_IN_FLIGHT];

    // building
    std::shared_ptr<ScratchBuffer> scratchBuffer;
    std::shared_ptr<ASBuilder> asBuilder;

    std::shared_ptr<CommandBufferManager> cmdManager;
    std::shared_ptr<TextureManager> textureMgr;

    AccelerationStructure staticBlas;
    AccelerationStructure staticMovableBlas;
    AccelerationStructure dynamicBlas[MAX_FRAMES_IN_FLIGHT];

    // top level AS
    Buffer instanceBuffers[MAX_FRAMES_IN_FLIGHT];
    AccelerationStructure tlas[MAX_FRAMES_IN_FLIGHT];

    // TLAS and buffer descriptors
    VkDescriptorPool descPool;

    VkDescriptorSetLayout buffersDescSetLayout;
    VkDescriptorSet buffersDescSets[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorSetLayout asDescSetLayout;
    VkDescriptorSet asDescSets[MAX_FRAMES_IN_FLIGHT];

    VertexBufferProperties properties;
};
