/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MESH_HPP
#define HYPERION_MESH_HPP

#include <core/object/HypObject.hpp>

#include <core/utilities/Pair.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <math/BoundingBox.hpp>
#include <math/Vertex.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderResources.hpp>
#include <rendering/Shader.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <cstdint>

namespace hyperion {

using renderer::PackedVertex;
using renderer::Topology;
using renderer::IndirectDrawCommand;

struct RENDER_COMMAND(SetStreamedMeshData);
struct RENDER_COMMAND(UploadMeshData);

class MeshRenderResources final : public RenderResourcesBase
{
public:
    MeshRenderResources(Mesh *mesh);
    MeshRenderResources(MeshRenderResources &&other) noexcept;
    virtual ~MeshRenderResources() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GPUBufferRef &GetVertexBuffer() const
        { return m_vbo; }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GPUBufferRef &GetIndexBuffer() const
        { return m_ibo; }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE uint32 NumIndices() const
        { return m_num_indices; }

    void SetVertexAttributes(const VertexAttributeSet &vertex_attributes);
    void SetStreamedMeshData(const RC<StreamedMeshData> &streamed_mesh_data);

protected:
    virtual void Initialize() override;
    virtual void Destroy() override;
    virtual void Update() override;

    virtual Name GetTypeName() const override
        { return NAME("MeshRenderResources"); }

private:
    static Array<float> BuildVertexBuffer(const VertexAttributeSet &vertex_attributes, const MeshData &mesh_data);

    void UploadMeshData();

    Mesh                    *m_mesh;

    VertexAttributeSet      m_vertex_attributes;
    RC<StreamedMeshData>    m_streamed_mesh_data;

    GPUBufferRef            m_vbo;
    GPUBufferRef            m_ibo;

    uint32                  m_num_indices;
};

HYP_CLASS()
class HYP_API Mesh final : public HypObject<Mesh>
{
    HYP_OBJECT_BODY(Mesh);

    HYP_PROPERTY(ID, &Mesh::GetID, { { "serialize", false } });
    
public:
    friend struct RENDER_COMMAND(SetStreamedMeshData);
    friend struct RENDER_COMMAND(UploadMeshData);

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

    HYP_FORCE_INLINE MeshRenderResources &GetRenderResources() const
        { return *m_render_resources; }

    void SetVertices(Array<Vertex> vertices);
    void SetVertices(Array<Vertex> vertices, Array<Index> indices);

    HYP_METHOD(Property="StreamedMeshData")
    const RC<StreamedMeshData> &GetStreamedMeshData() const;

    HYP_METHOD(Property="StreamedMeshData")
    void SetStreamedMeshData(RC<StreamedMeshData> streamed_mesh_data);

    void SetStreamedMeshData_ThreadSafe(RC<StreamedMeshData> streamed_mesh_data);

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

    Array<PackedVertex> BuildPackedVertices() const;
    Array<uint32> BuildPackedIndices() const;

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

    void Render(CommandBuffer *cmd, uint32 num_instances = 1, uint32 instance_index = 0) const;

    void RenderIndirect(
        CommandBuffer *cmd,
        const GPUBuffer *indirect_buffer,
        uint32 buffer_offset = 0
    ) const;

    void PopulateIndirectDrawCommand(IndirectDrawCommand &out);

    /*! \brief Set the mesh to be able to have Render* methods called without needing to have its resources claimed.
     *  \note Init() must be called before this method. */
    void SetPersistentRenderResourcesEnabled(bool enabled)
    {
        AssertIsInitCalled();

        HYP_MT_CHECK_RW(m_data_race_detector, "m_always_claimed_render_resources_handle");

        if (enabled) {
            m_always_claimed_render_resources_handle = RenderResourcesHandle(*m_render_resources);
        } else {
            m_always_claimed_render_resources_handle.Reset();
        }
    }

private:
    void CalculateAABB();

    Name                    m_name;

    MeshAttributes          m_mesh_attributes;

    RC<StreamedMeshData>    m_streamed_mesh_data;

    mutable BoundingBox     m_aabb;

    DataRaceDetector        m_data_race_detector;

    MeshRenderResources     *m_render_resources;
    RenderResourcesHandle   m_always_claimed_render_resources_handle;
};

} // namespace hyperion

#endif //HYPERION_MESH_HPP
