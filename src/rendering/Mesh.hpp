/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/HypObject.hpp>

#include <core/utilities/Pair.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vertex.hpp>

#include <scene/BVH.hpp>

#include <rendering/RenderableAttributes.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>

#include <asset/MeshAsset.hpp>

#include <cstdint>

namespace hyperion {

class BVHNode;
class RenderMesh;
class Material;

/*! \brief Represents a 3D mesh in the engine, containing vertex data, indices, and rendering attributes. */
HYP_CLASS()
class HYP_API Mesh final : public HypObjectBase
{
    HYP_OBJECT_BODY(Mesh);

public:
    using Index = uint32;

    static Pair<Array<Vertex>, Array<uint32>> CalculateIndices(const Array<Vertex>& vertices);

    Mesh();
    Mesh(const Handle<MeshAsset>& asset, Topology topology, const VertexAttributeSet& vertexAttributes);
    Mesh(const Handle<MeshAsset>& asset, Topology topology = TOP_TRIANGLES);
    Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology, const VertexAttributeSet& vertexAttributes);
    Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology = TOP_TRIANGLES);

    Mesh(const Mesh& other) = delete;
    Mesh& operator=(const Mesh& other) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    ~Mesh();

    HYP_METHOD()
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_METHOD()
    void SetName(Name name);

    void SetMeshData(const MeshData& meshData);

    HYP_METHOD()
    uint32 NumIndices() const;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GpuBufferRef& GetVertexBuffer() const
    {
        return m_vertexBuffer;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GpuBufferRef& GetIndexBuffer() const
    {
        return m_indexBuffer;
    }

    HYP_METHOD(Property = "VertexAttributes")
    HYP_FORCE_INLINE VertexAttributeSet GetVertexAttributes() const
    {
        return m_asset.IsValid() ? m_asset->GetMeshDesc().meshAttributes.vertexAttributes : VertexAttributeSet();
    }

    HYP_FORCE_INLINE MeshAttributes GetMeshAttributes() const
    {
        return m_asset.IsValid() ? m_asset->GetMeshDesc().meshAttributes : MeshAttributes();
    }

    HYP_METHOD(Property = "Topology")
    HYP_FORCE_INLINE Topology GetTopology() const
    {
        return GetMeshAttributes().topology;
    }

    HYP_FORCE_INLINE const Handle<MeshAsset>& GetAsset() const
    {
        return m_asset;
    }

    /*! \brief Get the axis-aligned bounding box for the mesh. */
    HYP_METHOD(Property = "AABB", Serialize = true, Editor = true)
    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    /*! \brief Manually set the AABB for the mesh */
    HYP_METHOD(Property = "AABB", Serialize = true, Editor = true)
    HYP_FORCE_INLINE void SetAABB(const BoundingBox& aabb)
    {
        m_aabb = aabb;
    }

    HYP_FORCE_INLINE const BVHNode& GetBVH() const
    {
        return m_bvh;
    }

    bool BuildBVH(int maxDepth = 3);

private:
    void Init() override;
    void CreateGpuBuffers();

    HYP_FIELD(Serialize, Editor)
    Name m_name;

    mutable BoundingBox m_aabb;

    HYP_FIELD(Serialize)
    BVHNode m_bvh;

    GpuBufferRef m_vertexBuffer;
    GpuBufferRef m_indexBuffer;

    Handle<MeshAsset> m_asset;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace hyperion

