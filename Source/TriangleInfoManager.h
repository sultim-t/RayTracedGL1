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

#include "RTGL1/RTGL1.h"
#include "AutoBuffer.h"
#include "SectorVisibility.h"

namespace RTGL1
{

class TriangleInfoManager
{
public:
    TriangleInfoManager(VkDevice device, std::shared_ptr<MemoryAllocator> &allocator, std::shared_ptr<SectorVisibility> sectorVisibility);
    ~TriangleInfoManager();

    TriangleInfoManager(const TriangleInfoManager &other) = delete;
    TriangleInfoManager(TriangleInfoManager &&other) noexcept = delete;
    TriangleInfoManager &operator=(const TriangleInfoManager &other) = delete;
    TriangleInfoManager &operator=(TriangleInfoManager &&other) noexcept = delete;

    void PrepareForFrame(uint32_t frameIndex);
    void Reset();

    uint32_t UploadAndGetArrayIndex(uint32_t frameIndex, const uint32_t *pTriangleSectorIDs, uint32_t count, RgGeometryType geomType);

    bool CopyFromStaging(VkCommandBuffer cmd, uint32_t frameIndex, bool insertBarrier = true);
    VkBuffer GetBuffer() const;

private:
    std::vector<SectorArrayIndex::index_t> &TransformIdsToIndices(const uint32_t *pTriangleSectorIDs, uint32_t count);

private:
    struct Range
    {
        explicit Range(uint32_t _startIndex) : startIndex(_startIndex), count(0), locked(false) {}
        ~Range() = default;

        Range(const Range &other) = delete;
        Range(Range &&other) noexcept = delete;
        Range &operator=(const Range &other) = delete;
        Range &operator=(Range &&other) noexcept = delete;

        void Add(uint32_t amount)                { assert(!locked); count += amount; }
        void Lock()                              { locked = true; }
        void Reset(uint32_t _startIndex)         { startIndex = _startIndex; count = 0; locked = false; }
        void StartIndexingAfter(const Range &r)  { assert(count == 0 && !locked); startIndex = r.GetFirstIndexAfterRange(); }

        uint32_t GetStartIndex() const           { return startIndex; }
        uint32_t GetFirstIndexAfterRange() const { return startIndex + count; }
        uint32_t GetCount() const                { return count; }
        bool IsLocked() const                    { return locked; }

    private:
        uint32_t startIndex;
        uint32_t count;
        bool locked;
    };

private:
    VkDevice device;

    std::shared_ptr<SectorVisibility> sectorVisibility;

    std::unique_ptr<AutoBuffer> triangleSectorIndicesBuffer;
    Range staticGeometryRange;
    Range dynamicGeometryRange;
    bool copyStaticRange;


    std::vector<SectorArrayIndex::index_t> tempValues;
};

}