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

#include "VertexCollector.h"
#include "Generated/ShaderCommonC.h"
#include "Matrix.h"

using namespace RTGL1;

constexpr uint32_t INDEX_BUFFER_SIZE        = MAX_VERTEX_COLLECTOR_INDEX_COUNT * sizeof(uint32_t);
constexpr uint32_t TRANSFORM_BUFFER_SIZE    = MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * sizeof(VkTransformMatrixKHR);
constexpr uint32_t GEOM_INFO_BUFFER_SIZE    = MAX_TOP_LEVEL_INSTANCE_COUNT * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * sizeof(ShGeometryInstance);

constexpr uint64_t OFFSET_TEX_COORDS_STATIC[] =
{
    offsetof(ShVertexBufferStatic, texCoords),
    offsetof(ShVertexBufferStatic, texCoordsLayer1),
    offsetof(ShVertexBufferStatic, texCoordsLayer2),
};

constexpr uint64_t OFFSET_TEX_COORDS_DYNAMIC[] =
{
    offsetof(ShVertexBufferDynamic, texCoords),
};

constexpr uint32_t TEXCOORD_LAYER_COUNT_STATIC = sizeof(OFFSET_TEX_COORDS_STATIC) / sizeof(OFFSET_TEX_COORDS_STATIC[0]);
constexpr uint32_t TEXCOORD_LAYER_COUNT_DYNAMIC = sizeof(OFFSET_TEX_COORDS_DYNAMIC) / sizeof(OFFSET_TEX_COORDS_DYNAMIC[0]);

constexpr uint32_t MAX_VERTEX_BUFFER_MEMBER_COUNT = 8;


VertexCollector::VertexCollector(
    VkDevice _device, 
    const std::shared_ptr<MemoryAllocator> &_allocator,
    VkDeviceSize _bufferSize,
    const VertexBufferProperties &_properties,
    VertexCollectorFilterTypeFlags _filters) 
:
    device(_device),
    properties(_properties),
    filtersFlags(_filters),
    geomInfosCopyRegions{},
    curVertexCount(0), curIndexCount(0), curPrimitiveCount(0), curGeometryCount(0),
    mappedVertexData(nullptr), mappedIndexData(nullptr), mappedTransformData(nullptr), mappedGeomInfosData(nullptr)
{
    assert(filtersFlags != 0);

    vertBuffer = std::make_shared<Buffer>();
    indexBuffer = std::make_shared<Buffer>();
    transformsBuffer = std::make_shared<Buffer>();
    geomInfosBuffer = std::make_shared<Buffer>();

    // vertex buffers
    vertBuffer->Init(
        _allocator, _bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? "Dynamic Vertices data buffer" : "Static Vertices data buffer");

    // index buffers
    indexBuffer->Init(
        _allocator, INDEX_BUFFER_SIZE,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? "Dynamic Index data buffer" : "Static Index data buffer");

    // transforms buffer
    transformsBuffer->Init(
        _allocator, TRANSFORM_BUFFER_SIZE,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? "Dynamic BLAS transforms buffer" : "Static BLAS transforms buffer");

    // geometry instance info for each geometry in each top level instance
    geomInfosBuffer->Init(
        _allocator, GEOM_INFO_BUFFER_SIZE,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? "Dynamic Geometry info buffer" : "Static Geometry info buffer");

    // device local buffers are 
    InitStagingBuffers(_allocator);
    InitFilters(filtersFlags);
}

VertexCollector::VertexCollector(
    const std::shared_ptr<const VertexCollector> &_src,
    const std::shared_ptr<MemoryAllocator> &_allocator)
:
    device(_src->device),
    properties(_src->properties),
    filtersFlags(_src->filtersFlags),
    vertBuffer(_src->vertBuffer),
    indexBuffer(_src->indexBuffer),
    transformsBuffer(_src->transformsBuffer),
    geomInfosBuffer(_src->geomInfosBuffer),
    geomInfosCopyRegions{},
    curVertexCount(0), curIndexCount(0), curPrimitiveCount(0), curGeometryCount(0),
    mappedVertexData(nullptr), mappedIndexData(nullptr), mappedTransformData(nullptr), mappedGeomInfosData(nullptr)
{
    // device local buffers are shared with the "src" vertex collector
    InitStagingBuffers(_allocator);
    InitFilters(filtersFlags);
}

