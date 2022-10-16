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

#include "BlueNoise.h"
#include "GlobalUniform.h"
#include "ShaderManager.h"
#include "Framebuffers.h"
#include "Scene.h"
#include "CubemapManager.h"
#include "RenderCubemap.h"
#include "PortalList.h"
#include "RestirBuffers.h"
#include "Volumetric.h"

namespace RTGL1
{

class RayTracingPipeline : public IShaderDependency
{
public:
    RayTracingPipeline( VkDevice                           device,
                        std::shared_ptr< PhysicalDevice >  physDevice,
                        std::shared_ptr< MemoryAllocator > allocator,
                        const ShaderManager&               shaderManager,
                        Scene&                             scene,
                        const GlobalUniform&               uniform,
                        const TextureManager&              textureManager,
                        const Framebuffers&                framebuffers,
                        const RestirBuffers&               restirBuffers,
                        const BlueNoise&                   blueNoise,
                        const CubemapManager&              cubemapManager,
                        const RenderCubemap&               renderCubemap,
                        const PortalList&                  portalList,
                        const Volumetric&                  volumetric,
                        const RgInstanceCreateInfo&        rgInfo );
    ~RayTracingPipeline() override;

    RayTracingPipeline( const RayTracingPipeline& other )     = delete;
    RayTracingPipeline( RayTracingPipeline&& other ) noexcept = delete;
    RayTracingPipeline& operator=( const RayTracingPipeline& other ) = delete;
    RayTracingPipeline& operator=( RayTracingPipeline&& other ) noexcept = delete;

    void                Bind( VkCommandBuffer cmd );

    void                GetEntries( uint32_t                         sbtRayGenIndex,
                                    VkStridedDeviceAddressRegionKHR& raygenEntry,
                                    VkStridedDeviceAddressRegionKHR& missEntry,
                                    VkStridedDeviceAddressRegionKHR& hitEntry,
                                    VkStridedDeviceAddressRegionKHR& callableEntry ) const;
    VkPipelineLayout    GetLayout() const;

    void                OnShaderReload( const ShaderManager* shaderManager ) override;

private:
    void CreatePipeline( const ShaderManager* shaderManager );
    void DestroyPipeline();
    void CreateSBT();
    void DestroySBT();

    void AddGeneralGroup( uint32_t generalIndex );

    void AddRayGenGroup( uint32_t raygenIndex );
    void AddMissGroup( uint32_t missIndex );
    void AddHitGroup( uint32_t closestHitIndex );
    void AddHitGroupOnlyAny( uint32_t anyHitIndex );
    void AddHitGroup( uint32_t closestHitIndex, uint32_t anyHitIndex );
    void AddHitGroup( uint32_t closestHitIndex, uint32_t anyHitIndex, uint32_t intersectionIndex );

private:
    struct ShaderStageInfo
    {
        const char* pName;
        // One uint specialization const to use in the shader.
        // Must exist throughout class lifetime,
        // as CreatePipeline can be called any time.
        std::unique_ptr< uint32_t > specConst;
    };

private:
    VkDevice                                            device;
    std::shared_ptr< PhysicalDevice >                   physDevice;

    std::vector< ShaderStageInfo >                      shaderStageInfos;

    std::vector< VkRayTracingShaderGroupCreateInfoKHR > shaderGroups;
    VkPipelineLayout                                    rtPipelineLayout;
    VkPipeline                                          rtPipeline;

    std::shared_ptr< AutoBuffer >                       shaderBindingTable;
    bool                                                copySBTFromStaging;

    uint32_t                                            groupBaseAlignment;
    uint32_t                                            handleSize;
    uint32_t                                            alignedHandleSize;

    uint32_t                                            raygenShaderCount;
    uint32_t                                            hitGroupCount;
    uint32_t                                            missShaderCount;
};

}
