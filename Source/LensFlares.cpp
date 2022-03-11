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

#include "LensFlares.h"

#include "Utils.h"
#include "Generated/ShaderCommonC.h"


constexpr VkDeviceSize MAX_VERTEX_COUNT   = 1 << 16;
constexpr VkDeviceSize MAX_INDEX_COUNT    = 1 << 18;

constexpr const char *VERT_SHADER = "VertLensFlare";
constexpr const char *FRAG_SHADER = "FragLensFlare";


struct RasterizerVertex
{
    float       position[3];
    uint32_t    color;
    float       texCoord[2];
};


// indirectDrawCommands: one uint32_t - for count, the rest - cmds
constexpr VkDeviceSize GetIndirectDrawCommandsOffset()
{
    return 0;
}
constexpr VkDeviceSize GetIndirectDrawCountOffset()
{
    return LENS_FLARES_MAX_DRAW_CMD_COUNT * sizeof(RTGL1::ShIndirectDrawCommand);
}
constexpr RTGL1::ShIndirectDrawCommand *GetIndirectDrawCommandsArrayStart(void *pCullingInputBuffer)
{
    return (RTGL1::ShIndirectDrawCommand *)((uint8_t*)pCullingInputBuffer + GetIndirectDrawCommandsOffset());
}


static_assert(sizeof(RasterizerVertex) == sizeof(RgRasterizedGeometryVertexStruct), "");
static_assert(offsetof(RasterizerVertex, position) == offsetof(RgRasterizedGeometryVertexStruct, position),    "");
static_assert(offsetof(RasterizerVertex, color)    == offsetof(RgRasterizedGeometryVertexStruct, packedColor), "");
static_assert(offsetof(RasterizerVertex, texCoord) == offsetof(RgRasterizedGeometryVertexStruct, texCoord),    "");

static_assert(offsetof(RTGL1::ShIndirectDrawCommand, indexCount)    == offsetof(VkDrawIndexedIndirectCommand, indexCount),    "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand");
static_assert(offsetof(RTGL1::ShIndirectDrawCommand, instanceCount) == offsetof(VkDrawIndexedIndirectCommand, instanceCount), "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand");
static_assert(offsetof(RTGL1::ShIndirectDrawCommand, firstIndex)    == offsetof(VkDrawIndexedIndirectCommand, firstIndex),    "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand");
static_assert(offsetof(RTGL1::ShIndirectDrawCommand, vertexOffset)  == offsetof(VkDrawIndexedIndirectCommand, vertexOffset),  "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand");
static_assert(offsetof(RTGL1::ShIndirectDrawCommand, firstInstance) == offsetof(VkDrawIndexedIndirectCommand, firstInstance), "ShIndirectDrawCommand mismatches VkDrawIndexedIndirectCommand");


RTGL1::LensFlares::LensFlares(
    VkDevice _device, 
    std::shared_ptr<MemoryAllocator> &_allocator, 
    const std::shared_ptr<ShaderManager> &_shaderManager,
    VkRenderPass renderPass,
    std::shared_ptr<GlobalUniform> _uniform,
    std::shared_ptr<Framebuffers> _framebuffers,
    std::shared_ptr<TextureManager> _textureManager,
    const RgInstanceCreateInfo &_instanceInfo)
