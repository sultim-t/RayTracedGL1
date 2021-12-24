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

#include <algorithm>
#include <cstring>

#include "Generated/ShaderCommonC.h"
#include "Matrix.h"

using namespace RTGL1;

constexpr uint32_t INDEX_BUFFER_SIZE        = MAX_INDEXED_PRIMITIVE_COUNT * 3 * sizeof(uint32_t);
constexpr uint32_t TRANSFORM_BUFFER_SIZE    = MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT * sizeof(VkTransformMatrixKHR);

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


VertexCollector::VertexCollector(
    VkDevice _device, 
    const std::shared_ptr<MemoryAllocator> &_allocator,
    std::shared_ptr<GeomInfoManager> _geomInfoManager,
    std::shared_ptr<TriangleInfoManager> _triangleInfoMgr,
    VkDeviceSize _bufferSize,
    const VertexBufferProperties &_properties,
    VertexCollectorFilterTypeFlags _filters) 
:
    device(_device),
    properties(_properties),
    filtersFlags(_filters),
    geomInfoMgr(std::move(_geomInfoManager)),
    triangleInfoMgr(std::move(_triangleInfoMgr)),
    curVertexCount(0), curIndexCount(0), curPrimitiveCount(0), curTransformCount(0),
    mappedVertexData(nullptr), mappedIndexData(nullptr), mappedTransformData(nullptr), 
    texCoordsToCopyLowerBound(UINT64_MAX),
    texCoordsToCopyUpperBound(0)
{
    assert(filtersFlags != 0);

    bool isDynamic = filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC;

    vertBuffer = std::make_shared<Buffer>();
    indexBuffer = std::make_shared<Buffer>();
    transformsBuffer = std::make_shared<Buffer>();

    // dynamic vertices need also be copied to previous frame buffer
    VkBufferUsageFlags transferUsage = isDynamic ?
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT :
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    // vertex buffers
    vertBuffer->Init(
        _allocator, _bufferSize,
        transferUsage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        isDynamic ? "Dynamic Vertices data buffer" : "Static Vertices data buffer");

    // index buffers
    indexBuffer->Init(
        _allocator, INDEX_BUFFER_SIZE,
        transferUsage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        isDynamic ? "Dynamic Index data buffer" : "Static Index data buffer");

    // transforms buffer
    transformsBuffer->Init(
        _allocator, TRANSFORM_BUFFER_SIZE,
        transferUsage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        isDynamic ? "Dynamic BLAS transforms buffer" : "Static BLAS transforms buffer");

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
    geomInfoMgr(_src->geomInfoMgr),
    triangleInfoMgr(_src->triangleInfoMgr),
    curVertexCount(0), curIndexCount(0), curPrimitiveCount(0), curTransformCount(0),
    mappedVertexData(nullptr), mappedIndexData(nullptr), mappedTransformData(nullptr),
    texCoordsToCopyLowerBound(UINT64_MAX),
    texCoordsToCopyUpperBound(0)
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
    assert(geomInfoMgr && triangleInfoMgr);

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

    mappedVertexData = static_cast<uint8_t *>(stagingVertBuffer.Map());
    mappedIndexData = static_cast<uint32_t *>(stagingIndexBuffer.Map());
    mappedTransformData = static_cast<VkTransformMatrixKHR *>(stagingTransformsBuffer.Map());
}

VertexCollector::~VertexCollector()
{
    // unmap buffers to destroy them 
    stagingVertBuffer.TryUnmap();
    stagingIndexBuffer.TryUnmap();
    stagingTransformsBuffer.TryUnmap();
}

static uint32_t GetMaterialsBlendFlags(const RgGeometryMaterialBlendType blendingTypes[], uint32_t count)
{
    uint32_t r = 0;

    for (uint32_t i = 0; i < count; i++)
    {
        RgGeometryMaterialBlendType b = blendingTypes[i];

        uint32_t bitOffset = MATERIAL_BLENDING_FLAG_BIT_COUNT * i;

        switch (b)
        {
            case RG_GEOMETRY_MATERIAL_BLEND_TYPE_OPAQUE:    r |= MATERIAL_BLENDING_FLAG_OPAQUE  << bitOffset; break;
            case RG_GEOMETRY_MATERIAL_BLEND_TYPE_ALPHA:     r |= MATERIAL_BLENDING_FLAG_ALPHA   << bitOffset; break;
            case RG_GEOMETRY_MATERIAL_BLEND_TYPE_ADD:       r |= MATERIAL_BLENDING_FLAG_ADD     << bitOffset; break;
            case RG_GEOMETRY_MATERIAL_BLEND_TYPE_SHADE:     r |= MATERIAL_BLENDING_FLAG_SHADE   << bitOffset; break;
            default: assert(0); break;
        }
    }

    return r;
}

