#include "VertexCollector.h"
#include "Generated/ShaderCommonC.h"

#define MAX_INDICES_IN_BUFFER 1 << 21
#define MAX_GEOMETRIES_IN_BUFFER 4096

VertexCollector::VertexCollector(std::shared_ptr<Buffer> stagingVertBuffer, std::shared_ptr<Buffer> vertBuffer, const VBProperties &properties)
{
    this->stagingVertBuffer = stagingVertBuffer;
    this->vertBuffer = vertBuffer;
    this->properties = properties;
}

VertexCollector::~VertexCollector()
{
    Reset();
}

void VertexCollector::BeginCollecting()
{
    assert(mappedData == nullptr);
    assert(curVertexCount == 0 && curIndexCount == 0 && curGeometryCount == 0);
    assert(indices.empty() && transforms.empty() && asGeometries.empty() && asBuildOffsetInfos.empty());

    if (stagingVertBuffer.expired() || vertBuffer.expired())
    {
        return;
    }

    mappedData = static_cast<uint8_t *>(stagingVertBuffer.lock()->Map());

    // preallocate as changing vector's size will break pointers
    // TODO: chains of vectors with fixed size
    indices.resize(MAX_INDICES_IN_BUFFER);
    transforms.resize(MAX_GEOMETRIES_IN_BUFFER);
}

uint32_t VertexCollector::AddGeometry(const RgGeometryCreateInfo &info)
{
    if (stagingVertBuffer.expired() || vertBuffer.expired())
    {
        return 0;
    }

    const bool collectStatic = info.geomType == RG_GEOMETRY_TYPE_STATIC || info.geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE;

    const uint32_t maxVertexCount = collectStatic ?
        MAX_STATIC_VERTEX_COUNT :
        MAX_DYNAMIC_VERTEX_COUNT;

    const uint32_t vertIndex = curVertexCount;
    // TODO: replace curIndexCount, as now "indices" vector stores gaps if useIndices==false
    const uint32_t indIndex = curIndexCount;
    const uint32_t geomIndex = curGeometryCount;

    const bool useIndices = info.indexCount != 0 && info.indexData != nullptr;
    const uint32_t indexCount = useIndices ? info.indexCount : info.vertexCount / 3;

    curVertexCount += info.vertexCount;
    curIndexCount += indexCount;
    curGeometryCount++;

    assert(curVertexCount < maxVertexCount);
    assert(curIndexCount < MAX_INDICES_IN_BUFFER);
    assert(curGeometryCount < MAX_GEOMETRIES_IN_BUFFER);

    if (curVertexCount >= maxVertexCount ||
        curIndexCount >= MAX_INDICES_IN_BUFFER ||
        curGeometryCount >= MAX_GEOMETRIES_IN_BUFFER)
    {
        return 0;
    }

    assert(stagingVertBuffer.lock()->IsMapped());

    // copy data to buffer
    CopyDataToStaging(info, vertIndex, collectStatic);

    // geometry type info
    VkAccelerationStructureCreateGeometryTypeInfoKHR geomType = {};
    geomType.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
    geomType.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geomType.maxPrimitiveCount = info.indexCount;
    geomType.indexType = useIndices ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_NONE_KHR;
    geomType.maxVertexCount = info.vertexCount;
    geomType.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geomType.allowsTransforms = VK_TRUE;

    PushGeometryType(info.geomType, geomType);

    if (useIndices)
    {
        memcpy(&indices[indIndex], info.indexData, info.indexCount * sizeof(uint32_t));
    }

    memcpy(&transforms[geomIndex], &info.transform, sizeof(VkTransformMatrixKHR));

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
    trData.vertexData.deviceAddress = vertexDataDeviceAddress;
    trData.vertexStride = properties.positionStride;
    trData.transformData.hostAddress = &transforms[geomIndex];

    if (useIndices)
    {
        trData.indexType = VK_INDEX_TYPE_UINT32;
        trData.indexData.hostAddress = &indices[indIndex];
    }
    else
    {
        trData.indexType = VK_INDEX_TYPE_NONE_KHR;
        trData.indexData = {};
    }

    PushGeometry(info.geomType, geom);

    VkAccelerationStructureBuildOffsetInfoKHR offsetInfo = {};
    offsetInfo.primitiveCount = indexCount;
    offsetInfo.primitiveOffset = 0;
    offsetInfo.firstVertex = 0;
    offsetInfo.transformOffset = 0;
    PushOffsetInfo(info.geomType, offsetInfo);

    return geomIndex;
}