:
    device(_device),
    uniform(std::move(_uniform)),
    framebuffers(std::move(_framebuffers)),
    textureManager(std::move(_textureManager)),
    cullingInputCount(0),
    vertexCount(0),
    indexCount(0),
    vertFragPipelineLayout(VK_NULL_HANDLE),
    rasterDescPool(VK_NULL_HANDLE),
    rasterDescSet(VK_NULL_HANDLE),
    rasterDescSetLayout(VK_NULL_HANDLE),
    cullPipelineLayout(VK_NULL_HANDLE),
    cullPipeline(VK_NULL_HANDLE),
    cullDescPool(VK_NULL_HANDLE),
    cullDescSet(VK_NULL_HANDLE),
    cullDescSetLayout(VK_NULL_HANDLE),
    lensFlareBlendFactorSrc{},
    lensFlareBlendFactorDst{},
    isPointToCheckInScreenSpace(!!_instanceInfo.lensFlarePointToCheckIsInScreenSpace)
{
    cullingInput = std::make_unique<AutoBuffer>(_allocator);
    vertexBuffer = std::make_unique<AutoBuffer>(_allocator);
    indexBuffer = std::make_unique<AutoBuffer>(_allocator);
    instanceBuffer = std::make_unique<AutoBuffer>(_allocator);


    cullingInput->Create(
        LENS_FLARES_MAX_DRAW_CMD_COUNT * sizeof(ShIndirectDrawCommand),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, "Lens flares culling input");

    indirectDrawCommands.Init(
        _allocator, 
        LENS_FLARES_MAX_DRAW_CMD_COUNT * sizeof(ShIndirectDrawCommand) + sizeof(uint32_t),
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Lens flares draw cmds");

    vertexBuffer->Create(
        MAX_VERTEX_COUNT * sizeof(RasterizerVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, "Lens flares vertex buffer");
    
    indexBuffer->Create(
        MAX_INDEX_COUNT * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, "Lens flares index buffer");

    instanceBuffer->Create(
        LENS_FLARES_MAX_DRAW_CMD_COUNT * sizeof(ShLensFlareInstance),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, "Lens flares instance buffer");


    CreateCullDescriptors();
    CreateRasterDescriptors();


    CreatePipelineLayouts(uniform->GetDescSetLayout(), textureManager->GetDescSetLayout(), rasterDescSetLayout, cullDescSetLayout, framebuffers->GetDescSetLayout());


    rasterPipelines = std::make_unique<RasterizerPipelines>(device, vertFragPipelineLayout, renderPass, _instanceInfo.rasterizedVertexColorGamma);
    rasterPipelines->SetShaders(_shaderManager.get(), VERT_SHADER, FRAG_SHADER);

    CreatePipelines(_shaderManager.get());
}

RTGL1::LensFlares::~LensFlares()
{
    vkDestroyDescriptorPool(device, rasterDescPool, nullptr);
    vkDestroyDescriptorSetLayout(device, rasterDescSetLayout, nullptr);
    vkDestroyDescriptorPool(device, cullDescPool, nullptr);
    vkDestroyDescriptorSetLayout(device, cullDescSetLayout, nullptr);

    vkDestroyPipelineLayout(device, vertFragPipelineLayout, nullptr);
    vkDestroyPipelineLayout(device, cullPipelineLayout, nullptr);

    DestroyPipelines();
}


void RTGL1::LensFlares::PrepareForFrame(uint32_t frameIndex)
{
    cullingInputCount = 0;
    vertexCount = 0;
    indexCount = 0;
}

void RTGL1::LensFlares::Upload(uint32_t frameIndex, const RgLensFlareUploadInfo &uploadInfo)
{
    if (cullingInputCount + 1 >= LENS_FLARES_MAX_DRAW_CMD_COUNT)
    {
        assert(!"Too many lens flares");
        return;
    }
    if (vertexCount + uploadInfo.vertexCount >= MAX_VERTEX_COUNT)
    {
        assert(!"Too many lens flare vertices");
        return;
    }
    if (indexCount + uploadInfo.indexCount >= MAX_INDEX_COUNT)
    {
        assert(!"Too many lens flare indices");
        return;
    }


    const uint32_t instanceIndex = cullingInputCount;
    const uint32_t vertexIndex = vertexCount;
    const uint32_t indexIndex = indexCount;
    cullingInputCount++;
    vertexCount += uploadInfo.vertexCount;
    indexCount += uploadInfo.indexCount;


    // vertices
    {
        RasterizerVertex *dst = (RasterizerVertex *)vertexBuffer->GetMapped(frameIndex);
        memcpy(&dst[vertexIndex], uploadInfo.pVertexData, uploadInfo.vertexCount * sizeof(RasterizerVertex));
    }


    // indices
    {
        uint32_t *dst = (uint32_t *)indexBuffer->GetMapped(frameIndex);
        memcpy(&dst[indexIndex], uploadInfo.pIndexData, uploadInfo.indexCount * sizeof(uint32_t));
    }


    // instances
    ShLensFlareInstance instance = {};
    instance.textureIndex = textureManager->GetMaterialTextures(uploadInfo.material).indices[MATERIAL_ALBEDO_ALPHA_INDEX];

    {
        ShLensFlareInstance *dst = (ShLensFlareInstance *)instanceBuffer->GetMapped(frameIndex);
        memcpy(&dst[instanceIndex], &instance, sizeof(ShLensFlareInstance));
    }


    // draw cmds
    ShIndirectDrawCommand input = {};
    input.vertexOffset = vertexIndex;
    input.firstIndex = indexIndex;
    input.indexCount = uploadInfo.indexCount;
    input.firstInstance = instanceIndex; // to access instance buffer with gl_InstanceIndex
    input.instanceCount = 1;
    input.positionToCheck_X = uploadInfo.pointToCheck.data[0];
    input.positionToCheck_Y = uploadInfo.pointToCheck.data[1];
    input.positionToCheck_Z = uploadInfo.pointToCheck.data[2];

    {
        ShIndirectDrawCommand *dst = GetIndirectDrawCommandsArrayStart(cullingInput->GetMapped(frameIndex));
        memcpy(&dst[instanceIndex], &input, sizeof(ShIndirectDrawCommand));
    }
}

void RTGL1::LensFlares::SubmitForFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (cullingInputCount == 0 || vertexCount == 0 || indexCount == 0)
    {
        return;
    }

      cullingInput->CopyFromStaging(cmd, frameIndex, cullingInputCount * sizeof(ShIndirectDrawCommand));
      vertexBuffer->CopyFromStaging(cmd, frameIndex, vertexCount       * sizeof(RasterizerVertex));
       indexBuffer->CopyFromStaging(cmd, frameIndex, indexCount        * sizeof(uint32_t));
    instanceBuffer->CopyFromStaging(cmd, frameIndex, cullingInputCount * sizeof(ShLensFlareInstance));
}

void RTGL1::LensFlares::SetParams(const RgDrawFrameLensFlareParams *pLensFlareParams)
{
    if (pLensFlareParams != nullptr)
    {
        lensFlareBlendFactorSrc = pLensFlareParams->lensFlareBlendFuncSrc;
        lensFlareBlendFactorDst = pLensFlareParams->lensFlareBlendFuncDst;
    }
    else
    {
        lensFlareBlendFactorSrc = RG_BLEND_FACTOR_ONE;
        lensFlareBlendFactorDst = RG_BLEND_FACTOR_ONE;
    }
}

void RTGL1::LensFlares::Cull(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (cullingInputCount == 0)
    {
        return;
    }

    // sync
    {
        VkBufferMemoryBarrier2KHR bs[1] = {};
        {
            VkBufferMemoryBarrier2KHR &b = bs[0];
            b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
            b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
            b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
            b.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
            b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT_KHR;
            b.buffer = cullingInput->GetDeviceLocal();
            b.offset = 0;
            b.size = cullingInputCount * sizeof(ShIndirectDrawCommand);
        }

        VkDependencyInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
        info.bufferMemoryBarrierCount = std::size(bs);
        info.pBufferMemoryBarriers = bs;

        svkCmdPipelineBarrier2KHR(cmd, &info);
    }


    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipeline);

    VkDescriptorSet sets[] =
    {
        uniform->GetDescSet(frameIndex),
        framebuffers->GetDescSet(frameIndex),
        cullDescSet
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            cullPipelineLayout,
                            0, std::size(sets), sets,
                            0, nullptr);
    
    uint32_t wgCount = Utils::GetWorkGroupCount(cullingInputCount, COMPUTE_INDIRECT_DRAW_FLARES_GROUP_SIZE_X);
    vkCmdDispatch(cmd, wgCount, 1, 1);
}