void VertexCollector::BeginCollecting(bool isStatic)
{
    assert(curVertexCount == 0 && curIndexCount == 0 && curPrimitiveCount == 0 );
    assert((isStatic && geomInfoMgr->GetStaticCount() == 0) || (!isStatic && geomInfoMgr->GetDynamicCount() == 0));
    assert(GetAllGeometryCount() == 0);
}

static uint32_t AlignUpBy3(uint32_t x)
{
    return ((x + 2) / 3) * 3;
}

uint32_t VertexCollector::AddGeometry(uint32_t frameIndex, const RgGeometryUploadInfo &info, const MaterialTextures materials[MATERIALS_MAX_LAYER_COUNT])
{
    typedef VertexCollectorFilterTypeFlagBits FT;
    const VertexCollectorFilterTypeFlags geomFlags = VertexCollectorFilterTypeFlags_GetForGeometry(info);


    // if exceeds a limit of geometries in a group with specified geomFlags
    if (GetGeometryCount(geomFlags) + 1 >= VertexCollectorFilterTypeFlags_GetAmountInGlobalArray(geomFlags))
    {
        assert(false && "Too many geometries in a group");
        return UINT32_MAX;
    }


    const bool collectStatic = geomFlags & (FT::CF_STATIC_NON_MOVABLE | FT::CF_STATIC_MOVABLE);

    const uint32_t maxVertexCount = collectStatic ? MAX_STATIC_VERTEX_COUNT : MAX_DYNAMIC_VERTEX_COUNT;


    const uint32_t vertIndex = AlignUpBy3(curVertexCount);
    const uint32_t indIndex = AlignUpBy3(curIndexCount);
    const uint32_t transformIndex = curTransformCount;

    const bool useIndices = info.indexCount != 0 && info.pIndexData != nullptr;
    const uint32_t primitiveCount = useIndices ? info.indexCount / 3 : info.vertexCount / 3;


    curVertexCount = vertIndex + info.vertexCount;
    curIndexCount = indIndex + (useIndices ? info.indexCount : 0);
    curPrimitiveCount += primitiveCount;
    curTransformCount += 1;


    // check bounds
    if (curVertexCount >= maxVertexCount ||
        curIndexCount >= MAX_INDEXED_PRIMITIVE_COUNT * 3 ||
        (geomInfoMgr->GetCount() + 1) >= MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT)
    {
        assert(0);
        return UINT32_MAX;
    }

    // copy data to buffer
    assert(stagingVertBuffer.IsMapped());
    CopyDataToStaging(info, vertIndex, collectStatic);

    if (useIndices)
    {
        assert(stagingIndexBuffer.IsMapped());
        memcpy(mappedIndexData + indIndex, info.pIndexData, info.indexCount * sizeof(uint32_t));
    }

    static_assert(sizeof(RgTransform) == sizeof(VkTransformMatrixKHR), "RgTransform and VkTransformMatrixKHR must have the same structure to be used in AS building");
    memcpy(mappedTransformData + transformIndex, &info.transform, sizeof(VkTransformMatrixKHR));

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
    trData.transformData.deviceAddress = transformsBuffer->GetAddress() + transformIndex * sizeof(VkTransformMatrixKHR);

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


    VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;
    PushRangeInfo(geomFlags, rangeInfo);


    PushPrimitiveCount(geomFlags, primitiveCount);


    ShGeometryInstance geomInfo = {};
    geomInfo.baseVertexIndex = vertIndex;
    geomInfo.baseIndexIndex = useIndices ? indIndex : UINT32_MAX;
    geomInfo.vertexCount = info.vertexCount;
    geomInfo.indexCount = useIndices ? info.indexCount : UINT32_MAX;
    geomInfo.defaultRoughness = info.defaultRoughness;
    geomInfo.defaultMetallicity = info.defaultMetallicity;
    geomInfo.defaultEmission = info.defaultEmission;

    Matrix::ToMat4Transposed(geomInfo.model, info.transform);

    static_assert(sizeof(info.geomMaterial.layerMaterials) / sizeof(info.geomMaterial.layerMaterials[0]) == MATERIALS_MAX_LAYER_COUNT,
                  "Layer count must be MATERIALS_MAX_LAYER_COUNT");

    geomInfo.flags = GetMaterialsBlendFlags(info.layerBlendingTypes, MATERIALS_MAX_LAYER_COUNT);

    if (info.pNormalData == nullptr)
    {
        geomInfo.flags |= GEOM_INST_FLAG_GENERATE_NORMALS;
    }

    if (info.flags & RG_GEOMETRY_UPLOAD_GENERATE_INVERTED_NORMALS_BIT)
    {
        geomInfo.flags |= GEOM_INST_FLAG_INVERTED_NORMALS;
    }

    if (info.flags & RG_GEOMETRY_UPLOAD_NO_MEDIA_CHANGE_ON_REFRACT_BIT)
    {
        geomInfo.flags |= GEOM_INST_FLAG_NO_MEDIA_CHANGE;
    }

    if (geomFlags & FT::CF_STATIC_MOVABLE)
    {
        geomInfo.flags |= GEOM_INST_FLAG_IS_MOVABLE;
    }

    switch (info.passThroughType)
    {
        case RG_GEOMETRY_PASS_THROUGH_TYPE_MIRROR:
            geomInfo.flags |= GEOM_INST_FLAG_REFLECT;
            break;
        case RG_GEOMETRY_PASS_THROUGH_TYPE_PORTAL:
            geomInfo.flags |= GEOM_INST_FLAG_PORTAL;
            break;
        case RG_GEOMETRY_PASS_THROUGH_TYPE_WATER_ONLY_REFLECT:
            geomInfo.flags |= GEOM_INST_FLAG_MEDIA_TYPE_WATER;
            geomInfo.flags |= GEOM_INST_FLAG_REFLECT;
            break;
        case RG_GEOMETRY_PASS_THROUGH_TYPE_WATER_REFLECT_REFRACT:
            geomInfo.flags |= GEOM_INST_FLAG_MEDIA_TYPE_WATER;
            geomInfo.flags |= GEOM_INST_FLAG_REFLECT;
            geomInfo.flags |= GEOM_INST_FLAG_REFRACT;
            break;
        case RG_GEOMETRY_PASS_THROUGH_TYPE_GLASS_REFLECT_REFRACT:
            geomInfo.flags |= GEOM_INST_FLAG_MEDIA_TYPE_GLASS;
            geomInfo.flags |= GEOM_INST_FLAG_REFLECT;
            geomInfo.flags |= GEOM_INST_FLAG_REFRACT;
            break;
        default:
            break;
    }

    for (int32_t layer = MATERIALS_MAX_LAYER_COUNT - 1; layer >= 0; layer--)
    {
        uint32_t *pMatArr = &geomInfo.materials0A;

        memcpy(&pMatArr[layer * TEXTURES_PER_MATERIAL_COUNT], materials[layer].indices, TEXTURES_PER_MATERIAL_COUNT * sizeof(uint32_t));
        memcpy(geomInfo.materialColors[layer], info.layerColors[layer].data, sizeof(info.layerColors[layer].data));

        // ignore lower level layers, if they won't be visible (i.e. current one is opaque) 
        if (info.layerBlendingTypes[layer] == RG_GEOMETRY_MATERIAL_BLEND_TYPE_OPAQUE &&
            info.geomMaterial.layerMaterials[layer] != RG_NO_MATERIAL)
        {
            break;
        }
    }

    geomInfo.triangleArrayIndex = triangleInfoMgr->UploadAndGetArrayIndex(frameIndex, info.pTriangleSectorIDs, primitiveCount, info.geomType);


    // simple index -- calculated as (global cur static count + global cur dynamic count)
    // global geometry index -- for indexing in geom infos buffer
    // local geometry index -- index of geometry in BLAS
    uint32_t simpleIndex = geomInfoMgr->WriteGeomInfo(frameIndex, info.uniqueID, localIndex, geomFlags, geomInfo);

    if (collectStatic)
    {
        // add material dependency but only for static geometry,
        // dynamic is updated each frame, so their materials will be updated anyway
        for (int32_t layer = MATERIALS_MAX_LAYER_COUNT - 1; layer >= 0; layer--)
        {
            const uint32_t materialIndex = info.geomMaterial.layerMaterials[layer];

            for (uint32_t t = 0; t < TEXTURES_PER_MATERIAL_COUNT; t++)
            {
                uint32_t *pMatArr = &geomInfo.materials0A;

                // if at least one texture is not empty on this layer, add dependency 
                if (pMatArr[layer * TEXTURES_PER_MATERIAL_COUNT + t] != EMPTY_TEXTURE_INDEX)
                {
                    AddMaterialDependency(simpleIndex, layer, materialIndex);

                    break;
                }               
            }
        }

        // also, save transform index for updating static movable's transforms
        simpleIndexToTransformIndex[simpleIndex] = transformIndex;
    }

    return simpleIndex;
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

    // positions
    void *positionsDst = mappedVertexData + offsetPositions + vertIndex * positionStride;
    assert(offsetPositions + (vertIndex + info.vertexCount) * positionStride < wholeBufferSize);

    memcpy(positionsDst, info.pVertexData, info.vertexCount * positionStride);

    // normals
    void *normalsDst = mappedVertexData + offsetNormals + vertIndex * normalStride;
    assert(offsetNormals + (vertIndex + info.vertexCount) * normalStride < wholeBufferSize);

    if (info.pNormalData != nullptr)
    {
        memcpy(normalsDst, info.pNormalData, info.vertexCount * normalStride);
    }

    //const bool useIndices = info.indexCount != 0 && info.indexData != nullptr;
    //const uint32_t triangleCount = useIndices ? info.indexCount / 3 : info.vertexCount / 3;

    CopyTexCoordsToStaging(isStatic, vertIndex, info.vertexCount, info.pTexCoordLayerData);
}

