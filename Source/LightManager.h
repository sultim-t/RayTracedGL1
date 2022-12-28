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

#include "RTGL1/RTGL1.h"
#include "Common.h"
#include "Containers.h"
#include "AutoBuffer.h"
#include "LightDefs.h"

namespace RTGL1
{

struct ShLightEncoded;

class LightManager
{
public:
    LightManager( VkDevice device, std::shared_ptr< MemoryAllocator >& allocator );
    ~LightManager();

    LightManager( const LightManager& other )                = delete;
    LightManager( LightManager&& other ) noexcept            = delete;
    LightManager& operator=( const LightManager& other )     = delete;
    LightManager& operator=( LightManager&& other ) noexcept = delete;

    void PrepareForFrame( VkCommandBuffer cmd, uint32_t frameIndex );
    void Reset();

    uint32_t GetLightCount() const;
    uint32_t GetLightCountPrev() const;
    uint32_t DoesDirectionalLightExist() const;

    uint32_t GetLightIndexIgnoreFPVShadows( uint32_t frameIndex, uint64_t* pLightUniqueId ) const;

    void Add( uint32_t frameIndex, const RgSphericalLightUploadInfo& info );
    void Add( uint32_t frameIndex, const RgPolygonalLightUploadInfo& info );
    void Add( uint32_t frameIndex, const RgDirectionalLightUploadInfo& info );
    void Add( uint32_t frameIndex, const RgSpotLightUploadInfo& info );

    void SubmitForFrame( VkCommandBuffer cmd, uint32_t frameIndex );
    void BarrierLightGrid( VkCommandBuffer cmd, uint32_t frameIndex );

    VkDescriptorSetLayout GetDescSetLayout() const;
    VkDescriptorSet       GetDescSet( uint32_t frameIndex ) const;

private:
    LightArrayIndex GetIndex( const ShLightEncoded& encodedLight ) const;
    void            IncrementCount( const ShLightEncoded& encodedLight );

    void AddInternal( uint32_t frameIndex, uint64_t uniqueId, const ShLightEncoded& encodedLight );

    void FillMatchPrev( uint32_t        curFrameIndex,
                        LightArrayIndex lightIndexInCurFrame,
                        UniqueLightID   uniqueID );

    void CreateDescriptors();
    void UpdateDescriptors( uint32_t frameIndex );

private:
    VkDevice device;

    std::shared_ptr< AutoBuffer > lightsBuffer;
    Buffer                        lightsBuffer_Prev;
    Buffer                        initialLightsGrid[ MAX_FRAMES_IN_FLIGHT ];

    // Match light indices between current and previous frames
    std::shared_ptr< AutoBuffer > prevToCurIndex;
    std::shared_ptr< AutoBuffer > curToPrevIndex;

    rgl::unordered_map< UniqueLightID, LightArrayIndex >
        uniqueIDToArrayIndex[ MAX_FRAMES_IN_FLIGHT ];

    uint32_t regLightCount;
    uint32_t regLightCount_Prev;
    uint32_t dirLightCount;
    uint32_t dirLightCount_Prev;

    VkDescriptorSetLayout descSetLayout;
    VkDescriptorPool      descPool;
    VkDescriptorSet       descSets[ MAX_FRAMES_IN_FLIGHT ];

    bool needDescSetUpdate[ MAX_FRAMES_IN_FLIGHT ];
};

}