void VertexCollector::InitStagingBuffers(const std::shared_ptr<MemoryAllocator> &allocator)
{
    // device local buffers must not be empty
    assert(vertBuffer       && vertBuffer->GetSize() > 0);
    assert(indexBuffer      && indexBuffer->GetSize() > 0);
    assert(transformsBuffer && transformsBuffer->GetSize() > 0);
    assert(geomInfosBuffer  && geomInfosBuffer->GetSize() > 0);

    static_assert(sizeof(geomInfosCopyRegions) / sizeof(geomInfosCopyRegions[0]) == MAX_TOP_LEVEL_INSTANCE_COUNT, "Number of geomInfo copy regions must be MAX_TOP_LEVEL_INSTANCE_COUNT");

    // vertex buffers
    stagingVertBuffer.Init(
        allocator, vertBuffer->GetSize(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? "Dynamic Vertices data staging buffer" : "Static Vertices data staging buffer");

    // index buffers
    stagingIndexBuffer.Init(
        allocator, indexBuffer->GetSize(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? "Dynamic Index data staging buffer" : "Static Index data staging buffer");

    // transforms buffer
    stagingTransformsBuffer.Init(
        allocator, transformsBuffer->GetSize(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? "Dynamic BLAS transforms staging buffer" : "Static BLAS transforms staging buffer");

    // geometry instance info for each geometry in each top level instance
    stagingGeomInfosBuffer.Init(
        allocator, geomInfosBuffer->GetSize(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC ? "Dynamic Geometry info staging buffer" : "Static Geometry info staging buffer");

    mappedVertexData = static_cast<uint8_t *>(stagingVertBuffer.Map());
    mappedIndexData = static_cast<uint32_t *>(stagingIndexBuffer.Map());
    mappedTransformData = static_cast<VkTransformMatrixKHR *>(stagingTransformsBuffer.Map());
    mappedGeomInfosData = static_cast<ShGeometryInstance *>(stagingGeomInfosBuffer.Map());
}

VertexCollector::~VertexCollector()
{
    // unmap buffers to destroy them 
    stagingVertBuffer.TryUnmap();
    stagingIndexBuffer.TryUnmap();
    stagingTransformsBuffer.TryUnmap();
    stagingGeomInfosBuffer.TryUnmap();
}

void VertexCollector::BeginCollecting()
{
    assert(curVertexCount == 0 && curIndexCount == 0 && curPrimitiveCount == 0 && curGeometryCount == 0);
    assert(GetAllGeometryCount() == 0);
}

uint32_t VertexCollector::AddGeometry(const RgGeometryUploadInfo &info, const MaterialTextures materials[MATERIALS_MAX_LAYER_COUNT])
{
    typedef VertexCollectorFilterTypeFlagBits FT;
    VertexCollectorFilterTypeFlags geomFlags = VertexCollectorFilterTypeFlags_GetForGeometry(info);

    const bool collectStatic = geomFlags & (FT::CF_STATIC_NON_MOVABLE | FT::CF_STATIC_MOVABLE);
    
    const uint32_t maxVertexCount = collectStatic ?
        MAX_STATIC_VERTEX_COUNT :
        MAX_DYNAMIC_VERTEX_COUNT;

    const uint32_t geomIndex = curGeometryCount;
    const uint32_t vertIndex = curVertexCount;
    const uint32_t indIndex = curIndexCount;

    geomType[geomIndex] = geomFlags;

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
    assert(curGeometryCount < MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT);

    if (curVertexCount >= maxVertexCount ||
        curIndexCount >= MAX_VERTEX_COLLECTOR_INDEX_COUNT ||
        curGeometryCount >= MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT)
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

    static_assert(sizeof(RgTransform) == sizeof(VkTransformMatrixKHR), "RgTransform and VkTransformMatrixKHR must have the same structure to be used in AS building");
    memcpy(mappedTransformData + geomIndex, &info.transform, sizeof(VkTransformMatrixKHR));

    const uint32_t offsetPositions = collectStatic ?
        offsetof(ShVertexBufferStatic, positions) :
        offsetof(ShVertexBufferDynamic, positions);

    // use positions and index data in the device local buffers: AS shouldn't be built using staging buffers
    const VkDeviceAddress vertexDataDeviceAddress =
        vertBuffer->GetAddress() + offsetPositions + vertIndex * static_cast<uint64_t>(properties.positionStride);

    // geometry info
    VkAccelerationStructureGeometryKHR geom = {};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

    geom.flags = geomFlags & FT::PT_OPAQUE ?
        VK_GEOMETRY_OPAQUE_BIT_KHR :
        VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;

    VkAccelerationStructureGeometryTrianglesDataKHR &trData = geom.geometry.triangles;
    trData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    trData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    trData.maxVertex = info.vertexCount;
    trData.vertexData.deviceAddress = vertexDataDeviceAddress;
    trData.vertexStride = properties.positionStride;
    trData.transformData.deviceAddress = transformsBuffer->GetAddress() + geomIndex * sizeof(VkTransformMatrixKHR);

    if (useIndices)
    {
        const VkDeviceAddress indexDataDeviceAddress =
            indexBuffer->GetAddress() + indIndex * sizeof(uint32_t);

        trData.indexType = VK_INDEX_TYPE_UINT32;
        trData.indexData.deviceAddress = indexDataDeviceAddress;
    }
    else
    {
        trData.indexType = VK_INDEX_TYPE_NONE_KHR;
        trData.indexData = {};
    }

    uint32_t localIndex = PushGeometry(geomFlags, geom);
    geomLocalIndex[geomIndex] = localIndex;

    // geomIndex must be the same as in pGeometries in BLAS,
    // geomIndex is a global index. For referencing this geometry's info,
    // the pair of gl_InstanceID and gl_GeometryIndexEXT is used
    assert(geomIndex == GetAllGeometryCount() - 1);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;
    PushRangeInfo(geomFlags, rangeInfo);

    PushPrimitiveCount(geomFlags, primitiveCount);

    {
        // copy geom info
        assert(stagingGeomInfosBuffer.IsMapped());

        static_assert(sizeof(ShGeometryInstance) % 16 == 0, "Std430 structs must be aligned by 16 bytes");

        ShGeometryInstance geomInfo = {};
        geomInfo.baseVertexIndex = vertIndex;
        geomInfo.baseIndexIndex = useIndices ? indIndex : UINT32_MAX;
        geomInfo.primitiveCount = primitiveCount;
        geomInfo.defaultRoughness = info.defaultRoughness;
        geomInfo.defaultMetallicity = info.defaultMetallicity;
        geomInfo.defaultEmission = info.defaultEmission;

        memcpy(geomInfo.color, info.color, sizeof(geomInfo.color));

        Matrix::ToMat4Transposed(geomInfo.model, info.transform);

        static_assert(sizeof(info.geomMaterial.layerMaterials) / sizeof(info.geomMaterial.layerMaterials[0]) == MATERIALS_MAX_LAYER_COUNT,
                      "Layer count must be MATERIALS_MAX_LAYER_COUNT");

        for (uint32_t layer = 0; layer < MATERIALS_MAX_LAYER_COUNT; layer++)
        {
            // only for static geometry, dynamic is updated each frame,
            // so the materials will be updated anyway
            if (collectStatic)
            {
                uint32_t materialIndex = info.geomMaterial.layerMaterials[layer];
                AddMaterialDependency(geomIndex, layer, materialIndex);
            }

            memcpy(geomInfo.materials[layer], materials[layer].indices,
                   TEXTURES_PER_MATERIAL_COUNT * sizeof(uint32_t));
        }

        WriteGeomInfo(geomIndex, geomInfo);

        // mark to be copied from staging
        MarkGeomInfoIndexToCopy(geomIndex);
    }

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

    const uint64_t positionStride = properties.positionStride;
    const uint64_t normalStride = properties.normalStride;
    const uint64_t texCoordStride = properties.texCoordStride;

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

    //const bool useIndices = info.indexCount != 0 && info.indexData != nullptr;
    //const uint32_t triangleCount = useIndices ? info.indexCount / 3 : info.vertexCount / 3;

    // additional tex coords for static geometry
    const uint64_t *offsetTexCoords = isStatic ? OFFSET_TEX_COORDS_STATIC : OFFSET_TEX_COORDS_DYNAMIC;
    uint32_t        offsetCount     = isStatic ? TEXCOORD_LAYER_COUNT_STATIC : TEXCOORD_LAYER_COUNT_DYNAMIC;

    for (uint32_t i = 0; i < offsetCount; i++)
    {
        void *texCoordDst = mappedVertexData + offsetTexCoords[i] + vertIndex * texCoordStride;
        assert(offsetTexCoords[i] + (vertIndex + info.vertexCount) * texCoordStride < wholeBufferSize);

        if (info.texCoordLayerData[i] != nullptr)
        {
            memcpy(texCoordDst, info.texCoordLayerData[i], info.vertexCount * texCoordStride);
        }
        else
        {
            memset(texCoordDst, 0, info.vertexCount * texCoordStride);
        }
    }
}


void VertexCollector::EndCollecting()
{}

void VertexCollector::Reset()
{
    curVertexCount = 0;
    curIndexCount = 0;
    curPrimitiveCount = 0;
    curGeometryCount = 0;

    geomType.clear();
    geomLocalIndex.clear();
    materialDependencies.clear();

    for (uint32_t type = 0; type < MAX_TOP_LEVEL_INSTANCE_COUNT; type++)
    {
        geomInfosCopyRegions[type] = 0;
    }

    for (auto &f : filters)
    {
        f.second->Reset();
    }
}

bool VertexCollector::CopyVertexDataFromStaging(VkCommandBuffer cmd, bool isStatic)
{
    std::array<VkBufferCopy, MAX_VERTEX_BUFFER_MEMBER_COUNT> vertCopyInfos = {};

    uint32_t count;
    bool hasInfo = GetVertBufferCopyInfos(isStatic, vertCopyInfos.data(), &count);

    if (!hasInfo)
    {
        return false;
    }

    vkCmdCopyBuffer(
        cmd,
        stagingVertBuffer.GetBuffer(), vertBuffer->GetBuffer(),
        count, vertCopyInfos.data());

    return true;
}

bool VertexCollector::CopyIndexDataFromStaging(VkCommandBuffer cmd)
{
    if (curIndexCount == 0)
    {
        return false;
    }

    VkBufferCopy info = {};
    info.srcOffset = 0;
    info.dstOffset = 0;
    info.size = curIndexCount * sizeof(uint32_t);

    vkCmdCopyBuffer(
        cmd,
        stagingIndexBuffer.GetBuffer(), indexBuffer->GetBuffer(),
        1, &info);

    return true;
}

bool VertexCollector::CopyGeometryInfosFromStaging(VkCommandBuffer cmd)
{
    VkBufferCopy infos[MAX_TOP_LEVEL_INSTANCE_COUNT];
    uint32_t infoCount = 0;

    for (uint32_t type = 0; type < MAX_TOP_LEVEL_INSTANCE_COUNT; type++)
    {
        if (geomInfosCopyRegions[type] > 0)
        {
            uint64_t typeOffset = sizeof(ShGeometryInstance) * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * type;

            infos[infoCount].srcOffset = typeOffset;
            infos[infoCount].dstOffset = typeOffset;
            infos[infoCount].size = geomInfosCopyRegions[type];

            infoCount++;
        }
    }

    if (infoCount == 0)
    {
        return false;
    }

    vkCmdCopyBuffer(
        cmd,
        stagingGeomInfosBuffer.GetBuffer(), geomInfosBuffer->GetBuffer(),
        infoCount, infos);

    return true;
}

bool VertexCollector::CopyTransformsFromStaging(VkCommandBuffer cmd, bool insertMemBarrier)
{
    if (curGeometryCount == 0)
    {
        return false;
    }

    VkBufferCopy info = {};
    info.srcOffset = 0;
    info.dstOffset = 0;
    info.size = curGeometryCount * sizeof(VkTransformMatrixKHR);

    vkCmdCopyBuffer(
        cmd,
        stagingTransformsBuffer.GetBuffer(), transformsBuffer->GetBuffer(),
        1, &info);

    if (insertMemBarrier)
    {
        VkBufferMemoryBarrier trnBr = {};
        trnBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        trnBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        trnBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        trnBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        trnBr.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        trnBr.buffer = transformsBuffer->GetBuffer();
        trnBr.size = curGeometryCount * sizeof(VkTransformMatrixKHR);

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            0, nullptr,
            1, &trnBr,
            0, nullptr);
    }

    return true;
}

bool VertexCollector::CopyTransformsFromStaging(VkCommandBuffer cmd)
{
    return CopyTransformsFromStaging(cmd, true);
}

bool VertexCollector::CopyFromStaging(VkCommandBuffer cmd, bool isStaticVertexData)
{
    bool vrtCopied = CopyVertexDataFromStaging(cmd, isStaticVertexData);
    bool indCopied = CopyIndexDataFromStaging(cmd);
    bool gmtCopied = CopyGeometryInfosFromStaging(cmd);
    bool trnCopied = CopyTransformsFromStaging(cmd, false);

    // sync dst buffer access
    VkBufferMemoryBarrier barriers[4] = {};
    uint32_t barrierCount = 0;

    if (vrtCopied)
    {
        VkBufferMemoryBarrier &vrtBr = barriers[barrierCount++];

        vrtBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vrtBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vrtBr.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
        vrtBr.buffer = vertBuffer->GetBuffer();
        vrtBr.size = VK_WHOLE_SIZE;
    }

    if (indCopied)
    {
        VkBufferMemoryBarrier &indBr = barriers[barrierCount++];

        indBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        indBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        indBr.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
        indBr.buffer = indexBuffer->GetBuffer();
        indBr.size = curIndexCount * sizeof(uint32_t);
    }

    if (gmtCopied)
    {
        VkBufferMemoryBarrier &gmtBr = barriers[barrierCount++];

        gmtBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        gmtBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        gmtBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        gmtBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        gmtBr.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        gmtBr.buffer = geomInfosBuffer->GetBuffer();
        gmtBr.size = VK_WHOLE_SIZE;
    }

    if (trnCopied)
    {
        VkBufferMemoryBarrier &trnBr = barriers[barrierCount++];

        trnBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        trnBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        trnBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        trnBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        trnBr.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        trnBr.buffer = transformsBuffer->GetBuffer();
        trnBr.size = curGeometryCount * sizeof(VkTransformMatrixKHR);
    }

    if (barrierCount > 0)
    {
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0,
            0, nullptr,
            barrierCount, barriers,
            0, nullptr);

        return true;
    }

    return false;
}

bool VertexCollector::GetVertBufferCopyInfos(bool isStatic, VkBufferCopy *pOutInfos, uint32_t *outCount) const
{
    if (curVertexCount == 0 || curPrimitiveCount == 0)
    {
        return false;
    }

    const uint32_t offsetPositions = isStatic ?
        offsetof(ShVertexBufferStatic, positions) :
        offsetof(ShVertexBufferDynamic, positions);

    const uint32_t offsetNormals = isStatic ?
        offsetof(ShVertexBufferStatic, normals) :
        offsetof(ShVertexBufferDynamic, normals);

    const uint64_t *offsetTexCoords = isStatic ? OFFSET_TEX_COORDS_STATIC : OFFSET_TEX_COORDS_DYNAMIC;
    uint32_t        offsetCount     = isStatic ? TEXCOORD_LAYER_COUNT_STATIC : TEXCOORD_LAYER_COUNT_DYNAMIC;

    uint32_t count = 0;

    pOutInfos[count++] = { offsetPositions,    offsetPositions,    (uint64_t)curVertexCount * properties.positionStride };
    pOutInfos[count++] = { offsetNormals,      offsetNormals,      (uint64_t)curVertexCount * properties.normalStride };

    if (MAX_VERTEX_BUFFER_MEMBER_COUNT < count + offsetCount)
    {
        assert(0);
    }

    for (uint32_t i = 0; i < offsetCount; i++)
    {
        pOutInfos[count++] = { offsetTexCoords[i], offsetTexCoords[i], (uint64_t)curVertexCount * properties.texCoordStride };
    }

    *outCount = count;
    return true;
}

void VertexCollector::UpdateTransform(uint32_t geomIndex, const RgTransform &transform)
{
    assert(geomIndex < MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT);
    assert(mappedTransformData != nullptr);

    static_assert(sizeof(RgTransform) == sizeof(VkTransformMatrixKHR), "RgTransform and VkTransformMatrixKHR must have the same structure to be used in AS building");
    memcpy(mappedTransformData + geomIndex, &transform, sizeof(VkTransformMatrixKHR));

    WriteGeomInfoTransform(geomIndex, transform);
}

void VertexCollector::AddMaterialDependency(uint32_t geomIndex, uint32_t layer, uint32_t materialIndex)
{
    // ignore empty materials
    if (materialIndex != RG_NO_MATERIAL)
    {
        auto it = materialDependencies.find(materialIndex);

        if (it == materialDependencies.end())
        {
            materialDependencies[materialIndex] = {};
            it = materialDependencies.find(materialIndex);
        }

        it->second.push_back({ geomIndex, layer });
    }
}
void VertexCollector::OnMaterialChange(uint32_t materialIndex, const MaterialTextures &newInfo)
{
    // for each geom index that has this material, update geometry instance infos
    for (const auto &p : materialDependencies[materialIndex])
    {    
        assert(p.geomIndex < curGeometryCount);

        WriteGeomInfoMaterials(p.geomIndex, p.layer, newInfo);
    }
}

ShGeometryInstance *VertexCollector::GetGeomInfoAddress(uint32_t geomIndex)
{
    assert(mappedGeomInfosData != nullptr);
    assert(geomType.find(geomIndex) != geomType.end());
    assert(geomLocalIndex.find(geomIndex) != geomLocalIndex.end());

    VertexCollectorFilterTypeFlags typeFlags = geomType[geomIndex];
    uint32_t localIndex = geomLocalIndex[geomIndex];

    uint32_t offset = VertexCollectorFilterTypeFlags_ToOffset(typeFlags);
    assert(offset < MAX_TOP_LEVEL_INSTANCE_COUNT);

    return &mappedGeomInfosData[offset * MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT + localIndex];
}

void VertexCollector::MarkGeomInfoIndexToCopy(uint32_t geomIndex)
{
    assert(mappedGeomInfosData != nullptr);
    assert(geomType.find(geomIndex) != geomType.end());
    assert(geomLocalIndex.find(geomIndex) != geomLocalIndex.end());

    VertexCollectorFilterTypeFlags typeFlags = geomType[geomIndex];
    uint32_t localIndex = geomLocalIndex[geomIndex];

    uint32_t offset = VertexCollectorFilterTypeFlags_ToOffset(typeFlags);
    assert(offset < MAX_TOP_LEVEL_INSTANCE_COUNT);

    // make sure that old value was for previous geometry of the same type
    assert(geomInfosCopyRegions[offset] == localIndex * sizeof(ShGeometryInstance));

    geomInfosCopyRegions[offset] = (localIndex + 1) * sizeof(ShGeometryInstance);
}

void VertexCollector::WriteGeomInfo(uint32_t geomIndex, const ShGeometryInstance &src)
{
    ShGeometryInstance *dst = GetGeomInfoAddress(geomIndex);

    memcpy(dst, &src, sizeof(ShGeometryInstance));
}

void VertexCollector::WriteGeomInfoMaterials(uint32_t geomIndex, uint32_t layer, const MaterialTextures &src)
{
    ShGeometryInstance *dst = GetGeomInfoAddress(geomIndex);

    memcpy(dst->materials[layer], src.indices, TEXTURES_PER_MATERIAL_COUNT * sizeof(uint32_t));
}

void VertexCollector::WriteGeomInfoTransform(uint32_t geomIndex, const RgTransform &src)
{
    ShGeometryInstance *dst = GetGeomInfoAddress(geomIndex);

    float modelMatix[16];
    Matrix::ToMat4Transposed(modelMatix, src);

    memcpy(dst->model, modelMatix, 16 * sizeof(float));
}


VkBuffer VertexCollector::GetVertexBuffer() const
{
    return vertBuffer->GetBuffer();
}

VkBuffer VertexCollector::GetIndexBuffer() const
{
    return indexBuffer->GetBuffer();
}

VkBuffer VertexCollector::GetGeometryInfosBuffer() const
{
    return geomInfosBuffer->GetBuffer();
}

const std::vector<uint32_t> &VertexCollector::GetPrimitiveCounts(
    VertexCollectorFilterTypeFlags filter) const
{
    auto f = filters.find(filter);
    assert(f != filters.end());

    return f->second->GetPrimitiveCounts();
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollector::GetASGeometries(
    VertexCollectorFilterTypeFlags filter) const
{
    auto f = filters.find(filter);
    assert(f != filters.end());

    return f->second->GetASGeometries();
}

const std::vector<VkAccelerationStructureBuildRangeInfoKHR> &VertexCollector::GetASBuildRangeInfos(
    VertexCollectorFilterTypeFlags filter) const
{
    auto f = filters.find(filter);
    assert(f != filters.end());

    return f->second->GetASBuildRangeInfos();
}

bool VertexCollector::AreGeometriesEmpty(VertexCollectorFilterTypeFlags flags) const
{
    for (const auto &p : filters)
    {
        const auto &f = p.second;

        // if filter includes any type from flags
        // and it's not empty
        if ((f->GetFilter() & flags) && f->GetGeometryCount() > 0)
        {
            return false;
        }
    }

    return true;
}

bool VertexCollector::AreGeometriesEmpty(VertexCollectorFilterTypeFlagBits type) const
{
    return AreGeometriesEmpty((VertexCollectorFilterTypeFlags)type);
}

uint32_t VertexCollector::PushGeometry(VertexCollectorFilterTypeFlags type,
                                   const VkAccelerationStructureGeometryKHR &geom)
{
    assert(filters.find(type) != filters.end());

    return filters[type]->PushGeometry(type, geom);
}

void VertexCollector::PushPrimitiveCount(VertexCollectorFilterTypeFlags type, uint32_t primCount)
{
    assert(filters.find(type) != filters.end());

    filters[type]->PushPrimitiveCount(type, primCount);
}

void VertexCollector::PushRangeInfo(VertexCollectorFilterTypeFlags type,
                                    const VkAccelerationStructureBuildRangeInfoKHR &rangeInfo)
{
    assert(filters.find(type) != filters.end());

    filters[type]->PushRangeInfo(type, rangeInfo);
}

uint32_t VertexCollector::GetAllGeometryCount() const
{
    uint32_t count = 0;

    for (const auto &f : filters)
    {
        count += f.second->GetGeometryCount();
    }

    return count;
}

void VertexCollector::AddFilter(VertexCollectorFilterTypeFlags filterGroup)
{
    if (filterGroup == (VertexCollectorFilterTypeFlags)0)
    {
        return;
    }

    assert(filters.find(filterGroup) == filters.end());

    filters[filterGroup] = std::make_shared<VertexCollectorFilter>(filterGroup);
}

// try create filters for each group (mask)
void VertexCollector::InitFilters(VertexCollectorFilterTypeFlags flags)
{
    typedef VertexCollectorFilterTypeFlags FL;
    typedef VertexCollectorFilterTypeFlagBits FT;

    // iterate over all pairs of group bits
    VertexCollectorFilterTypeFlags_IterateOverFlags([this, flags] (FL f)
    {
        // if flags contain this pair of group bits
        if ((flags & f) == f)
        {
            AddFilter(f);
        }
    });
}
