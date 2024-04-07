#ifndef HYPERION_V2_MESH_H
#define HYPERION_V2_MESH_H

#include <core/Base.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/DynArray.hpp>

#include <math/BoundingBox.hpp>
#include <math/Vertex.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/Shader.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <cstdint>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::Device;
using renderer::GPUBuffer;
using renderer::PackedVertex;
using renderer::PackedIndex;
using renderer::AccelerationStructure;
using renderer::AccelerationGeometry;
using renderer::Topology;
using renderer::IndirectDrawCommand;

struct RENDER_COMMAND(SetStreamedMeshData);

class Mesh
    : public BasicObject<STUB_CLASS(Mesh)>
{
public:
    friend struct RENDER_COMMAND(SetStreamedMeshData);

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

    Mesh(const Mesh &other) = delete;
    Mesh &operator=(const Mesh &other) = delete;

    Mesh(Mesh &&other) noexcept;
    Mesh &operator=(Mesh &&other) noexcept;

    ~Mesh();

    const GPUBufferRef &GetVertexBuffer() const
        { return m_vbo; }

    const GPUBufferRef &GetIndexBuffer() const
        { return m_ibo; }

    void SetVertices(Array<Vertex> vertices);
    void SetVertices(Array<Vertex> vertices, Array<Index> indices);

    uint NumIndices() const
        { return m_indices_count; }

    const RC<StreamedMeshData> &GetStreamedMeshData() const
        { return m_streamed_mesh_data; }

    /*! \brief Set the mesh data for the Mesh. Only usable on the Render thread. If needed
        from another thread, use the static version of this function. */
    void SetStreamedMeshData(RC<StreamedMeshData> streamed_mesh_data);
    static void SetStreamedMeshData(Handle<Mesh> mesh, RC<StreamedMeshData> streamed_mesh_data);

    const VertexAttributeSet &GetVertexAttributes() const
        { return m_mesh_attributes.vertex_attributes; }

    void SetVertexAttributes(const VertexAttributeSet &attributes)
        { m_mesh_attributes.vertex_attributes = attributes; }

    const MeshAttributes &GetMeshAttributes() const { return m_mesh_attributes; }

    Topology GetTopology() const { return m_mesh_attributes.topology; }

    Array<PackedVertex> BuildPackedVertices() const;
    Array<PackedIndex> BuildPackedIndices() const;

    void CalculateNormals(bool weighted = false);
    void CalculateTangents();
    void InvertNormals();

    /*! \brief Get the axis-aligned bounding box for the mesh.
        If the mesh has not been initialized, the AABB will be invalid, unless SetAABB() has been called.
        Otherwise, the AABB will be calculated from the mesh vertices. */
    const BoundingBox &GetAABB() const
        { return m_aabb; }

    /*! \brief Manually set the AABB for the mesh. If CalculateAABB is called after this, or the mesh data is changed, the
        manually set AABB will be overwritten. */
    void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; }

    void Init();

    void Render(CommandBuffer *cmd, uint num_instances = 1) const;

    void RenderIndirect(
        CommandBuffer *cmd,
        const GPUBuffer *indirect_buffer,
        uint32 buffer_offset = 0
    ) const;

    void PopulateIndirectDrawCommand(IndirectDrawCommand &out);

private:
    void CalculateAABB();

    static Array<float> BuildVertexBuffer(const VertexAttributeSet &vertex_attributes, const MeshData &mesh_data);

    GPUBufferRef            m_vbo;
    GPUBufferRef            m_ibo;

    uint                    m_indices_count = 0;

    MeshAttributes          m_mesh_attributes;

    RC<StreamedMeshData>    m_streamed_mesh_data;

    mutable BoundingBox     m_aabb;
};

} // namespace hyperion::v2

#endif //HYPERION_MESH_H
