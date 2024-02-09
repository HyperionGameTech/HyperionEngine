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

#include <cstdint>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::CommandBuffer;
using renderer::Device;
using renderer::GPUBuffer;
using renderer::PackedVertex;
using renderer::PackedIndex;
using renderer::AccelerationStructure;
using renderer::AccelerationGeometry;
using renderer::Topology;
using renderer::IndirectDrawCommand;

class Mesh
    : public BasicObject<STUB_CLASS(Mesh)>
{
public:
    using Index = uint32;

    static Pair<Array<Vertex>, Array<Index>> CalculateIndices(const Array<Vertex> &vertices);

    Mesh();

    Mesh(
        Array<Vertex> vertices,
        Array<Index> indices,
        Topology topology,
        const VertexAttributeSet &vertex_attributes
    );

    Mesh(
        Array<Vertex> vertices,
        Array<Index> indices,
        Topology topology = Topology::TRIANGLES
    );

    Mesh(const Mesh &other) = delete;
    Mesh &operator=(const Mesh &other) = delete;

    Mesh(Mesh &&other) noexcept;
    Mesh &operator=(Mesh &&other) noexcept;

    ~Mesh();

    const GPUBufferRef &GetVertexBuffer() const { return m_vbo; }
    const GPUBufferRef &GetIndexBuffer() const { return m_ibo; }
                                                                   
    const Array<Vertex> &GetVertices() const { return m_vertices; }

    void SetVertices(Array<Vertex> vertices);
    void SetIndices(Array<Index> indices);

    void SetVertices(Array<Vertex> vertices, Array<Index> indices)
        { m_vertices = std::move(vertices); m_indices = std::move(indices); }

    const Array<Index> &GetIndices() const { return m_indices; }

    uint NumIndices() const { return m_indices_count; }
    uint NumVertices() const { return uint(m_vertices.Size()); }

    const VertexAttributeSet &GetVertexAttributes() const { return m_mesh_attributes.vertex_attributes; }
    void SetVertexAttributes(const VertexAttributeSet &attributes) { m_mesh_attributes.vertex_attributes = attributes; }

    const MeshAttributes &GetMeshAttributes() const { return m_mesh_attributes; }

    Topology GetTopology() const { return m_mesh_attributes.topology; }

    Array<PackedVertex> BuildPackedVertices() const;
    Array<PackedIndex> BuildPackedIndices() const;
    
    void CalculateNormals(bool weighted = false);
    void CalculateTangents();
    void InvertNormals();
    
    const BoundingBox &GetAABB() const
        { return m_aabb; }

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

    Array<float> BuildVertexBuffer();

    GPUBufferRef        m_vbo;
    GPUBufferRef        m_ibo;

    uint                m_indices_count = 0;

    MeshAttributes      m_mesh_attributes;

    Array<Vertex>       m_vertices;
    Array<Index>        m_indices;

    mutable BoundingBox m_aabb;
};

} // namespace hyperion::v2

#endif //HYPERION_MESH_H
