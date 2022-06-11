#include "mesh_builder.h"

namespace hyperion::v2 {

std::unique_ptr<Mesh> MeshBuilder::Quad(Topology topology)
{
    static const std::vector<Vertex> vertices{
        Vertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        Vertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
    };

    static const std::vector<Mesh::Index> indices{
        0, 3, 2,
        0, 2, 1
    };

    std::unique_ptr<Mesh> mesh;

    const VertexAttributeSet vertex_attributes = renderer::static_mesh_vertex_attributes;

#ifndef HYP_APPLE
    switch (topology) {
    case Topology::TRIANGLE_FAN: {
        auto [new_vertices, new_indices] = Mesh::CalculateIndices(vertices);

        mesh = std::make_unique<Mesh>(
            new_vertices,
            new_indices,
            vertex_attributes
        );

        break;
    }
    default:
#endif
        mesh = std::make_unique<Mesh>(
            vertices,
            indices, 
            vertex_attributes
        );

#ifndef HYP_APPLE
        break;
    }
#endif

    mesh->CalculateTangents();

    return std::move(mesh);
}

} // namespace hyperion::v2
