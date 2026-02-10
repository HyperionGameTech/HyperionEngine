/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>
#include <asset/BlobStorageStructs.hpp>

#include <core/reflection/ObjectFwd.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/containers/Array.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/Shared.hpp>
#include <rendering/Vertex.hpp>

namespace Hyperion {

class BVHNode;

HYP_STRUCT()
struct MeshDesc
{
    HYP_STRUCT_BODY(MeshDesc);

    HYP_FIELD(Serialize)
    MeshAttributes meshAttributes;

    HYP_FIELD(Serialize)
    uint32 numVertices = 0;

    HYP_FIELD(Serialize)
    uint32 numIndices = 0;
};

HYP_STRUCT()
struct MeshData
{
    HYP_STRUCT_BODY(MeshData);

    HYP_FIELD(Serialize, Compressed)
    Array<Vertex> vertexData;

    HYP_FIELD(Serialize, Compressed)
    ByteBuffer indexData;

    HYP_API BoundingBox CalculateAABB() const;
    HYP_API Array<float> BuildVertexBuffer(const VertexAttributeSet& vertexAttributes) const;
    HYP_API Array<PackedVertex> BuildPackedVertices() const;
    HYP_API Array<uint32> BuildPackedIndices() const;
    HYP_API void InvertNormals();
    HYP_API void CalculateNormals(bool weighted = false);
    HYP_API void CalculateTangents();
    HYP_API bool BuildBVH(BVHNode& bvhNode, int maxDepth = 3) const;
};

struct MeshData2
{
    BlobPointer<Vertex> vertexData;
    BlobPointer<uint32> indexData;

    MeshData2() = default;

    static MeshData2* CreateMeshData(Span<const Vertex> vertices, Span<const uint32> indices)
    {
        SizeType totalSize = ByteUtil::AlignAs(sizeof(MeshData2), alignof(Vertex))
            + (sizeof(Vertex) * vertices.Size())
            + (sizeof(uint32) * indices.Size());

        MeshData2* meshData = (MeshData2*)HYP_ALLOC_ALIGNED(totalSize, 16);
        if (!meshData)
            return nullptr;

        uint8* dataBasePtr = (uint8*)(meshData + 1);
        Vertex* verticesPtr = HYP_ALIGN_PTR_AS(dataBasePtr, Vertex);

        dataBasePtr += sizeof(Vertex) * vertices.Size();
        uint32* indicesPtr = HYP_ALIGN_PTR_AS(dataBasePtr, uint32);

        Memory::Copy(verticesPtr, vertices.Data(), sizeof(Vertex) * vertices.Size());
        Memory::Copy(indicesPtr, indices.Data(), sizeof(uint32) * indices.Size());

        meshData->vertexData = BlobPointer<Vertex>(reinterpret_cast<UIntPtr>(verticesPtr) - reinterpret_cast<UIntPtr>(meshData));
        meshData->indexData = BlobPointer<uint32>(reinterpret_cast<UIntPtr>(indicesPtr) - reinterpret_cast<UIntPtr>(meshData));

        return meshData;
    }
};

HYP_CLASS()
class MeshAsset : public AssetObject
{
    HYP_OBJECT_BODY(MeshAsset);

public:
    MeshAsset()
        : AssetObject(),
          m_meshDesc()
    {
        AssetObject::SetData(MeshData());
    }

    MeshAsset(Name name, const MeshDesc& desc)
        : AssetObject(name),
          m_meshDesc(desc)
    {
        AssetObject::SetData(MeshData());
    }

    MeshAsset(Name name, const MeshDesc& desc, const MeshData& meshData)
        : AssetObject(name, meshData),
          m_meshDesc(desc)
    {
    }

    MeshAsset(Name name, const MeshDesc& desc, MeshData&& meshData)
        : AssetObject(name, std::move(meshData)),
          m_meshDesc(desc)
    {
    }

    MeshAsset(const MeshAsset& other) = delete;
    MeshAsset& operator=(const MeshAsset& other) = delete;

    MeshAsset(MeshAsset&& other) noexcept = delete;
    MeshAsset& operator=(MeshAsset&& other) noexcept = delete;

    ~MeshAsset() = default;

    HYP_FORCE_INLINE const MeshDesc& GetMeshDesc() const
    {
        return m_meshDesc;
    }

    HYP_FORCE_INLINE MeshData* GetMeshData() const
    {
        return GetResourceData<MeshData>();
    }

private:
    HYP_FIELD(Serialize)
    MeshDesc m_meshDesc;
};

} // namespace Hyperion