void RTGL1::VertexCollector::CopyTexCoordsToStaging(bool isStatic, uint32_t globalVertIndex, uint32_t vertexCount, const void *const texCoordLayerData[3], bool addToCopy)
{
    assert(mappedVertexData != nullptr);

    const uint64_t texCoordStride = properties.texCoordStride;
    const uint64_t wholeBufferSize = isStatic ?
        sizeof(ShVertexBufferStatic) :
        sizeof(ShVertexBufferDynamic);

    const uint64_t texCoordDataSize = vertexCount * texCoordStride;


    // additional tex coords for static geometry
    const uint64_t *offsetTexCoords = isStatic ? OFFSET_TEX_COORDS_STATIC : OFFSET_TEX_COORDS_DYNAMIC;
    uint32_t        offsetCount     = isStatic ? TEXCOORD_LAYER_COUNT_STATIC : TEXCOORD_LAYER_COUNT_DYNAMIC;


    for (uint32_t i = 0; i < offsetCount; i++)
    {
        if (texCoordLayerData[i] != nullptr)
        {
            uint64_t dstOffsetBegin = offsetTexCoords[i] + globalVertIndex * texCoordStride;
            uint64_t dstOffsetEnd = dstOffsetBegin + texCoordDataSize;

            void *texCoordDst = mappedVertexData + dstOffsetBegin;
            assert(dstOffsetEnd < wholeBufferSize);

            memcpy(texCoordDst, texCoordLayerData[i], texCoordDataSize);


            if (addToCopy)
            {
                texCoordsToCopyLowerBound = std::min(dstOffsetBegin, texCoordsToCopyLowerBound);
                texCoordsToCopyUpperBound = std::max(dstOffsetEnd, texCoordsToCopyUpperBound);

                VkBufferCopy cp = {};
                cp.srcOffset = dstOffsetBegin;
                cp.dstOffset = dstOffsetBegin;
                cp.size = texCoordDataSize;

                texCoordsToCopy.push_back(cp);
            }
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
    curTransformCount = 0;

    simpleIndexToTransformIndex.clear();

    materialDependencies.clear();

    for (auto &f : filters)
    {
        f.second->Reset();
    }
}

std::vector<VkBufferCopy> VertexCollector::CopyVertexDataFromStaging(VkCommandBuffer cmd, bool isStatic)
{
    std::vector<VkBufferCopy> vertCopyInfos;

    if (!GetVertBufferCopyInfos(isStatic, vertCopyInfos))
    {
        return vertCopyInfos;
    }

    vkCmdCopyBuffer(
        cmd,
        stagingVertBuffer.GetBuffer(), vertBuffer->GetBuffer(),
        vertCopyInfos.size(), vertCopyInfos.data());

    return vertCopyInfos;
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

bool VertexCollector::CopyTransformsFromStaging(VkCommandBuffer cmd, bool insertMemBarrier)
{
    if (curTransformCount == 0)
    {
        return false;
    }

    VkBufferCopy info = {};
    info.srcOffset = 0;
    info.dstOffset = 0;
    info.size = curTransformCount * sizeof(VkTransformMatrixKHR);

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
        trnBr.size = curTransformCount * sizeof(VkTransformMatrixKHR);

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

bool VertexCollector::RecopyTransformsFromStaging(VkCommandBuffer cmd)
{
    return CopyTransformsFromStaging(cmd, true);
}

bool RTGL1::VertexCollector::RecopyTexCoordsFromStaging(VkCommandBuffer cmd)
{
    if (curTransformCount == 0 || texCoordsToCopy.empty())
    {
        return false;
    }

    vkCmdCopyBuffer(
        cmd,
        stagingVertBuffer.GetBuffer(), vertBuffer->GetBuffer(),
        texCoordsToCopy.size(), texCoordsToCopy.data());

    VkBufferMemoryBarrier txcBr = {};
    txcBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    txcBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    txcBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    txcBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    txcBr.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    txcBr.buffer = vertBuffer->GetBuffer();
    txcBr.offset = texCoordsToCopyLowerBound;
    txcBr.size = texCoordsToCopyUpperBound - texCoordsToCopyLowerBound;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        0, nullptr,
        1, &txcBr,
        0, nullptr);

    texCoordsToCopy.clear();
    texCoordsToCopyLowerBound = UINT64_MAX;
    texCoordsToCopyUpperBound = 0;

    return true;
}

bool VertexCollector::CopyFromStaging(VkCommandBuffer cmd, bool isStaticVertexData)
{
    const auto vrtCopied = CopyVertexDataFromStaging(cmd, isStaticVertexData);
    bool indCopied = CopyIndexDataFromStaging(cmd);
    bool trnCopied = CopyTransformsFromStaging(cmd, false);

    VkBufferMemoryBarrier barriers[9];
    uint32_t barrierCount = 0;

    assert(vrtCopied.size() + 1 < sizeof(barriers) / sizeof(barriers[0]));

    // prepare for preprocessing
    for (const auto &cp : vrtCopied)
    {
        VkBufferMemoryBarrier &vrtBr = barriers[barrierCount];
        barrierCount++;

        vrtBr = {};
        vrtBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vrtBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vrtBr.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vrtBr.buffer = vertBuffer->GetBuffer();
        vrtBr.offset = cp.dstOffset;
        vrtBr.size = cp.size;
    }

    // prepare for preprocessing
    if (indCopied)
    {
        VkBufferMemoryBarrier &indBr = barriers[barrierCount];
        barrierCount++;

        indBr = {};
        indBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        indBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        indBr.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        indBr.buffer = indexBuffer->GetBuffer();
        indBr.size = curIndexCount * sizeof(uint32_t);
    }

    if (barrierCount > 0)
    {
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            0, nullptr,
            barrierCount, barriers,
            0, nullptr);
    }


    if (trnCopied)
    {
        VkBufferMemoryBarrier trnBr = {};
        trnBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        trnBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        trnBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        trnBr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        trnBr.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        trnBr.buffer = transformsBuffer->GetBuffer();
        trnBr.size = curTransformCount * sizeof(VkTransformMatrixKHR);

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            0, nullptr,
            1, &trnBr,
            0, nullptr);
    }


    return !vrtCopied.empty() || indCopied || trnCopied;
}

bool VertexCollector::GetVertBufferCopyInfos(bool isStatic, std::vector<VkBufferCopy> &outInfos) const
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
    
    // positions, normals + texCoords
    uint32_t count = 2 + offsetCount;
    outInfos.reserve(count);

    outInfos.push_back({ offsetPositions,    offsetPositions,    (uint64_t)curVertexCount * properties.positionStride });
    outInfos.push_back({ offsetNormals,      offsetNormals,      (uint64_t)curVertexCount * properties.normalStride   });

    for (uint32_t i = 0; i < offsetCount; i++)
    {
        outInfos.push_back({ offsetTexCoords[i], offsetTexCoords[i], (uint64_t)curVertexCount * properties.texCoordStride });
    }

    return true;
}

