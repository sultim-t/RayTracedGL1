#include "VertexCollector.h"
#include "Generated/ShaderCommonC.h"

VertexCollector::VertexCollector(VkDevice device, const PhysicalDevice& physDevice,
                                 std::shared_ptr<Buffer> stagingVertBuffer, std::shared_ptr<Buffer> vertBuffer,
                                 const VBProperties& properties) :
    properties({}),
    mappedVertexData(nullptr), mappedIndexData(nullptr), mappedTransformData(nullptr),
    curVertexCount(0), curIndexCount(0), curPrimitiveCount(0), curGeometryCount(0)
{
    this->device = device;
    this->stagingVertBuffer = stagingVertBuffer;
    this->vertBuffer = vertBuffer;
    this->properties = properties;

    indices.Init(
        device, physDevice, MAX_VERTEX_COLLECTOR_INDEX_COUNT * sizeof(uint32_t),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    transforms.Init(
        device, physDevice, MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT * sizeof(VkTransformMatrixKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

VertexCollector::~VertexCollector()
{
    // unmap buffers to destroy them 
    if (auto sb = stagingVertBuffer.lock())
    {
        sb->TryUnmap();
    }

    indices.TryUnmap();
    transforms.TryUnmap();
}

void VertexCollector::BeginCollecting()
{
    assert((mappedVertexData == nullptr && mappedIndexData == nullptr && mappedTransformData == nullptr) ||
           (mappedVertexData != nullptr && mappedIndexData != nullptr && mappedTransformData != nullptr));
    assert(curVertexCount == 0 && curIndexCount == 0 && curPrimitiveCount == 0 && curGeometryCount == 0);
    assert(asGeometries.empty() && asBuildRangeInfos.empty());

    if (stagingVertBuffer.expired() || vertBuffer.expired())
    {
        mappedVertexData = nullptr;
        mappedIndexData = nullptr;
        mappedTransformData = nullptr;
        return;
    }

    if (mappedVertexData == nullptr)
    {
        mappedVertexData = static_cast<uint8_t *>(stagingVertBuffer.lock()->Map());
        mappedIndexData = static_cast<uint32_t *>(indices.Map());
        mappedTransformData = static_cast<VkTransformMatrixKHR *>(transforms.Map());
    }
}

uint32_t VertexCollector::AddGeometry(const RgGeometryUploadInfo &info)
{
    if (stagingVertBuffer.expired() || vertBuffer.expired())
    {
        return 0;
    }

    const bool collectStatic = info.geomType == RG_GEOMETRY_TYPE_STATIC || info.geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE;

    const uint32_t maxVertexCount = collectStatic ?
        MAX_STATIC_VERTEX_COUNT :
        MAX_DYNAMIC_VERTEX_COUNT;

    const uint32_t geomIndex = curGeometryCount;
    const uint32_t vertIndex = curVertexCount;
    const uint32_t indIndex = curIndexCount;

    curGeometryCount++;
    curVertexCount += info.vertexCount;

    const bool useIndices = info.indexCount != 0 && info.indexData != nullptr;
    const uint32_t primitiveCount = useIndices ? info.indexCount / 3 : info.vertexCount / 3;

    curPrimitiveCount += primitiveCount;

    if (useIndices)
    {
        curIndexCount += primitiveCount;
    }

    assert(curVertexCount < maxVertexCount);
    assert(curIndexCount < MAX_VERTEX_COLLECTOR_INDEX_COUNT);
    assert(curGeometryCount < MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT);

    if (curVertexCount >= maxVertexCount ||
        curIndexCount >= MAX_VERTEX_COLLECTOR_INDEX_COUNT ||
        curGeometryCount >= MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT)
    {
        return 0;
    }

    assert(stagingVertBuffer.lock()->IsMapped());

    // copy data to buffer
    CopyDataToStaging(info, vertIndex, collectStatic);

    if (useIndices)
    {
        memcpy(mappedIndexData + indIndex, info.indexData, primitiveCount * sizeof(uint32_t));
    }

    memcpy(mappedTransformData + geomIndex, &info.transform, sizeof(RgTransform));

    // use positions data in the final vertex buffer: AS won't be built using staging buffer
    VkDeviceAddress vertexDataDeviceAddress =
        vertBuffer.lock()->GetAddress() + (uint64_t) vertIndex * (uint64_t) properties.positionStride;

    // geometry info
    VkAccelerationStructureGeometryKHR geom = {};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

    VkAccelerationStructureGeometryTrianglesDataKHR &trData = geom.geometry.triangles;
    trData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    trData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    trData.maxVertex = 1 << 16;
    trData.vertexData.deviceAddress = vertexDataDeviceAddress;
    trData.vertexStride = properties.positionStride;
    trData.transformData.deviceAddress = transforms.GetAddress() + geomIndex * sizeof(RgTransform);

    if (useIndices)
    {
        trData.indexType = VK_INDEX_TYPE_UINT32;
        trData.indexData.deviceAddress = indices.GetAddress() + indIndex * sizeof(uint32_t);
    }
    else
    {
        trData.indexType = VK_INDEX_TYPE_NONE_KHR;
        trData.indexData = {};
    }

    PushGeometry(info.geomType, geom);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;
    PushRangeInfo(info.geomType, rangeInfo);

    PushPrimitiveCount(info.geomType, primitiveCount);

    return geomIndex;
}

void VertexCollector::CopyDataToStaging(const RgGeometryUploadInfo &info, uint32_t vertIndex, bool isStatic)
{
    const uint32_t wholeBufferSize = isStatic ?
        sizeof(ShVertexBufferStatic) :
        sizeof(ShVertexBufferDynamic);

    const uint32_t offsetPositions = isStatic ?
        offsetof(ShVertexBufferStatic, positions) :
        offsetof(ShVertexBufferDynamic, positions);
    const uint32_t offsetNormals = isStatic ?
        offsetof(ShVertexBufferStatic, normals) :
        offsetof(ShVertexBufferDynamic, normals);
    const uint32_t offsetTexCoords = isStatic ?
        offsetof(ShVertexBufferStatic, texCoords) :
        offsetof(ShVertexBufferDynamic, texCoords);
    const uint32_t offsetColors = isStatic ?
        offsetof(ShVertexBufferStatic, colors) :
        offsetof(ShVertexBufferDynamic, colors);
    const uint32_t offsetMaterials = isStatic ?
        offsetof(ShVertexBufferStatic, materialIds) :
        offsetof(ShVertexBufferDynamic, materialIds);

    // positions
    void *positionsDst = mappedVertexData + offsetPositions + vertIndex * properties.positionStride;
    assert(offsetPositions + vertIndex * properties.positionStride < wholeBufferSize);

    memcpy(positionsDst, info.vertexData, info.vertexCount * properties.positionStride);

    // normals
    void *normalsDst = mappedVertexData + offsetNormals + vertIndex * properties.normalStride;
    assert(offsetNormals + vertIndex * properties.normalStride < wholeBufferSize);

    if (info.normalData != nullptr)
    {
        memcpy(normalsDst, info.normalData, info.vertexCount * properties.normalStride);
    }
    else
    {
        // TODO: generate normals
        memset(normalsDst, 0, info.vertexCount * properties.normalStride);
    }

    // tex coords
    void *texCoordDst = mappedVertexData + offsetTexCoords + vertIndex * properties.texCoordStride;
    assert(offsetTexCoords + vertIndex * properties.texCoordStride < wholeBufferSize);

    if (info.texCoordData != nullptr)
    {
        memcpy(texCoordDst, info.texCoordData, info.vertexCount * properties.texCoordStride);
    }
    else
    {
        memset(texCoordDst, 0, info.vertexCount * properties.texCoordStride);
    }

    // colors
    void *colorDst = mappedVertexData + offsetColors + vertIndex * properties.colorStride;
    assert(offsetColors + vertIndex * properties.colorStride < wholeBufferSize);

    if (info.colorData != nullptr)
    {
        memcpy(colorDst, info.colorData, info.vertexCount * properties.colorStride);
    }
    else
    {
        // set white color
        memset(colorDst, 0xFF, info.vertexCount * properties.colorStride);
    }

    const bool useIndices = info.indexCount != 0 && info.indexData != nullptr;
    const uint32_t triangleCount = useIndices ? info.indexCount / 3 : info.vertexCount / 3;

    // materials
    void *matDst = mappedVertexData + offsetMaterials + vertIndex * sizeof(RgLayeredMaterial);
    assert(offsetMaterials + vertIndex * sizeof(RgLayeredMaterial) < wholeBufferSize);

    if (info.triangleMaterials != nullptr)
    {
        memcpy(matDst, info.triangleMaterials, triangleCount * sizeof(RgLayeredMaterial));
    }
    else
    {
        // TODO: info.geomMaterial
        memset(matDst, RG_NO_TEXTURE, triangleCount * sizeof(RgLayeredMaterial));
    }
}

void VertexCollector::EndCollecting()
{
    if (auto sb = stagingVertBuffer.lock())
    {
        sb->Unmap();
    }

    indices.Unmap();
    transforms.Unmap();

    mappedVertexData = nullptr;
    mappedIndexData = nullptr;
    mappedTransformData = nullptr;
}

const std::vector<uint32_t> &VertexCollector::GetPrimitiveCounts() const
{
    return primitiveCounts;
}

void VertexCollector::Reset()
{
    curVertexCount = 0;
    curIndexCount = 0;
    curPrimitiveCount = 0;
    curGeometryCount = 0;

    primitiveCounts.clear();
    asGeometries.clear();
    asBuildRangeInfos.clear();
}

void VertexCollector::CopyFromStaging(VkCommandBuffer cmd, bool copyStatic)
{
    auto src = stagingVertBuffer.lock();
    auto dst = vertBuffer.lock();

    if (src && dst)
    {
        std::array<VkBufferCopy, 5> copyInfos = {};
        bool hasInfo = GetCopyInfos(copyStatic, copyInfos);

        if (!hasInfo)
        {
            return;
        }

        vkCmdCopyBuffer(cmd, src->GetBuffer(), dst->GetBuffer(),
                        copyInfos.size(), copyInfos.data());

        // sync dst buffer access
        VkBufferMemoryBarrier bufferMemBarrier = {};
        bufferMemBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bufferMemBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        bufferMemBarrier.buffer = dst->GetBuffer();
        bufferMemBarrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             0, 0, nullptr, 1, &bufferMemBarrier,
                             0, nullptr);
    }
}

bool VertexCollector::GetCopyInfos(bool isStatic, std::array<VkBufferCopy, 5> &outInfos) const
{
    const uint32_t offsetPositions = isStatic ?
        offsetof(ShVertexBufferDynamic, positions) :
        offsetof(ShVertexBufferStatic, positions);
    const uint32_t offsetNormals = isStatic ?
        offsetof(ShVertexBufferDynamic, normals) :
        offsetof(ShVertexBufferStatic, normals);
    const uint32_t offsetTexCoords = isStatic ?
        offsetof(ShVertexBufferDynamic, texCoords) :
        offsetof(ShVertexBufferStatic, texCoords);
    const uint32_t offsetColors = isStatic ?
        offsetof(ShVertexBufferDynamic, colors) :
        offsetof(ShVertexBufferStatic, colors);
    const uint32_t offsetMaterials = isStatic ?
        offsetof(ShVertexBufferDynamic, materialIds) :
        offsetof(ShVertexBufferStatic, materialIds);

    if (curVertexCount == 0 || curPrimitiveCount == 0)
    {
        return false;
    }

    outInfos[0] = { offsetPositions,offsetPositions,    curVertexCount * properties.positionStride };
    outInfos[1] = { offsetNormals,offsetNormals,        curVertexCount * properties.normalStride };
    outInfos[2] = { offsetTexCoords,offsetTexCoords,    curVertexCount * properties.texCoordStride };
    outInfos[3] = { offsetColors,offsetColors,          curVertexCount * properties.colorStride };
    outInfos[4] = { offsetMaterials,offsetMaterials,    curPrimitiveCount * sizeof(RgLayeredMaterial) };

    return true;
}

void VertexCollector::UpdateTransform(uint32_t geomIndex, const RgTransform &transform)
{
    assert(geomIndex < MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT);
    assert(mappedTransformData != nullptr);

    memcpy(mappedTransformData + geomIndex, &transform, sizeof(RgTransform));
}

void VertexCollector::PushPrimitiveCount(RgGeometryType type, uint32_t primCount)
{
    primitiveCounts.push_back(primCount);
}

void VertexCollector::PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR &geom)
{
    asGeometries.push_back(geom);
}

void VertexCollector::PushRangeInfo(RgGeometryType type, const VkAccelerationStructureBuildRangeInfoKHR &rangeInfo)
{
    asBuildRangeInfos.push_back(rangeInfo);
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollector::GetASGeometries() const
{
    return asGeometries;
}

const std::vector<VkAccelerationStructureBuildRangeInfoKHR> &VertexCollector::GetASBuildRangeInfos() const
{
    return asBuildRangeInfos;
}