#include "VertexCollector.h"
#include "Generated/ShaderCommonC.h"
#include "Matrix.h"

VertexCollector::VertexCollector(
    VkDevice device, const std::shared_ptr<PhysicalDevice> &physDevice, 
    VkDeviceSize bufferSize, const VertexBufferProperties &properties) :
    properties({}),
    mappedVertexData(nullptr), mappedIndexData(nullptr), mappedTransformData(nullptr), mappedGeomInfosData(nullptr),
    curVertexCount(0), curIndexCount(0), curPrimitiveCount(0), curGeometryCount(0)
{
    this->device = device;
    this->properties = properties;

    // vertex buffers
    stagingVertBuffer.Init(
        device, *physDevice, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "Vertices data staging buffer");

    vertBuffer.Init(
        device, *physDevice, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "Vertices data device local buffer");

    // index buffers
    stagingIndexBuffer.Init(
        device, *physDevice, MAX_VERTEX_COLLECTOR_INDEX_COUNT * sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "Index data staging buffer");

    indexBuffer.Init(
        device, *physDevice, MAX_VERTEX_COLLECTOR_INDEX_COUNT * sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "Index data device local buffer");

    // transforms buffer
    transforms.Init(
        device, *physDevice, MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT * sizeof(VkTransformMatrixKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "Vertex collector transforms buffer");

    geomInfosBuffer.Init(
        device, *physDevice, MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT * sizeof(ShGeometryInstance),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "BLAS geometry info buffer");

    mappedVertexData = static_cast<uint8_t *>(stagingVertBuffer.Map());
    mappedIndexData = static_cast<uint32_t *>(stagingIndexBuffer.Map());
    mappedTransformData = static_cast<VkTransformMatrixKHR *>(transforms.Map());
    mappedGeomInfosData = static_cast<ShGeometryInstance *>(geomInfosBuffer.Map());
}

VertexCollector::~VertexCollector()
{
    // unmap buffers to destroy them 
    stagingVertBuffer.TryUnmap();
    stagingIndexBuffer.TryUnmap();
    transforms.TryUnmap();
    geomInfosBuffer.TryUnmap();
}

void VertexCollector::BeginCollecting()
{
    assert(curVertexCount == 0 && curIndexCount == 0 && curPrimitiveCount == 0 && curGeometryCount == 0);
    assert(asGeometries.empty() && asBuildRangeInfos.empty());
}

uint32_t VertexCollector::AddGeometry(const RgGeometryUploadInfo &info)
{
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
        curIndexCount += info.indexCount;
    }

    assert(curVertexCount < maxVertexCount);
    assert(curIndexCount < MAX_VERTEX_COLLECTOR_INDEX_COUNT);
    assert(curGeometryCount < MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT);

    if (curVertexCount >= maxVertexCount ||
        curIndexCount >= MAX_VERTEX_COLLECTOR_INDEX_COUNT ||
        curGeometryCount >= MAX_VERTEX_COLLECTOR_TRANSFORMS_COUNT)
    {
        return UINT32_MAX;
    }

    // copy data to buffer
    assert(stagingVertBuffer.IsMapped());
    CopyDataToStaging(info, vertIndex, collectStatic);

    if (useIndices)
    {
        assert(stagingIndexBuffer.IsMapped());
        memcpy(mappedIndexData + indIndex, info.indexData, info.indexCount * sizeof(uint32_t));
    }

    memcpy(mappedTransformData + geomIndex, &info.transform, sizeof(RgTransform));

    const uint32_t offsetPositions = collectStatic ?
        offsetof(ShVertexBufferStatic, positions) :
        offsetof(ShVertexBufferDynamic, positions);

    // use positions and index data in the device local buffers: AS shouldn't be built using staging buffers
    const VkDeviceAddress vertexDataDeviceAddress =
        vertBuffer.GetAddress() + offsetPositions + vertIndex * static_cast<uint64_t>(properties.positionStride);

    // geometry info
    VkAccelerationStructureGeometryKHR geom = {};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

    VkAccelerationStructureGeometryTrianglesDataKHR &trData = geom.geometry.triangles;
    trData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    trData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    trData.maxVertex = info.vertexCount;
    trData.vertexData.deviceAddress = vertexDataDeviceAddress;
    trData.vertexStride = properties.positionStride;
    trData.transformData.deviceAddress = transforms.GetAddress() + geomIndex * sizeof(RgTransform);

    if (useIndices)
    {
        const VkDeviceAddress indexDataDeviceAddress =
            indexBuffer.GetAddress() + indIndex * sizeof(uint32_t);

        trData.indexType = VK_INDEX_TYPE_UINT32;
        trData.indexData.deviceAddress = indexDataDeviceAddress;
    }
    else
    {
        trData.indexType = VK_INDEX_TYPE_NONE_KHR;
        trData.indexData = {};
    }

    PushGeometry(info.geomType, geom);

    // geomIndex must be the same as in pGeometries in BLAS
    // for referencing it in shaders by gl_GeometryIndexEXT (RayGeometryIndexKHR)
    assert(geomIndex == GetGeometryCount() - 1);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;
    PushRangeInfo(info.geomType, rangeInfo);

    PushPrimitiveCount(info.geomType, primitiveCount);

    // copy geom info
    assert(geomInfosBuffer.IsMapped());

    ShGeometryInstance geomInfo = {};
    geomInfo.baseVertexIndex = vertIndex;
    geomInfo.baseIndexIndex = useIndices ? indIndex : UINT32_MAX;
    geomInfo.primitiveCount = primitiveCount;
    // RgTexture is union, all textures indices are unique even with different types
    geomInfo.materialId0 = info.geomMaterial.layerTextures[0].staticTexture;
    geomInfo.materialId1 = info.geomMaterial.layerTextures[1].staticTexture;
    geomInfo.materialId2 = info.geomMaterial.layerTextures[2].staticTexture;

    Matrix::ToMat4Transposed(geomInfo.model, info.transform);

    assert(sizeof(ShGeometryInstance) % 16 == 0);
    memcpy(mappedGeomInfosData + geomIndex, &geomInfo, sizeof(ShGeometryInstance));

    return geomIndex;
}

