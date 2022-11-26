#include "MeshBuilder.hpp"
#include <Engine.hpp>

// temp
#include <util/NoiseFactory.hpp>

namespace hyperion::v2 {

const std::vector<Vertex> MeshBuilder::quad_vertices = {
    Vertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
};

const std::vector<Mesh::Index> MeshBuilder::quad_indices = {
    0, 3, 2,
    0, 2, 1
};

const std::vector<Vertex> MeshBuilder::cube_vertices = {
    Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    
    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex{{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    
    Vertex{{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

    Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    
    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},

    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex{{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    
    Vertex{{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},

    Vertex{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    
    Vertex{{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},

    Vertex{{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex{{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    
    Vertex{{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex{{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex{{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}}
};

Handle<Mesh> MeshBuilder::Quad(Topology topology)
{
    Handle<Mesh> mesh;

    const VertexAttributeSet vertex_attributes = renderer::static_mesh_vertex_attributes;

#ifndef HYP_APPLE
    switch (topology) {
    case Topology::TRIANGLE_FAN: {
        auto [new_vertices, new_indices] = Mesh::CalculateIndices(quad_vertices);

        mesh = Engine::Get()->CreateObject<Mesh>(
            new_vertices,
            new_indices,
            topology,
            vertex_attributes
        );

        break;
    }
    default:
#endif
        mesh = Engine::Get()->CreateObject<Mesh>(
            quad_vertices,
            quad_indices,
            topology,
            vertex_attributes
        );

#ifndef HYP_APPLE
        break;
    }
#endif

    mesh->CalculateTangents();

    return mesh;
}

Handle<Mesh> MeshBuilder::Cube()
{
    Handle<Mesh> mesh;

    const VertexAttributeSet vertex_attributes = renderer::static_mesh_vertex_attributes;

    auto mesh_data = Mesh::CalculateIndices(cube_vertices);

    mesh = Engine::Get()->CreateObject<Mesh>(
        mesh_data.first,
        mesh_data.second,
        Topology::TRIANGLES,
        vertex_attributes
    );

    mesh->CalculateTangents();

    return mesh;
}

Handle<Mesh> MeshBuilder::NormalizedCubeSphere(UInt num_divisions)
{
    const Float step = 1.0f / static_cast<Float>(num_divisions);

    static const Vector3 origins[6] = {
        Vector3(-1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, -1.0f),
        Vector3(1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, -1.0f, 1.0f),
        Vector3(-1.0f, 1.0f, -1.0f),
        Vector3(-1.0f, -1.0f, 1.0f)
    };

    static const Vector3 rights[6] = {
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(-2.0f, 0.0f, 0.0f),
        Vector3(0.0f, 0.0f, -2.0f),
        Vector3(2.0f, 0.0f, 0.0f),
        Vector3(2.0f, 0.0f, 0.0f)
    };

    static const Vector3 ups[6] = {
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 2.0f, 0.0f),
        Vector3(0.0f, 0.0f, 2.0f),
        Vector3(0.0f, 0.0f, -2.0f)
    };

    std::vector<Vertex> vertices;
    std::vector<Mesh::Index> indices;

    for (UInt face = 0; face < 6; face++) {
        const Vector3 &origin = origins[face];
        const Vector3 &right = rights[face];
        const Vector3 &up = ups[face];

        for (UInt j = 0; j < num_divisions + 1; j++) {
            for (UInt i = 0; i < num_divisions + 1; i++) {
                const Vector3 point = (origin + Vector3(step) * (Vector3(i) * right + Vector3(j) * up)).Normalized();
                Vector3 position = point;
                Vector3 normal = point;

                const Vector2 uv(
                    static_cast<Float>(j + (face * num_divisions)) / static_cast<Float>(num_divisions * 6),
                    static_cast<Float>(i + (face * num_divisions)) / static_cast<Float>(num_divisions * 6)
                );

                vertices.push_back(Vertex(position, uv));
            }
        }
    }

    const UInt k = num_divisions + 1;

    for (UInt face = 0; face < 6; face++) {
        for (UInt j = 0; j < num_divisions; j++) {
            const bool is_bottom = j < (num_divisions / 2);

            for (UInt i = 0; i < num_divisions; i++) {
                const bool is_left = i < (num_divisions / 2);

                const UInt a = (face * k + j) * k + i;
                const UInt b = (face * k + j) * k + i + 1;
                const UInt c = (face * k + j + 1) * k + i;
                const UInt d = (face * k + j + 1) * k + i + 1;

                if (is_bottom ^ is_left) {
                    indices.push_back(a);
                    indices.push_back(c);
                    indices.push_back(b);
                    indices.push_back(c);
                    indices.push_back(d);
                    indices.push_back(b);
                } else {
                    indices.push_back(a);
                    indices.push_back(c);
                    indices.push_back(d);
                    indices.push_back(a);
                    indices.push_back(d);
                    indices.push_back(b);
                }
            }
        }
    }

    auto mesh = Engine::Get()->CreateObject<Mesh>(
        vertices,
        indices,
        Topology::TRIANGLES,
        renderer::static_mesh_vertex_attributes
    );

    mesh->CalculateNormals(true);
    mesh->CalculateTangents();

    return mesh;
}

Handle<Mesh> MeshBuilder::ApplyTransform(const Mesh *mesh, const Transform &transform)
{
    AssertThrow(mesh != nullptr);

    std::vector<Vertex> new_vertices(mesh->GetVertices());

    const auto normal_matrix = transform.GetMatrix().Inverted().Transposed();

    for (Vertex &vertex : new_vertices) {
        vertex.SetPosition(transform.GetMatrix() * vertex.GetPosition());
        vertex.SetNormal(normal_matrix * vertex.GetNormal());
        vertex.SetTangent(normal_matrix * vertex.GetTangent());
        vertex.SetBitangent(normal_matrix * vertex.GetBitangent());
    }

    return Engine::Get()->CreateObject<Mesh>(
        new_vertices,
        mesh->GetIndices(),
        mesh->GetTopology(),
        mesh->GetVertexAttributes(),
        mesh->GetFlags()
    );
}

Handle<Mesh> MeshBuilder::Merge(const Mesh *a, const Mesh *b, const Transform &a_transform, const Transform &b_transform)
{
    AssertThrow(a != nullptr);
    AssertThrow(b != nullptr);

    Handle<Mesh> transformed_meshes[] = {
        ApplyTransform(a, a_transform),
        ApplyTransform(b, b_transform)
    };
    
    const auto merged_vertex_attributes = a->GetVertexAttributes() | b->GetVertexAttributes();

    std::vector<Vertex> all_vertices(transformed_meshes[0]->GetVertices().size() + transformed_meshes[1]->GetVertices().size());
    std::vector<Mesh::Index> all_indices(transformed_meshes[0]->GetIndices().size() + transformed_meshes[1]->GetIndices().size());

    SizeType vertex_offset = 0,
        index_offset = 0;

    for (SizeType mesh_index = 0; mesh_index < 2; mesh_index++) {
        const SizeType vertex_offset_before = vertex_offset;
        
        for (SizeType i = 0; i < transformed_meshes[mesh_index]->GetVertices().size(); i++) {
            all_vertices[vertex_offset++] = transformed_meshes[mesh_index]->GetVertices()[i];
        }

        for (SizeType i = 0; i < transformed_meshes[mesh_index]->GetIndices().size(); i++) {
            all_indices[index_offset++] = transformed_meshes[mesh_index]->GetIndices()[i] + vertex_offset_before;
        }
    }

    return Engine::Get()->CreateObject<Mesh>(
        all_vertices,
        all_indices,
        a->GetTopology(),
        merged_vertex_attributes,
        a->GetFlags()
    );
}

Handle<Mesh> MeshBuilder::Merge(const Mesh *a, const Mesh *b)
{
    return Merge(a, b, Transform(), Transform());
}

MeshBuilder::VoxelGrid MeshBuilder::Voxelize(const Mesh *mesh, float voxel_size)
{
    BoundingBox total_aabb = mesh->CalculateAABB();
    Vector3 total_aabb_dimensions = total_aabb.GetExtent();

    UInt num_voxels_x = MathUtil::Ceil<Float, UInt>(total_aabb_dimensions.x / voxel_size);
    UInt num_voxels_y = MathUtil::Ceil<Float, UInt>(total_aabb_dimensions.y / voxel_size);
    UInt num_voxels_z = MathUtil::Ceil<Float, UInt>(total_aabb_dimensions.z / voxel_size);

    // building out grid
    VoxelGrid grid;
    grid.voxel_size = voxel_size;
    grid.size_x = num_voxels_x;
    grid.size_y = num_voxels_y;
    grid.size_z = num_voxels_z;

    if (!num_voxels_x || !num_voxels_y || !num_voxels_z) {
        return grid;
    }

    grid.voxels.Resize(num_voxels_x * num_voxels_y * num_voxels_z);

    for (UInt x = 0; x < num_voxels_x; x++) {
        for (UInt y = 0; y < num_voxels_y; y++) {
            for (UInt z = 0; z < num_voxels_z; z++) {
                UInt voxel_index = (z * num_voxels_x * num_voxels_y) + (y * num_voxels_y) + x;
                grid.voxels[voxel_index] = Voxel(
                    BoundingBox(
                        Vector3(x, y, z) * voxel_size,
                        Vector3(x + 1, y + 1, z + 1) * voxel_size
                    ),
                    false
                );
            }
        }
    }

    UInt num_filled_voxels = 0;

    // filling in data
    for (const auto &vertex : mesh->GetVertices()) {
        Vector3 vertex_over_dimensions = (vertex.GetPosition() - total_aabb.GetMin()) / Vector3::Max(total_aabb.GetExtent(), 0.0001f);

        UInt x = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(vertex_over_dimensions.x * (static_cast<Float>(num_voxels_x) - 1.0f), 0.0f, static_cast<Float>(num_voxels_x) - 1.0f));
        UInt y = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(vertex_over_dimensions.y * (static_cast<Float>(num_voxels_y) - 1.0f), 0.0f, static_cast<Float>(num_voxels_y) - 1.0f));
        UInt z = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(vertex_over_dimensions.z * (static_cast<Float>(num_voxels_z) - 1.0f), 0.0f, static_cast<Float>(num_voxels_z) - 1.0f));

        UInt voxel_index = (z * num_voxels_x * num_voxels_y) + (y * num_voxels_y) + x;

        grid.voxels[voxel_index].filled = true;
        ++num_filled_voxels;
    }

    return grid;
}

Handle<Mesh> MeshBuilder::BuildVoxelMesh(VoxelGrid &&voxel_grid)
{
    Handle<Mesh> mesh;

    for (UInt x = 0; x < voxel_grid.size_x; x++) {
        for (UInt y = 0; y < voxel_grid.size_y; y++) {
            for (UInt z = 0; z < voxel_grid.size_z; z++) {
                UInt index = (z * voxel_grid.size_x * voxel_grid.size_y) + (y * voxel_grid.size_y) + x;

                if (!voxel_grid.voxels[index].filled) {
                    continue;
                }

                auto cube = Cube();

                if (!mesh) {
                    // std::cout << "new voxel " << "\n";
                    mesh = ApplyTransform(
                        cube.Get(),
                        Transform(Vector3(x, y, z) * voxel_grid.voxel_size, voxel_grid.voxel_size, Quaternion::Identity())
                    );
                } else {
                    mesh = Merge(
                        mesh.Get(),
                        cube.Get(),
                        Transform(),
                        Transform(Vector3(x, y, z) * voxel_grid.voxel_size, voxel_grid.voxel_size, Quaternion::Identity())
                    );
                }
            }
        }
    }

    return mesh;
}

} // namespace hyperion::v2
