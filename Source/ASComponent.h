// Copyright (c) 2021 Sultim Tsyrendashiev
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

#include <vector>

#include "Common.h"
#include "Buffer.h"
#include "VertexCollectorFilterType.h"

namespace RTGL1
{

struct ASComponent
{
protected:
    explicit ASComponent(VkDevice device, const char *debugName);

public:
    virtual ~ASComponent() = 0;
    void Destroy();

    ASComponent(const ASComponent &other) = delete;
    ASComponent &operator=(const ASComponent &other) = delete;
    ASComponent &operator=(ASComponent &&other) noexcept = delete;

    void RegisterGeometries(const std::vector<VkAccelerationStructureGeometryKHR> &geoms);

    void RecreateIfNotValid(
        const VkAccelerationStructureBuildSizesInfoKHR &buildSizes, 
        const std::shared_ptr<MemoryAllocator> &allocator);

    VkAccelerationStructureKHR GetAS() const;
    VkDeviceAddress GetASAddress() const;

    bool IsValid(const VkAccelerationStructureBuildSizesInfoKHR &buildSizes) const;
    bool IsEmpty() const;

protected:
    virtual void CreateAS(VkDeviceSize size) = 0;
    virtual const char *GetBufferDebugName() const = 0;

private:
    void CreateBuffer(
        const std::shared_ptr<MemoryAllocator> &allocator,
        VkDeviceSize size);

    VkDeviceAddress GetASAddress(VkAccelerationStructureKHR as) const;

protected:
    VkDevice device;

    Buffer buffer;
    VkAccelerationStructureKHR as;

    bool isEmpty;
    const char *debugName;
};


struct BLASComponent : public ASComponent
{
public:
    explicit BLASComponent(VkDevice device, VertexCollectorFilterTypeFlags filter);
    VertexCollectorFilterTypeFlags GetFilter() const;

protected:
    void CreateAS(VkDeviceSize size) override;
    const char *GetBufferDebugName() const override;

private:
    VertexCollectorFilterTypeFlags filter;
};


struct TLASComponent : public ASComponent
{
public:
    explicit TLASComponent(VkDevice device, const char *debugName = nullptr);

protected:
    void CreateAS(VkDeviceSize size) override;
    const char *GetBufferDebugName() const override;
};

}