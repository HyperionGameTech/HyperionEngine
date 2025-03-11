/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MESH_HPP
#define HYPERION_MESH_HPP

#include <core/object/HypObject.hpp>

#include <core/utilities/Pair.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vertex.hpp>

#include <rendering/RenderableAttributes.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <cstdint>

namespace hyperion {

class BVHNode;
class MeshRenderResource;

HYP_CLASS()
class HYP_API Mesh final : public HypObject<Mesh>
{
    HYP_OBJECT_BODY(Mesh);

public:
    using Index = uint32;

    static Pair<Array<Vertex>, Array<uint32>> CalculateIndices(const Array<Vertex> &vertices);

    Mesh();

    Mesh(
        RC<StreamedMeshData> streamed_mesh_data,
        Topology topology,
        const VertexAttributeSet &vertex_attributes
    );

    Mesh(
        RC<StreamedMeshData> streamed_mesh_data,
        Topology topology = Topology::TRIANGLES
    );

    Mesh(
        Array<Vertex> vertices,
        Array<uint32> indices,
        Topology topology,
        const VertexAttributeSet &vertex_attributes
    );

    Mesh(
        Array<Vertex> vertices,
        Array<uint32> indices,
        Topology topology = Topology::TRIANGLES
    );

    Mesh(const Mesh &other)             = delete;
    Mesh &operator=(const Mesh &other)  = delete;

    Mesh(Mesh &&other) noexcept;
    Mesh &operator=(Mesh &&other) noexcept;

    ~Mesh();

    HYP_METHOD(Property="Name", Serialize=true, Editor=true)
    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    HYP_METHOD(Property="Name", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetName(Name name)
        { m_name = name; }

    HYP_FORCE_INLINE MeshRenderResource &GetRenderResource() const
        { return *m_render_resource; }

    void SetVertices(Span<const Vertex> vertices);
    void SetVertices(Span<const Vertex> vertices, Span<const uint32> indices);

    HYP_METHOD(Property="StreamedMeshData")
    const RC<StreamedMeshData> &GetStreamedMeshData() const;

    HYP_METHOD(Property="StreamedMeshData")
    void SetStreamedMeshData(RC<StreamedMeshData> streamed_mesh_data);

    HYP_METHOD()
    uint32 NumIndices() const;

    HYP_METHOD(Property="VertexAttributes")
    HYP_FORCE_INLINE const VertexAttributeSet &GetVertexAttributes() const
        { return m_mesh_attributes.vertex_attributes; }

    HYP_METHOD(Property="VertexAttributes")
    void SetVertexAttributes(const VertexAttributeSet &attributes);

    HYP_FORCE_INLINE const MeshAttributes &GetMeshAttributes() const
        { return m_mesh_attributes; }

    void SetMeshAttributes(const MeshAttributes &attributes);

    HYP_METHOD(Property="Topology")
    HYP_FORCE_INLINE Topology GetTopology() const
        { return m_mesh_attributes.topology; }

    void CalculateNormals(bool weighted = false);
    void CalculateTangents();

    HYP_METHOD()
    void InvertNormals();

    /*! \brief Get the axis-aligned bounding box for the mesh.
        If the mesh has not been initialized, the AABB will be invalid, unless SetAABB() has been called.
        Otherwise, the AABB will be calculated from the mesh vertices. */
    HYP_METHOD(Property="AABB", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const BoundingBox &GetAABB() const
        { return m_aabb; }

    /*! \brief Manually set the AABB for the mesh. If CalculateAABB is called after this, or the mesh data is changed, the
        manually set AABB will be overwritten. */
    HYP_METHOD(Property="AABB", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; }

    void Init();

    /*! \brief Set the mesh to be able to have Render* methods called without needing to have its resources claimed.
     *  \note Init() must be called before this method. */
    void SetPersistentRenderResourceEnabled(bool enabled);

    bool BuildBVH(BVHNode &out_bvh_node, int max_depth = 3);

private:
    void CalculateAABB();

    Name                    m_name;

    MeshAttributes          m_mesh_attributes;

    RC<StreamedMeshData>    m_streamed_mesh_data;

    mutable BoundingBox     m_aabb;

    MeshRenderResource      *m_render_resource;
    ResourceHandle          m_always_claimed_render_resource_handle;
    
    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

} // namespace hyperion

#endif //HYPERION_MESH_HPP