void RTGL1::LensFlares::SyncForDraw(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (cullingInputCount == 0)
    {
        return;
    }		

    VkBufferMemoryBarrier2KHR bs[5] = {};

    {
        VkBufferMemoryBarrier2KHR &b = bs[0];
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
        b.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
        b.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
        b.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR;
        b.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR;
        b.buffer = indirectDrawCommands.GetBuffer();
        b.offset = GetIndirectDrawCommandsOffset();
        b.size = cullingInputCount * sizeof(ShIndirectDrawCommand);
    }
    {
        VkBufferMemoryBarrier2KHR &b = bs[1];
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
        b.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR;
        b.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT_KHR;
        b.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT_KHR;
        b.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT_KHR;
        b.buffer = indirectDrawCommands.GetBuffer();
        b.offset = GetIndirectDrawCountOffset();
        b.size = sizeof(uint32_t);
    }
    {
        VkBufferMemoryBarrier2KHR &b = bs[2];
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
        b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
        b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
        b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
        b.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR;
        b.buffer = instanceBuffer->GetDeviceLocal();
        b.offset = 0;
        b.size = cullingInputCount * sizeof(ShLensFlareInstance);
    }
    {
        VkBufferMemoryBarrier2KHR &b = bs[3];
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
        b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
        b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
        b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR;
        b.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR;
        b.buffer = vertexBuffer->GetDeviceLocal();
        b.offset = 0;
        b.size = vertexCount * sizeof(RasterizerVertex);
    }
    {
        VkBufferMemoryBarrier2KHR &b = bs[4];
        b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
        b.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT_KHR;
        b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
        b.dstStageMask = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR;
        b.dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT_KHR;
        b.buffer = indexBuffer->GetDeviceLocal();
        b.offset = 0;
        b.size = indexCount * sizeof(uint32_t);
    }


    VkDependencyInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    info.bufferMemoryBarrierCount = std::size(bs);
    info.pBufferMemoryBarriers = bs;

    svkCmdPipelineBarrier2KHR(cmd, &info);
}

