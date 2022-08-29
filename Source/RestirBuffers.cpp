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

#include "RestirBuffers.h"

#include "Utils.h"
#include "Generated/ShaderCommonC.h"

RTGL1::RestirBuffers::RestirBuffers(VkDevice _device, std::shared_ptr<MemoryAllocator> _allocator)
    : device(_device)
    , allocator(std::move(_allocator))
{
    CreateDescriptors();
}

RTGL1::RestirBuffers::~RestirBuffers()
{
    DestroyBuffers();
    vkDestroyDescriptorSetLayout(device, descLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);
}

VkDescriptorSet RTGL1::RestirBuffers::GetDescSet(uint32_t frameIndex) const
{
    return descSets[frameIndex];
}

VkDescriptorSetLayout RTGL1::RestirBuffers::GetDescSetLayout() const
{
    return descLayout;
}

void RTGL1::RestirBuffers::OnFramebuffersSizeChange(const ResolutionState& resolutionState)
{
    DestroyBuffers();
    CreateBuffers(resolutionState.renderWidth, resolutionState.renderHeight);
}

namespace 
{
    auto MakeBuffer(const std::shared_ptr<RTGL1::MemoryAllocator> &allocator, VkDeviceSize size, const char *name)
    {
        using namespace RTGL1;
        RestirBuffers::BufferDef result = {};

        VkBufferCreateInfo bufferInfo =
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult r = vkCreateBuffer(allocator->GetDevice(), &bufferInfo, nullptr, &result.buffer);
        VK_CHECKERROR(r);
        SET_DEBUG_NAME(allocator->GetDevice(), result.buffer, VK_OBJECT_TYPE_BUFFER, name);

        VkMemoryRequirements memReq = {};
        vkGetBufferMemoryRequirements(allocator->GetDevice(), result.buffer, &memReq);

        result.memory = allocator->AllocDedicated(memReq, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, MemoryAllocator::AllocType::DEFAULT, name);

        return result;
    }
}

void RTGL1::RestirBuffers::CreateBuffers(uint32_t renderWidth, uint32_t renderHeight)
{
    initialSamples = MakeBuffer(allocator, sizeof(uint32_t) * PACKED_INDIRECT_SAMPLE_SIZE_IN_WORDS * renderWidth * renderHeight, "Restir Indirect - Initial");

    for (auto &r : reservoirs)
    {
        r = MakeBuffer(allocator, sizeof(uint32_t) * PACKED_INDIRECT_RESERVOIR_SIZE_IN_WORDS * renderWidth * renderHeight, "Restir Indirect - Reservois");
    }
}

void RTGL1::RestirBuffers::DestroyBuffers()
{
    BufferDef* allBufs[] =
    {
        &initialSamples,
        &reservoirs[0],
        &reservoirs[1],
    };
    static_assert(sizeof(reservoirs) / sizeof(reservoirs[0]) == 2);

    for (auto* b : allBufs)
    {
        if (b->buffer)
        {
            vkDestroyBuffer(device, b->buffer, nullptr);
        }

        if (b->memory)
        {
            MemoryAllocator::FreeDedicated(device, b->memory);
        }

        *b = {};
    }
}

void RTGL1::RestirBuffers::CreateDescriptors()
{
    VkResult r;

    VkDescriptorSetLayoutBinding bindings[] =
    {
    {
            .binding = BINDING_RESTIR_INDIRECT_INITIAL_SAMPLES,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = BINDING_RESTIR_INDIRECT_RESERVOIRS,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
        },
        {
            .binding = BINDING_RESTIR_INDIRECT_RESERVOIRS_PREV,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
        },
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = std::size(bindings),
        .pBindings = bindings,
    };

    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descLayout);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Restir Indirect Desc set layout");

    VkDescriptorPoolSize poolSize = 
    {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = std::size(bindings) * MAX_FRAMES_IN_FLIGHT,
    };

    VkDescriptorPoolCreateInfo poolInfo = 
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    SET_DEBUG_NAME(device, descPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Restir Indirect Desc pool");

    for (auto& d : descSets)
    {
        VkDescriptorSetAllocateInfo allocInfo = 
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descLayout,
        };

        r = vkAllocateDescriptorSets(device, &allocInfo, &d);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, d, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Restir Indirect Desc set");
    }
}

void RTGL1::RestirBuffers::UpdateDescriptors()
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        VkBuffer bufs[] =
        {
            initialSamples.buffer,
            reservoirs[i].buffer,
            reservoirs[Utils::GetPreviousByModulo(i, MAX_FRAMES_IN_FLIGHT)].buffer,
        };
        uint32_t bnds[] =
        {
            BINDING_RESTIR_INDIRECT_INITIAL_SAMPLES,
            BINDING_RESTIR_INDIRECT_RESERVOIRS,
            BINDING_RESTIR_INDIRECT_RESERVOIRS_PREV,
        };
        static_assert(std::size(bufs) == std::size(bnds));

        VkDescriptorBufferInfo bufInfo[std::size(bufs)] = {};
        VkWriteDescriptorSet wrts[std::size(bufs)] = {};

        for (int k = 0; k < static_cast<int>(std::size(bufs)); k++)
        {
            bufInfo[k] =
            {
                .buffer = bufs[k],
                .offset = 0,
                .range = VK_WHOLE_SIZE,
            };

            wrts[k] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descSets[i],
                .dstBinding = bnds[k],
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &bufInfo[k],
            };
        }

        vkUpdateDescriptorSets(device, std::size(wrts), wrts, 0, nullptr);
    }
}
