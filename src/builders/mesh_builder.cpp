#include "mesh_builder.h"

namespace hyperion::v2 {

#if !HYP_APPLE
std::unique_ptr<Mesh> MeshBuilder::Quad(Topology topology)
#else
std::unique_ptr<Mesh> MeshBuilder::Quad()
#endif
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

#if !HYP_APPLE
    switch (topology) {
    case Topology::TRIANGLE_FAN: {
        auto [new_vertices, new_indices] = Mesh::CalculateIndices(vertices);

        mesh = std::make_unique<Mesh>(
            new_vertices,
            new_indices,
            VertexAttributeSet::static_mesh
        );

        break;
    }
    default:
#endif
        mesh = std::make_unique<Mesh>(
            vertices,
            indices, 
            VertexAttributeSet::static_mesh
        );

#if !HYP_APPLE
        break;
    }
#endif

    mesh->CalculateTangents();

    return std::move(mesh);
}

} // namespace hyperion::v2