void RTGL1::LensFlares::Draw(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (cullingInputCount == 0)
    {
        return;
    }

    VkPipeline drawPipeline = rasterPipelines->GetPipeline(true, lensFlareBlendFactorSrc, lensFlareBlendFactorDst, false, false, false);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, drawPipeline);

    VkDescriptorSet sets[] =
    {
        uniform->GetDescSet(frameIndex),
        textureManager->GetDescSet(frameIndex),
        rasterDescSet
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterPipelines->GetPipelineLayout(),
                            0, std::size(sets), sets,
                            0, nullptr);       

    VkBuffer vb = vertexBuffer->GetDeviceLocal();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer->GetDeviceLocal(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexedIndirectCount(cmd, 
                                  indirectDrawCommands.GetBuffer(), GetIndirectDrawCommandsOffset(),
                                  indirectDrawCommands.GetBuffer(), GetIndirectDrawCountOffset(),
                                  LENS_FLARES_MAX_DRAW_CMD_COUNT,
                                  sizeof(ShIndirectDrawCommand));
}

uint32_t RTGL1::LensFlares::GetCullingInputCount() const
{
    return cullingInputCount;
}

void RTGL1::LensFlares::CreatePipelineLayouts(VkDescriptorSetLayout uniform, VkDescriptorSetLayout textures, VkDescriptorSetLayout raster,
                                              VkDescriptorSetLayout lensFlaresCull, VkDescriptorSetLayout framebufs)
{
    {
        VkDescriptorSetLayout s[] =
        {
            uniform,
            textures,
            raster
        };

        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = std::size(s);
        layoutInfo.pSetLayouts = s;

        VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &vertFragPipelineLayout);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, vertFragPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Lens flares vert-frag pipeline layout");
    }
    {
        VkDescriptorSetLayout s[] =
        {
            uniform,
            framebufs,
            lensFlaresCull
        };

        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = std::size(s);
        layoutInfo.pSetLayouts = s;

        VkResult r = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &cullPipelineLayout);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, cullPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "Lens flares cull pipeline layout");
    }
}

void RTGL1::LensFlares::CreatePipelines(const ShaderManager *shaderManager)
{
    VkSpecializationMapEntry entry = {};
    entry.constantID = 0;
    entry.size = sizeof(uint32_t);
    entry.offset = 0;

    VkSpecializationInfo spec = {};
    spec.mapEntryCount = 1;
    spec.pMapEntries = &entry;
    spec.dataSize = sizeof(uint32_t);
    spec.pData = &isPointToCheckInScreenSpace;

    VkComputePipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.layout = cullPipelineLayout;
    info.stage = shaderManager->GetStageInfo("CCullLensFlares");
    info.stage.pSpecializationInfo = &spec;

    VkResult r = vkCreateComputePipelines(device, nullptr, 1, &info, nullptr, &cullPipeline);
    VK_CHECKERROR(r);
}

