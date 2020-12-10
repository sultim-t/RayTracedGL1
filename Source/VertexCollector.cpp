#include "VertexCollector.h"
#include "Generated/ShaderCommonC.h"

#define MAX_INDICES_IN_BUFFER 1 << 21
#define MAX_GEOMETRIES_IN_BUFFER 4096

VertexCollector::VertexCollector(std::shared_ptr<Buffer> vertBuffer, const VBProperties &properties)
{
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
    assert(indices.empty() && transforms.empty() && blasGeometries.empty());

    if (!vertBuffer.expired())
    {
        mappedData = static_cast<uint8_t*>(vertBuffer.lock()->Map());
    }

    // preallocate as changing vector's size will break pointers
    // TODO: chains of vectors with fixed size
    indices.resize(MAX_INDICES_IN_BUFFER);
    transforms.resize(MAX_GEOMETRIES_IN_BUFFER);
}

void VertexCollector::AddGeometry(const RgGeometryCreateInfo &info)
{
    const bool collectStatic = info.geomType == RG_GEOMETRY_TYPE_STATIC || info.geomType == RG_GEOMETRY_TYPE_STATIC_MOVABLE;

    const uint32_t maxVertexCount = collectStatic ?
        MAX_STATIC_VERTEX_COUNT :
        MAX_DYNAMIC_VERTEX_COUNT;

    const uint32_t offsetNormals = collectStatic ?
        offsetof(ShVertexBufferDynamic, normals) :
        offsetof(ShVertexBufferStatic, normals);
    const uint32_t offsetTexCoords = collectStatic ?
        offsetof(ShVertexBufferDynamic, texCoords) :
        offsetof(ShVertexBufferStatic, texCoords);
    const uint32_t offsetColors = collectStatic ?
        offsetof(ShVertexBufferDynamic, colors) :
        offsetof(ShVertexBufferStatic, colors);
    const uint32_t offsetMaterials = collectStatic ?
        offsetof(ShVertexBufferDynamic, materialIds) :
        offsetof(ShVertexBufferStatic, materialIds);

    if (vertBuffer.expired())
    {
        return;
    }

    const uint32_t vertIndex = curVertexCount;
    const uint32_t indIndex = curIndexCount;
    const uint32_t geomIndex = curGeometryCount;

    curVertexCount += info.vertexCount;
    curIndexCount += info.indexCount;
    curGeometryCount++;

    assert(curVertexCount < maxVertexCount);
    assert(curIndexCount < MAX_INDICES_IN_BUFFER);
    assert(curGeometryCount < MAX_GEOMETRIES_IN_BUFFER);

    if (curVertexCount >= maxVertexCount ||
        curIndexCount >= MAX_INDICES_IN_BUFFER ||
        curGeometryCount >= MAX_GEOMETRIES_IN_BUFFER)
    {
        return;
    }

    auto vb = vertBuffer.lock();
    assert(vb->IsMapped());

    // copy data to buffer
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

    // geometry type info
    VkAccelerationStructureCreateGeometryTypeInfoKHR geomType = {};
    geomType.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_GEOMETRY_TYPE_INFO_KHR;
    geomType.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geomType.maxPrimitiveCount = info.indexCount;
    geomType.indexType = VK_INDEX_TYPE_UINT32;
    geomType.maxVertexCount = info.vertexCount;
    geomType.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geomType.allowsTransforms = VK_TRUE;

    PushGeometryType(info.geomType, geomType);

    memcpy(&indices[indIndex], info.indexData, info.indexCount * sizeof(uint32_t));
    memcpy(&transforms[geomIndex], &info.transform, sizeof(VkTransformMatrixKHR));

    // geometry info
    VkAccelerationStructureGeometryKHR geom = {};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;

    VkAccelerationStructureGeometryTrianglesDataKHR &trData = geom.geometry.triangles;
    trData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    trData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    // use positions data in vertex buffer
    trData.vertexData.deviceAddress = vb->GetAddress() + (uint64_t)vertIndex * (uint64_t)properties.positionStride;
    trData.vertexStride = properties.positionStride;
    trData.indexType = VK_INDEX_TYPE_UINT32;
    trData.indexData.hostAddress = &indices[indIndex];
    trData.transformData.hostAddress = &transforms[geomIndex];

    PushGeometry(info.geomType, geom);
}

void VertexCollector::EndCollecting()
{
    if (!vertBuffer.expired())
    {
        vertBuffer.lock()->Unmap();
    }
}

void VertexCollector::Reset()
{
    mappedData = nullptr;

    curVertexCount = 0;
    curIndexCount = 0;
    curGeometryCount = 0;

    indices.clear();
    transforms.clear();
    blasGeometryTypes.clear();
    blasGeometries.clear();
}

void VertexCollector::PushGeometryType(RgGeometryType type,
    const VkAccelerationStructureCreateGeometryTypeInfoKHR& geomType)
{
    blasGeometryTypes.push_back(geomType);
}

void VertexCollector::PushGeometry(RgGeometryType type, const VkAccelerationStructureGeometryKHR& geom)
{
    blasGeometries.push_back(geom);
}

const std::vector<VkAccelerationStructureGeometryKHR> &VertexCollector::GetBlasGeometries() const
{
    return blasGeometries;
}

const std::vector<VkAccelerationStructureCreateGeometryTypeInfoKHR> &VertexCollector::GetBlasGeometriyTypes() const
{
    return blasGeometryTypes;
}
