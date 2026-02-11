/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>
#include <asset/BlobStorageStructs.hpp>
#include <asset/BlobBuilder.hpp>

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
};

struct MeshData2
{
    static constexpr uint8 Version = 2;
    static constexpr const char Header[] = "MESH";

    uint32 numVertices;
    uint32 numIndices;

    BlobPointer<Vertex> vertexData;
    BlobPointer<ubyte> indexData;

    MeshData2() = default;

    static HYP_NODISCARD MeshData2* Allocate(const MeshData2& other, BlobHeader& outHeader)
    {
        return Allocate(
            Span<const Vertex>(&other.vertexData[0], other.numVertices),
            Span<const ubyte>(&other.indexData[0], other.numIndices),
            outHeader);
    }

    static HYP_NODISCARD MeshData2* Allocate(
        Span<const Vertex> vertices,
        Span<const ubyte> indices,
        BlobHeader& outHeader)
    {
        MeshData2 data {};
        data.numVertices = uint32(vertices.Size());
        data.numIndices = uint32(indices.Size());

        TInlineBlobBuilder<MeshData2, 16> builder(&data);

        return builder
            .Append(offsetof(MeshData2, vertexData), vertices)
            .Append(offsetof(MeshData2, indexData), indices)
            .Build(outHeader);
    }

    HYP_API BoundingBox CalculateAABB() const;
    HYP_API Array<float> BuildVertexBuffer(const VertexAttributeSet& vertexAttributes) const;
    HYP_API Array<PackedVertex> BuildPackedVertices() const;
    HYP_API Array<uint32> BuildPackedIndices() const;
    HYP_API void InvertNormals();
    HYP_API void CalculateNormals(bool weighted = false);
    HYP_API void CalculateTangents();
    HYP_API bool BuildBVH(BVHNode& bvhNode, int maxDepth = 3) const;
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
        ConstructBlobData<MeshData2>(MeshData2 {});
    }

    MeshAsset(Name name, const MeshDesc& desc)
        : AssetObject(name),
          m_meshDesc(desc)
    {
        ConstructBlobData<MeshData2>(MeshData2 {});
    }

    MeshAsset(Name name, const MeshDesc& desc, Span<const Vertex> vertices, Span<const ubyte> indices)
        : AssetObject(name),
          m_meshDesc(desc)
    {
        ConstructBlobData<MeshData2>(vertices, indices);
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

    HYP_FORCE_INLINE MeshData2* GetMeshData() const
    {
        return GetResourceData<MeshData2>();
    }

private:
    HYP_FIELD(Serialize)
    MeshDesc m_meshDesc;
};

} // namespace Hyperion
