#include "VertexBufferManager.h"

#include <array>
#include "Generated/ShaderCommonC.h"


VertexBufferManager::VertexBufferManager(VkDevice device, const PhysicalDevice &physDevice, const RgInstanceCreateInfo &info)
{
    this->device = device;

    properties.vertexArrayOfStructs = info.vertexArrayOfStructs == RG_TRUE;
    properties.positionStride = info.vertexPositionStride;
    properties.normalStride = info.vertexNormalStride;
    properties.texCoordStride = info.vertexTexCoordStride;
    properties.colorStride = info.vertexColorStride;

    // static vertices
    staticVertsStaging = std::make_shared<Buffer>();
    staticVertsBuffer = std::make_shared<Buffer>();

    staticVertsStaging->Init(device, physDevice,
                            sizeof(ShVertexBufferStatic),
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staticVertsBuffer->Init(device, physDevice,
                           sizeof(ShVertexBufferStatic),
                           VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    collectorStaticMovable = std::make_shared<VertexCollectorFiltered>(staticVertsStaging, properties, RG_GEOMETRY_TYPE_STATIC_MOVABLE);

    // dynamic vertices
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        dynamicVertsStaging[i] = std::make_shared<Buffer>();
        dynamicVertsBuffer[i] = std::make_shared<Buffer>();

        dynamicVertsStaging[i]->Init(device, physDevice,
                                    sizeof(ShVertexBufferDynamic),
                                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        dynamicVertsBuffer[i]->Init(device, physDevice,
                                   sizeof(ShVertexBufferDynamic),
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        collectorDynamic[i] = std::make_shared<VertexCollector>(dynamicVertsStaging[i], properties);

    }

    CreateDescriptors();
}

VertexBufferManager::~VertexBufferManager()
{
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
}

void VertexBufferManager::AddGeometry(const RgGeometryCreateInfo& info)
{
    if (info.geomType == RG_GEOMETRY_TYPE_DYNAMIC)
    {
        collectorDynamic[currentFrameIndex]->AddGeometry(info);
    }
    else
    {
        collectorStaticMovable->AddGeometry(info);
    }
}

void VertexBufferManager::BeginStaticGeometry()
{
    collectorStaticMovable->BeginCollecting();
}

void VertexBufferManager::SubmitStaticGeometry()
{
    collectorStaticMovable->EndCollecting();

    VkResult r;

    // TODO: delete previous?

    const auto& blasGT = collectorStaticMovable->GetBlasGeometriyTypes();
    VkAccelerationStructureCreateInfoKHR blasInfo = {};
    blasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    blasInfo.maxGeometryCount = blasGT.size();
    blasInfo.pGeometryInfos = blasGT.data();
    r = vksCreateAccelerationStructureKHR(device, &blasInfo, nullptr, &staticBlas);
    VK_CHECKERROR(r);

    const auto &movableBlasGT = collectorStaticMovable->GetBlasGeometriyTypesFiltered();
    VkAccelerationStructureCreateInfoKHR movableBlasInfo = {};
    movableBlasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    movableBlasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    movableBlasInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    movableBlasInfo.maxGeometryCount = movableBlasGT.size();
    movableBlasInfo.pGeometryInfos = movableBlasGT.data();
    r = vksCreateAccelerationStructureKHR(device, &movableBlasInfo, nullptr, &staticMovableBlas);
    VK_CHECKERROR(r);



    // TODO: alloc, bind memory, build




    collectorStaticMovable->Reset();
}

void VertexBufferManager::BeginDynamicGeometry(uint32_t frameIndex)
{
    currentFrameIndex = frameIndex;

    collectorDynamic[currentFrameIndex]->BeginCollecting();
}

void VertexBufferManager::SubmitDynamicGeometry()
{
    const auto &dynCol = collectorDynamic[currentFrameIndex];
    dynCol->EndCollecting();

    VkResult r;

    // TODO: recreate every frame? delete previous?

    const auto &blasGT = dynCol->GetBlasGeometriyTypes();
    VkAccelerationStructureCreateInfoKHR blasInfo = {};
    blasInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    blasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
    blasInfo.maxGeometryCount = blasGT.size();
    blasInfo.pGeometryInfos = blasGT.data();
    r = vksCreateAccelerationStructureKHR(device, &blasInfo, nullptr, &dynamicBlas[currentFrameIndex]);
    VK_CHECKERROR(r);



    // TODO: alloc, bind memory, build





    dynCol->Reset();
}

void VertexBufferManager::CreateDescriptors()
{
    VkResult r;
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    // static
    bindings[0].binding = BINDING_VERTEX_BUFFER_STATIC;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_ALL;

    // dynamic
    bindings[1].binding = BINDING_VERTEX_BUFFER_DYNAMIC;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 2;
    bindings[1].stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindings.size();
    layoutInfo.pBindings = bindings.data();
    
    r = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout);
    VK_CHECKERROR(r);

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    r = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool);
    VK_CHECKERROR(r);

    VkDescriptorSetAllocateInfo descSetInfo = {};
    descSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetInfo.descriptorPool = descPool;
    descSetInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    descSetInfo.pSetLayouts = &descSetLayout;

    r = vkAllocateDescriptorSets(device, &descSetInfo, descSets);
    VK_CHECKERROR(r);

    // bind buffers to descriptors
    std::array<VkDescriptorBufferInfo, 2 * MAX_FRAMES_IN_FLIGHT> bufferInfos{};

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        bufferInfos[i * 2].buffer = staticVertsBuffer->GetBuffer();
        bufferInfos[i * 2].offset = 0;
        bufferInfos[i * 2].range = VK_WHOLE_SIZE;

        bufferInfos[i * 2 + 1].buffer = dynamicVertsBuffer[i]->GetBuffer();
        bufferInfos[i * 2 + 1].offset = 0;
        bufferInfos[i * 2 + 1].range = VK_WHOLE_SIZE;
    }


    std::array<VkWriteDescriptorSet, bufferInfos.size()> writes{};

    for (uint32_t i = 0; i < bufferInfos.size(); i++)
    {
        VkWriteDescriptorSet &w = writes[i];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = descSets[i];
        w.dstBinding = BINDING_VERTEX_BUFFER_STATIC;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}