void VertexCollector::CopyDataToStaging(const RgGeometryCreateInfo &info, uint32_t vertIndex, bool isStatic)
{
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

    void *positionsDst = mappedData + vertIndex * properties.positionStride;
    memcpy(positionsDst, info.vertexData, info.vertexCount * properties.positionStride);

    void *normalsDst = mappedData + offsetNormals + vertIndex * properties.normalStride;
    if (info.normalData != nullptr)
    {
        memcpy(normalsDst, info.normalData, info.vertexCount * properties.normalStride);
    }
    else
    {
        // TODO: generate normals
        memset(normalsDst, 0, info.vertexCount * properties.normalStride);
    }

    void *texCoordDst = mappedData + offsetTexCoords + vertIndex * properties.texCoordStride;
    if (info.texCoordData != nullptr)
    {
        memcpy(texCoordDst, info.texCoordData, info.vertexCount * properties.texCoordStride);
    }
    else
    {
        memset(texCoordDst, 0, info.vertexCount * properties.texCoordStride);
    }

    void *colorDst = mappedData + offsetColors + vertIndex * properties.colorStride;
    if (info.colorData != nullptr)
    {
        memcpy(colorDst, info.colorData, info.vertexCount * properties.colorStride);
    }
    else
    {
        // set white color
        memset(colorDst, 0xFF, info.vertexCount * properties.colorStride);
    }

    void *matDst = mappedData + offsetMaterials + vertIndex * sizeof(RgLayeredMaterial);
    if (info.triangleMaterials != nullptr)
    {
        memcpy(matDst, info.triangleMaterials, info.vertexCount * sizeof(RgLayeredMaterial));
    }
    else
    {
        // TODO: info.geomMaterial
        memset(matDst, RG_NO_TEXTURE, info.vertexCount * sizeof(RgLayeredMaterial));
    }
}

void VertexCollector::EndCollecting()
{
    if (stagingVertBuffer.expired() || vertBuffer.expired())
    {
        return;
    }

    stagingVertBuffer.lock()->Unmap();
}

void VertexCollector::Reset()
{
    mappedData = nullptr;

    curVertexCount = 0;
    curIndexCount = 0;
    curGeometryCount = 0;

    indices.clear();
    transforms.clear();
    asGeometryTypes.clear();
    asGeometries.clear();
    asBuildOffsetInfos.clear();
}

void VertexCollector::CopyFromStaging(VkCommandBuffer cmd)
{
    auto src = stagingVertBuffer.lock();
    auto dst = vertBuffer.lock();

    if (src && dst)
    {
        std::vector<VkBufferCopy> copyInfos;
        GetCopyInfos(true, copyInfos);

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

void VertexCollector::GetCopyInfos(bool isStatic, std::vector<VkBufferCopy> &outInfos) const
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

    outInfos.push_back({ offsetPositions,offsetPositions,curVertexCount * properties.positionStride });
    outInfos.push_back({ offsetNormals,offsetNormals,curVertexCount * properties.normalStride });
    outInfos.push_back({ offsetTexCoords,offsetTexCoords,curVertexCount * properties.texCoordStride });
    outInfos.push_back({ offsetColors,offsetColors,curVertexCount * properties.colorStride });
    outInfos.push_back({ offsetMaterials,offsetMaterials,(curIndexCount / 3) * sizeof(RgLayeredMaterial) });
}

void VertexCollector::UpdateTransform(uint32_t geomIndex, const RgTransform &transform)
{
    assert(geomIndex < rgTypes.size() && geomIndex < transforms.size());

    if (rgTypes[geomIndex] != RG_GEOMETRY_TYPE_STATIC)
    {
        memcpy(&transforms[geomIndex], &transform, sizeof(RgTransform));
    }
}

void VertexCollector::PushGeometryType(RgGeometryType type,
                                       const VkAccelerationStructureCreateGeometryTypeInfoKHR &geomType)
{
    asGeometryTypes.push_back(geomType);
}

void VertexCollector::PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR &geom)
{
    asGeometries.push_back(geom);
}

void VertexCollector::PushOffsetInfo(RgGeometryType type, const VkAccelerationStructureBuildOffsetInfoKHR &offsetInfo)
{
    asBuildOffsetInfos.push_back(offsetInfo);
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollector::GetASGeometries() const
{
    return asGeometries;
}

const std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> &VertexCollector::GetASGeometryTypes() const
{
    return asGeometryTypes;
}

const std::vector<VkAccelerationStructureBuildOffsetInfoKHR> &VertexCollector::GetASBuildOffsetInfos() const
{
    return asBuildOffsetInfos;
}