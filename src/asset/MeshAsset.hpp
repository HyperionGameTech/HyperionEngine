/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <asset/AssetObject.hpp>

#include <core/reflection/HypObjectFwd.hpp>

#include <core/math/Vertex.hpp>
#include <core/math/BoundingBox.hpp>

#include <core/containers/Array.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/Shared.hpp>

#include <core/HashCode.hpp>
#include <core/Types.hpp>

namespace hyperion {

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

} // namespace hyperion