void RTGL1::LensFlares::DestroyPipelines()
{
    if (cullPipeline != nullptr)
    {
        vkDestroyPipeline(device, cullPipeline, nullptr);
    }
}

void RTGL1::LensFlares::OnShaderReload(const ShaderManager *shaderManager)
{
    rasterPipelines->Clear();
    rasterPipelines->SetShaders(shaderManager, VERT_SHADER, FRAG_SHADER);

    DestroyPipelines();
    CreatePipelines(shaderManager);
}

void RTGL1::LensFlares::CreateCullDescriptors()
{
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 2;

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        VkResult r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &cullDescPool);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, cullDescPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Lens flare cull desc pool");
    }
    {
        VkDescriptorSetLayoutBinding binding[2] = {};

        binding[0].binding = BINDING_LENS_FLARES_CULLING_INPUT;
        binding[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding[0].descriptorCount = 1;
        binding[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        binding[1].binding = BINDING_LENS_FLARES_DRAW_CMDS;
        binding[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding[1].descriptorCount = 1;
        binding[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = std::size(binding);
        info.pBindings = binding;

        VkResult r = vkCreateDescriptorSetLayout(device, &info, nullptr, &cullDescSetLayout);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, cullDescSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Lens flare cull desc set layout");
    }

    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = cullDescPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &cullDescSetLayout;

        VkResult r = vkAllocateDescriptorSets(device, &allocInfo, &cullDescSet);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, cullDescSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Lens flare cull desc set");
    }
    {
        VkDescriptorBufferInfo bufs[2] = {};
        VkWriteDescriptorSet writes[2] = {};

        {
            VkDescriptorBufferInfo &b = bufs[0];
            b.buffer = cullingInput->GetDeviceLocal();
            b.offset = 0;
            b.range = VK_WHOLE_SIZE;

            VkWriteDescriptorSet &w = writes[0];
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = cullDescSet;
            w.dstBinding = BINDING_LENS_FLARES_CULLING_INPUT;
            w.dstArrayElement = 0;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.descriptorCount = 1;
            w.pBufferInfo = &b;
        }
        {
            VkDescriptorBufferInfo &b = bufs[1];
            b.buffer = indirectDrawCommands.GetBuffer();
            b.offset = 0;
            b.range = VK_WHOLE_SIZE;

            VkWriteDescriptorSet &w = writes[1];
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = cullDescSet;
            w.dstBinding = BINDING_LENS_FLARES_DRAW_CMDS;
            w.dstArrayElement = 0;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.descriptorCount = 1;
            w.pBufferInfo = &b;
        }

        vkUpdateDescriptorSets(device, std::size(writes), writes, 0, nullptr);
    }
}

void RTGL1::LensFlares::CreateRasterDescriptors()
{
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        VkResult r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &rasterDescPool);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, rasterDescPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL, "Lens flare raster desc pool");
    }
    {
        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = BINDING_DRAW_LENS_FLARES_INSTANCES;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &binding;

        VkResult r = vkCreateDescriptorSetLayout(device, &info, nullptr, &rasterDescSetLayout);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, rasterDescSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "Lens flare raster desc set layout");
    }

    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = rasterDescPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &rasterDescSetLayout;

        VkResult r = vkAllocateDescriptorSets(device, &allocInfo, &rasterDescSet);
        VK_CHECKERROR(r);

        SET_DEBUG_NAME(device, rasterDescSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "Lens flare raster desc set");
    }
    {
        VkDescriptorBufferInfo b = {};
        b.buffer = instanceBuffer->GetDeviceLocal();
        b.offset = 0;
        b.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet w = {};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = rasterDescSet;
        w.dstBinding = BINDING_DRAW_LENS_FLARES_INSTANCES;
        w.dstArrayElement = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 1;
        w.pBufferInfo = &b;

        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }
}