void VertexCollector::CopyDataToStaging(const RgGeometryUploadInfo &info, uint32_t vertIndex, bool isStatic)
{
    const uint64_t wholeBufferSize = isStatic ?
        sizeof(ShVertexBufferStatic) :
        sizeof(ShVertexBufferDynamic);

    const uint64_t offsetPositions = isStatic ?
        offsetof(ShVertexBufferStatic, positions) :
        offsetof(ShVertexBufferDynamic, positions);
    const uint64_t offsetNormals = isStatic ?
        offsetof(ShVertexBufferStatic, normals) :
        offsetof(ShVertexBufferDynamic, normals);
    const uint64_t offsetTexCoords = isStatic ?
        offsetof(ShVertexBufferStatic, texCoords) :
        offsetof(ShVertexBufferDynamic, texCoords);
    const uint64_t offsetColors = isStatic ?
        offsetof(ShVertexBufferStatic, colors) :
        offsetof(ShVertexBufferDynamic, colors);
    const uint64_t offsetMaterials = isStatic ?
        offsetof(ShVertexBufferStatic, materialIds) :
        offsetof(ShVertexBufferDynamic, materialIds);

    const uint64_t positionStride = properties.positionStride;
    const uint64_t normalStride = properties.normalStride;
    const uint64_t texCoordStride = properties.texCoordStride;
    const uint64_t colorStride = properties.colorStride;

    // positions
    void *positionsDst = mappedVertexData + offsetPositions + vertIndex * positionStride;
    assert(offsetPositions + (vertIndex + info.vertexCount) * positionStride < wholeBufferSize);

    memcpy(positionsDst, info.vertexData, info.vertexCount * positionStride);

    // normals
    void *normalsDst = mappedVertexData + offsetNormals + vertIndex * normalStride;
    assert(offsetNormals + (vertIndex + info.vertexCount) * normalStride < wholeBufferSize);

    if (info.normalData != nullptr)
    {
        memcpy(normalsDst, info.normalData, info.vertexCount * normalStride);
    }
    else
    {
        // TODO: generate normals
        memset(normalsDst, 0, info.vertexCount * normalStride);
    }

    // tex coords
    void *texCoordDst = mappedVertexData + offsetTexCoords + vertIndex * texCoordStride;
    assert(offsetTexCoords + (vertIndex + info.vertexCount) * texCoordStride < wholeBufferSize);

    if (info.texCoordData != nullptr)
    {
        memcpy(texCoordDst, info.texCoordData, info.vertexCount * texCoordStride);
    }
    else
    {
        memset(texCoordDst, 0, info.vertexCount * texCoordStride);
    }

    // colors
    void *colorDst = mappedVertexData + offsetColors + info.vertexCount * colorStride;
    assert(offsetColors + (info.vertexCount + info.vertexCount) * colorStride < wholeBufferSize);

    if (info.colorData != nullptr)
    {
        memcpy(colorDst, info.colorData, info.vertexCount * colorStride);
    }
    else
    {
        // set white color
        memset(colorDst, 0xFF, info.vertexCount * colorStride);
    }

    const bool useIndices = info.indexCount != 0 && info.indexData != nullptr;
    const uint32_t triangleCount = useIndices ? info.indexCount / 3 : info.vertexCount / 3;

    // materials
    /*void *matDst = mappedVertexData + offsetMaterials + curPrimitiveCount * sizeof(RgLayeredMaterial);
    assert(offsetMaterials + (curPrimitiveCount + triangleCount) * sizeof(RgLayeredMaterial) < wholeBufferSize);

    if (info.triangleMaterials != nullptr)
    {
        memcpy(matDst, info.triangleMaterials, triangleCount * sizeof(RgLayeredMaterial));
    }
    else
    {
        // TODO: info.geomMaterial
        memset(matDst, RG_NO_TEXTURE, triangleCount * sizeof(RgLayeredMaterial));
    }*/
}