void VertexCollector::UpdateTransform(uint32_t simpleIndex, const RgUpdateTransformInfo &updateInfo)
{
    if (simpleIndex >= MAX_BOTTOM_LEVEL_GEOMETRIES_COUNT)
    {
        assert(0);
        return;
    }

    assert(mappedTransformData != nullptr);

    static_assert(sizeof(RgTransform) == sizeof(VkTransformMatrixKHR), "RgTransform and VkTransformMatrixKHR must have the same structure to be used in AS building");
    memcpy(mappedTransformData + simpleIndexToTransformIndex[simpleIndex], &updateInfo.transform, sizeof(VkTransformMatrixKHR));

    geomInfoMgr->WriteStaticGeomInfoTransform(simpleIndex, updateInfo.movableStaticUniqueID, updateInfo.transform);
}

void RTGL1::VertexCollector::UpdateTexCoords(uint32_t simpleIndex, const RgUpdateTexCoordsInfo &texCoordsInfo)
{
    const bool isStatic = true;
    const uint32_t maxVertexCount = isStatic ? MAX_STATIC_VERTEX_COUNT : MAX_DYNAMIC_VERTEX_COUNT;

    // base vertex index is saved in geometry instance info
    uint32_t globalVertIndex = geomInfoMgr->GetStaticGeomBaseVertexIndex(simpleIndex);
    uint32_t dstVertIndex = globalVertIndex + texCoordsInfo.vertexOffset;

    if (dstVertIndex + texCoordsInfo.vertexCount >= maxVertexCount)
    {
        assert(0);
        return;
    }

    CopyTexCoordsToStaging(isStatic, dstVertIndex, texCoordsInfo.vertexCount, texCoordsInfo.pTexCoordLayerData, true);
}

