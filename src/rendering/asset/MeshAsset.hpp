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
        data.numIndices = uint32(indices.Size()) / sizeof(uint32); // @TODO Fix for non-uint32 indices

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
    }

    MeshAsset(Name name, const MeshDesc& desc)
        : AssetObject(name),
          m_meshDesc(desc)
    {
    }

    MeshAsset(Name name, const MeshDesc& desc, Span<const Vertex> vertices, Span<const ubyte> indices)
        : AssetObject(name),
          m_meshDesc(desc)
    {
        AllocateBlobData(vertexData, vertices);
        AllocateBlobData(indexData, indices);
    }

    MeshAsset(const MeshAsset& other) = delete;
    MeshAsset& operator=(const MeshAsset& other) = delete;

    MeshAsset(MeshAsset&& other) noexcept = delete;
    MeshAsset& operator=(MeshAsset&& other) noexcept = delete;

    ~MeshAsset()
    {
        FreeBlobData(vertexData);
        FreeBlobData(indexData);
    }

    HYP_FORCE_INLINE const MeshDesc& GetMeshDesc() const
    {
        return m_meshDesc;
    }

    HYP_FORCE_INLINE Span<const Vertex> GetVertexData() const
    {
        return vertexData.raw != nullptr
            ? Span<const Vertex>(reinterpret_cast<const Vertex*>(vertexData.raw), vertexData.size / sizeof(Vertex))
            : Span<const Vertex>();
    }

    HYP_FORCE_INLINE Span<const ubyte> GetIndexData() const
    {
        return indexData.raw != nullptr
            ? Span<const ubyte>(reinterpret_cast<const ubyte*>(indexData.raw), indexData.size)
            : Span<const ubyte>();
    }

protected:
    void WriteBlobData(BlobStorage& blobStorage) override
    {
        Assert(vertexData.raw != nullptr);
        Assert(indexData.raw != nullptr);

        if (!vertexData.readOnly)
        {
            BlobHeader vertexDataHeader {};
            Memory::Copy(vertexDataHeader.magic, "VB", 4);
            vertexDataHeader.version = 1;
            vertexDataHeader.payloadOffset = 0;
            vertexDataHeader.payloadSize = vertexData.size;

            BlobResourceKey key {};

            if (blobStorage.AllocateBlob(vertexDataHeader, key))
            {
                vertexData.bufferOffset = key.offset;
            }
            else
            {
                return;
            }
        }
        
        if (!indexData.readOnly)
        {
            BlobHeader indexDataHeader {};
            Memory::Copy(indexDataHeader.magic, "IB", 4);
            indexDataHeader.version = 1;
            indexDataHeader.payloadOffset = 0;
            indexDataHeader.payloadSize = indexData.size;

            BlobResourceKey key {};

            if (blobStorage.AllocateBlob(indexDataHeader, key))
            {
                indexData.bufferOffset = key.offset;
            }
            else
            {
                return;
            }
        }

        ByteWriter* writeStream = blobStorage.GetWriteStream();

        writeStream->Write(vertexData.raw, vertexData.size);
        writeStream->Write(indexData.raw, indexData.size);
    }

    void ReadBlobData(BlobStorage& blobStorage) override
    {
        HYP_NOT_IMPLEMENTED();
    }

    HYP_FIELD(Serialize)
    MeshDesc m_meshDesc;

    HYP_FIELD(Serialize)
    BlobDataReference vertexData;
    
    HYP_FIELD(Serialize)
    BlobDataReference indexData;
};

} // namespace Hyperion
