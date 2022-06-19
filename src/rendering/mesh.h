#ifndef HYPERION_V2_MESH_H
#define HYPERION_V2_MESH_H

#include "base.h"
#include "shader.h"

#include <math/bounding_box.h>

#include <rendering/backend/renderer_buffer.h>
#include <rendering/backend/renderer_command_buffer.h>
#include <rendering/backend/rt/renderer_acceleration_structure.h>

#include <math/vertex.h>

#include <cstdint>
#include <vector>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::CommandBuffer;
using renderer::Device;
using renderer::VertexBuffer;
using renderer::IndexBuffer;
using renderer::PackedVertex;
using renderer::PackedIndex;
using renderer::AccelerationStructure;
using renderer::AccelerationGeometry;

class Mesh : public EngineComponentBase<STUB_CLASS(Mesh)> {
public:
    using Index = UInt32;

    enum Flags {
        MESH_FLAGS_NONE = 0,
        MESH_FLAGS_HAS_ACCELERATION_GEOMETRY = 1
    };

    static std::pair<std::vector<Vertex>, std::vector<Index>> CalculateIndices(const std::vector<Vertex> &vertices);

    Mesh(
        const std::vector<Vertex> &vertices,
        const std::vector<Index> &indices,
        const VertexAttributeSet &vertex_attributes,
        Flags flags = MESH_FLAGS_NONE
    );

    Mesh(
        const std::vector<Vertex> &vertices,
        const std::vector<Index> &indices,
        Flags flags = MESH_FLAGS_NONE
    );

    Mesh(const Mesh &other) = delete;
    Mesh &operator=(const Mesh &other) = delete;
    ~Mesh();

    inline VertexBuffer *GetVertexBuffer() const                 { return m_vbo.get(); }
    inline IndexBuffer *GetIndexBuffer() const                   { return m_ibo.get(); }

    inline const std::vector<Vertex> &GetVertices() const        { return m_vertices; }
    inline const std::vector<Index> &GetIndices() const          { return m_indices; }

    const VertexAttributeSet &GetVertexAttributes() const        { return m_vertex_attributes; }

    Flags GetFlags() const                                       { return m_flags; }
    inline void SetFlags(Flags flags)                            { m_flags = flags; }

    std::vector<PackedVertex> BuildPackedVertices() const;
    std::vector<PackedIndex> BuildPackedIndices() const;
    
    void CalculateNormals();
    void CalculateTangents();
    void InvertNormals();

    BoundingBox CalculateAabb() const;

    void Init(Engine *engine);
    void Render(Engine *engine, CommandBuffer *cmd) const;

private:
    std::vector<float> BuildVertexBuffer();

    std::unique_ptr<VertexBuffer>         m_vbo;
    std::unique_ptr<IndexBuffer>          m_ibo;

    VertexAttributeSet m_vertex_attributes;

    std::vector<Vertex> m_vertices;
    std::vector<Index>  m_indices;
    
    Flags m_flags;
};

}

#endif //HYPERION_MESH_H