void VertexCollector::AddMaterialDependency(uint32_t simpleIndex, uint32_t layer, uint32_t materialIndex)
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

        it->second.push_back({ simpleIndex, layer });
    }
}
void VertexCollector::OnMaterialChange(uint32_t materialIndex, const MaterialTextures &newInfo)
{
    // for each geom index that has this material, update geometry instance infos
    for (const auto &p : materialDependencies[materialIndex])
    {    
        geomInfoMgr->WriteStaticGeomInfoMaterials(p.simpleIndex, p.layer, newInfo);
    }
}


VkBuffer VertexCollector::GetVertexBuffer() const
{
    return vertBuffer->GetBuffer();
}

VkBuffer VertexCollector::GetIndexBuffer() const
{
    return indexBuffer->GetBuffer();
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

void VertexCollector::InsertVertexPreprocessBeginBarrier(VkCommandBuffer cmd)
{
    // barriers were already inserted in CopyFromStaging()
}

void VertexCollector::InsertVertexPreprocessFinishBarrier(VkCommandBuffer cmd)
{
    assert((curVertexCount > 0 && curIndexCount > 0) ||
           (curVertexCount == 0 && curIndexCount == 0));

    if (curVertexCount == 0 || curIndexCount == 0)
    {
        return;
    }

    std::vector<VkBufferCopy> vertCopyInfos;
    bool isDynamic = filtersFlags & VertexCollectorFilterTypeFlagBits::CF_DYNAMIC;
    GetVertBufferCopyInfos(!isDynamic, vertCopyInfos);


    VkBufferMemoryBarrier barriers[10];
    uint32_t barrierCount = 0;

    for (const auto &cp : vertCopyInfos)
    {
        VkBufferMemoryBarrier &vrtBr = barriers[barrierCount];
        barrierCount++;

        vrtBr = {};
        vrtBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vrtBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vrtBr.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vrtBr.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
        vrtBr.buffer = GetVertexBuffer();
        vrtBr.offset = cp.dstOffset;
        vrtBr.size = cp.size;
    }

    {
        VkBufferMemoryBarrier &indBr = barriers[barrierCount];
        barrierCount++;

        indBr = {};
        indBr.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        indBr.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        indBr.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        indBr.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;
        indBr.buffer = indexBuffer->GetBuffer();
        indBr.size = curIndexCount * sizeof(uint32_t);
    }

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | 
                                              VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        0, nullptr,
        barrierCount, barriers,
        0, nullptr);
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

uint32_t RTGL1::VertexCollector::GetGeometryCount(VertexCollectorFilterTypeFlags type)
{
    assert(filters.find(type) != filters.end());

    return filters[type]->GetGeometryCount();
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

uint32_t VertexCollector::GetCurrentVertexCount() const
{
    return curVertexCount;
}

uint32_t VertexCollector::GetCurrentIndexCount() const
{
    return curIndexCount;
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
