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

#include "ASManager.h"
#include "GlobalUniform.h"
#include "ShaderManager.h"

class RayTracingPipeline
{
public:
    // TODO: remove imagesSetLayout
    explicit RayTracingPipeline(
        VkDevice device,
        const std::shared_ptr<PhysicalDevice> &physDevice,
        const std::shared_ptr<ShaderManager> &shaderManager,
        const std::shared_ptr<ASManager> &asManager,
        const std::shared_ptr<GlobalUniform> &uniform,
        const std::shared_ptr<TextureManager> &textureMgr,
        VkDescriptorSetLayout imagesSetLayout
    );
    ~RayTracingPipeline();

    RayTracingPipeline(const RayTracingPipeline& other) = delete;
    RayTracingPipeline(RayTracingPipeline&& other) noexcept = delete;
    RayTracingPipeline& operator=(const RayTracingPipeline& other) = delete;
    RayTracingPipeline& operator=(RayTracingPipeline&& other) noexcept = delete;

    void Bind(VkCommandBuffer cmd);

    void GetEntries(VkStridedDeviceAddressRegionKHR &raygenEntry,
                    VkStridedDeviceAddressRegionKHR &missEntry,
                    VkStridedDeviceAddressRegionKHR &hitEntry,
                    VkStridedDeviceAddressRegionKHR &callableEntry) const;
    VkPipelineLayout GetLayout() const;

private:
    void CreateSBT(const std::shared_ptr<PhysicalDevice> &physDevice);

    void AddGeneralGroup(uint32_t generalIndex);
    void AddHitGroup(uint32_t closestHitIndex);
    void AddHitGroup(uint32_t closestHitIndex, uint32_t anyHitIndex);
    void AddHitGroup(uint32_t closestHitIndex, uint32_t anyHitIndex, uint32_t intersectionIndex);

private:
    VkDevice device;

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
    VkPipelineLayout rtPipelineLayout;
    VkPipeline rtPipeline;

    std::shared_ptr<Buffer> shaderBindingTable;

    uint32_t groupBaseAlignment;
    uint32_t handleSize;
    uint32_t alignedHandleSize;
};