void VertexCollector::EndCollecting()
{}

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

bool VertexCollector::CopyVertexDataFromStaging(VkCommandBuffer cmd, bool isStatic)
{
    std::array<VkBufferCopy, 5> vertCopyInfos = {};
    bool hasInfo = GetVertBufferCopyInfos(isStatic, vertCopyInfos);

    if (!hasInfo)
    {
        return false;
    }

    vkCmdCopyBuffer(
        cmd,
        stagingVertBuffer.GetBuffer(), vertBuffer.GetBuffer(),
        vertCopyInfos.size(), vertCopyInfos.data());

    return true;
}

bool VertexCollector::CopyIndexDataFromStaging(VkCommandBuffer cmd)
{
    if (curIndexCount == 0)
    {
        return false;
    }

    VkBufferCopy indexCopyInfo = {};
    indexCopyInfo.srcOffset = 0;
    indexCopyInfo.dstOffset = 0;
    indexCopyInfo.size = curIndexCount * sizeof(uint32_t);

    vkCmdCopyBuffer(
        cmd,
        stagingIndexBuffer.GetBuffer(), indexBuffer.GetBuffer(),
        1, &indexCopyInfo);

    return true;
}

void VertexCollector::CopyFromStaging(VkCommandBuffer cmd, bool isStatic)
{
    bool vrtCopied = CopyVertexDataFromStaging(cmd, isStatic);
    bool indCopied = CopyIndexDataFromStaging(cmd);

    // sync dst buffer access
    VkBufferMemoryBarrier barriers[2] = {};
    uint32_t barrierCount = 0;

    if (vrtCopied)
    {
        VkBufferMemoryBarrier &vrtBr = barriers[barrierCount++];
        vrtBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vrtBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vrtBr.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vrtBr.buffer = vertBuffer.GetBuffer();
        vrtBr.size = VK_WHOLE_SIZE;
    }

    if (indCopied)
    {
        VkBufferMemoryBarrier &indBr = barriers[barrierCount++];
        indBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        indBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        indBr.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        indBr.buffer = indexBuffer.GetBuffer();
        indBr.size = curIndexCount * sizeof(uint32_t);
    }

    if (barrierCount > 0)
    {
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, nullptr,
            barrierCount, barriers,
            0, nullptr);
    }
}

bool VertexCollector::GetVertBufferCopyInfos(bool isStatic, std::array<VkBufferCopy, 5> &outInfos) const
{
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

    if (curVertexCount == 0 || curPrimitiveCount == 0)
    {
        return false;
    }

    outInfos[0] = { offsetPositions,    offsetPositions,    curVertexCount * properties.positionStride };
    outInfos[1] = { offsetNormals,      offsetNormals,      curVertexCount * properties.normalStride };
    outInfos[2] = { offsetTexCoords,    offsetTexCoords,    curVertexCount * properties.texCoordStride };
    outInfos[3] = { offsetColors,       offsetColors,       curVertexCount * properties.colorStride };
    outInfos[4] = { offsetMaterials,    offsetMaterials,    curPrimitiveCount * sizeof(RgLayeredMaterial) };

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

uint32_t VertexCollector::GetGeometryCount() const
{
    return asGeometries.size();
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollector::GetASGeometries() const
{
    return asGeometries;
}

const std::vector<VkAccelerationStructureBuildRangeInfoKHR> &VertexCollector::GetASBuildRangeInfos() const
{
    return asBuildRangeInfos;
}

VkBuffer VertexCollector::GetVertexBuffer() const
{
    return vertBuffer.GetBuffer();
}

VkBuffer VertexCollector::GetIndexBuffer() const
{
    return indexBuffer.GetBuffer();
}

VkBuffer VertexCollector::GetGeometryInfosBuffer() const
{
    return geomInfosBuffer.GetBuffer();
}
