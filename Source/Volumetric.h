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

#include "BlueNoise.h"
#include "CommandBufferManager.h"
#include "GlobalUniform.h"
#include "IShaderDependency.h"
#include "MemoryAllocator.h"

namespace RTGL1
{
    class Volumetric : public IShaderDependency
    {
    public:
        Volumetric( VkDevice              device,
                    CommandBufferManager* cmdManager,
                    MemoryAllocator*      allocator,
                    const ShaderManager*  shaderManager,
                    const GlobalUniform*  uniform,
                    const BlueNoise*      rnd );
        ~Volumetric() override;

        Volumetric( const Volumetric& other )     = delete;
        Volumetric( Volumetric&& other ) noexcept = delete;
        Volumetric& operator=( const Volumetric& other ) = delete;
        Volumetric& operator=( Volumetric&& other ) noexcept = delete;

        VkDescriptorSetLayout GetDescSetLayout() const;
        VkDescriptorSet GetDescSet(uint32_t frameIndex) const;

        void Process( VkCommandBuffer      cmd,
                      uint32_t             frameIndex,
                      const GlobalUniform* uniform,
                      const BlueNoise*     rnd );
        void BarrierToReadProcessed( VkCommandBuffer cmd, uint32_t frameIndex );

        void OnShaderReload( const ShaderManager* shaderManager ) override;

    private:
        void CreateSampler();
        void CreateImages( CommandBufferManager* cmdManager, MemoryAllocator* allocator );

        void CreateDescriptors();
        void UpdateDescriptors();

        void CreatePipelineLayout( const GlobalUniform* uniform, const BlueNoise* rnd );
        void CreatePipelines( const ShaderManager* shaderManager );
        void DestroyPipelines();

    private:
        VkDevice device = VK_NULL_HANDLE;

        VkImage               volumeImages[ MAX_FRAMES_IN_FLIGHT ] = {};
        VkDeviceMemory        volumeMemory[ MAX_FRAMES_IN_FLIGHT ] = {};
        VkImageView           volumeViews[ MAX_FRAMES_IN_FLIGHT ]  = {};
        VkSampler             volumeSampler                        = VK_NULL_HANDLE;
        VkDescriptorPool      descPool                             = VK_NULL_HANDLE;
        VkDescriptorSetLayout descLayout                           = VK_NULL_HANDLE;
        VkDescriptorSet       descSets[ MAX_FRAMES_IN_FLIGHT ]     = {};

        VkPipelineLayout processPipelineLayout = VK_NULL_HANDLE;
        VkPipeline       processPipeline       = VK_NULL_HANDLE;
    };
}
