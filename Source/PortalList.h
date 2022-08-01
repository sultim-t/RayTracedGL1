// Copyright (c) 2022 Sultim Tsyrendashiev
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

#include <bitset>

#include "RTGL1/RTGL1.h"

#include "AutoBuffer.h"
#include "Const.h"

namespace RTGL1
{
    class PortalList
    {
    public:
        using PortalID = decltype(RgPortalUploadInfo::outUp);

    public:
        PortalList(VkDevice device, std::shared_ptr<MemoryAllocator> allocator);
        ~PortalList();

        PortalList(const PortalList &other) = delete;
        PortalList(PortalList &&other) noexcept = delete;
        PortalList &operator=(const PortalList &other) = delete;
        PortalList &operator=(PortalList &&other) noexcept = delete;

        void Upload(uint32_t frameIndex, const RgPortalUploadInfo &info);
        void SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex);

        VkDescriptorSet GetDescSet(uint32_t frameIndex) const;
        VkDescriptorSetLayout GetDescSetLayout() const;

    private:
        void CreateDescriptors();

    private:
        VkDevice device;
        std::shared_ptr<AutoBuffer> buffer;

        VkDescriptorPool        descPool;
        VkDescriptorSetLayout   descSetLayout;
        VkDescriptorSet         descSet;

        std::bitset<MAX_PORTALS> uploadedIndices;
    };
}
