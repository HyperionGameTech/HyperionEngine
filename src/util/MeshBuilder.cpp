#include "MeshBuilder.hpp"
#include <Engine.hpp>

#include <math/Triangle.hpp>

// temp
#include <util/NoiseFactory.hpp>

namespace hyperion::v2 {

const Array<Vertex> MeshBuilder::quad_vertices = {
    Vertex {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex {{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex {{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex {{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}}
};

const Array<Mesh::Index> MeshBuilder::quad_indices = {
    0, 3, 2,
    0, 2, 1
};

const Array<Vertex> MeshBuilder::cube_vertices = {
    Vertex {{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    
    Vertex {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex {{-1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    Vertex {{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

    Vertex {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    
    Vertex {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    Vertex {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

    Vertex {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex {{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    
    Vertex {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    Vertex {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},

    Vertex {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    
    Vertex {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    Vertex {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},

    Vertex {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex {{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    
    Vertex {{-1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    Vertex {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},

    Vertex {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    
    Vertex {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    Vertex {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}}
};

Handle<Mesh> MeshBuilder::Quad(Topology topology)
{
    Handle<Mesh> mesh;

    const VertexAttributeSet vertex_attributes = renderer::static_mesh_vertex_attributes;

#ifndef HYP_APPLE
    switch (topology) {
    case Topology::TRIANGLE_FAN: {
        auto [new_vertices, new_indices] = Mesh::CalculateIndices(quad_vertices);

        mesh = CreateObject<Mesh>(
            new_vertices,
            new_indices,
            topology,
            vertex_attributes
        );

        break;
    }
    default:
#endif
        mesh = CreateObject<Mesh>(
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

    mesh = CreateObject<Mesh>(
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

    Array<Vertex> vertices;
    Array<Mesh::Index> indices;

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

                vertices.PushBack(Vertex(position, uv));
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
                    indices.PushBack(a);
                    indices.PushBack(c);
                    indices.PushBack(b);
                    indices.PushBack(c);
                    indices.PushBack(d);
                    indices.PushBack(b);
                } else {
                    indices.PushBack(a);
                    indices.PushBack(c);
                    indices.PushBack(d);
                    indices.PushBack(a);
                    indices.PushBack(d);
                    indices.PushBack(b);
                }
            }
        }
    }

    auto mesh = CreateObject<Mesh>(
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

    Array<Vertex> new_vertices(mesh->GetVertices());

    const auto normal_matrix = transform.GetMatrix().Inverted().Transposed();

    for (Vertex &vertex : new_vertices) {
        vertex.SetPosition(transform.GetMatrix() * vertex.GetPosition());
        vertex.SetNormal(normal_matrix * vertex.GetNormal());
        vertex.SetTangent(normal_matrix * vertex.GetTangent());
        vertex.SetBitangent(normal_matrix * vertex.GetBitangent());
    }

    return CreateObject<Mesh>(
        new_vertices,
        mesh->GetIndices(),
        mesh->GetTopology(),
        mesh->GetVertexAttributes()
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

    Array<Vertex> all_vertices;
    all_vertices.Resize(transformed_meshes[0]->GetVertices().Size() + transformed_meshes[1]->GetVertices().Size());
    
    Array<Mesh::Index> all_indices;
    all_indices.Resize(transformed_meshes[0]->GetIndices().Size() + transformed_meshes[1]->GetIndices().Size());

    SizeType vertex_offset = 0,
        index_offset = 0;

    for (SizeType mesh_index = 0; mesh_index < 2; mesh_index++) {
        const SizeType vertex_offset_before = vertex_offset;
        
        for (SizeType i = 0; i < transformed_meshes[mesh_index]->GetVertices().Size(); i++) {
            all_vertices[vertex_offset++] = transformed_meshes[mesh_index]->GetVertices()[i];
        }

        for (SizeType i = 0; i < transformed_meshes[mesh_index]->GetIndices().Size(); i++) {
            all_indices[index_offset++] = transformed_meshes[mesh_index]->GetIndices()[i] + vertex_offset_before;
        }
    }

    return CreateObject<Mesh>(
        all_vertices,
        all_indices,
        a->GetTopology(),
        merged_vertex_attributes
    );
}

Handle<Mesh> MeshBuilder::Merge(const Mesh *a, const Mesh *b)
{
    return Merge(a, b, Transform(), Transform());
}

MeshBuilder::VoxelGrid MeshBuilder::Voxelize(const Mesh *mesh, Float voxel_size)
{
    BoundingBox total_aabb = mesh->GetAABB();
    Vector3 total_aabb_dimensions = total_aabb.GetExtent();

    UInt num_voxels_x = MathUtil::Floor<Float, UInt>(total_aabb_dimensions.x / voxel_size);
    UInt num_voxels_y = MathUtil::Floor<Float, UInt>(total_aabb_dimensions.y / voxel_size);
    UInt num_voxels_z = MathUtil::Floor<Float, UInt>(total_aabb_dimensions.z / voxel_size);

    return Voxelize(mesh, Vec3u(num_voxels_x, num_voxels_y, num_voxels_z));
}

MeshBuilder::VoxelGrid MeshBuilder::Voxelize(const Mesh *mesh, Vec3u voxel_grid_size)
{
    BoundingBox mesh_aabb = mesh->GetAABB();

    Float voxel_size = 1.0f / Float(voxel_grid_size.Max());

 // building out grid
    VoxelGrid grid;
    grid.voxel_size = voxel_size;
    grid.size_x = voxel_grid_size.x;
    grid.size_y = voxel_grid_size.y;
    grid.size_z = voxel_grid_size.z;

    if (!voxel_grid_size.Max()) {
        return grid;
    }

    grid.voxels.Resize(voxel_grid_size.x * voxel_grid_size.y * voxel_grid_size.z);

    BoundingBox total_aabb;

    for (UInt x = 0; x < voxel_grid_size.x; x++) {
        for (UInt y = 0; y < voxel_grid_size.y; y++) {
            for (UInt z = 0; z < voxel_grid_size.z; z++) {
                UInt voxel_index = grid.GetIndex(x, y, z);

                AssertThrow(voxel_index < grid.voxels.Size());

                BoundingBox voxel_aabb(
                    Vector3(x, y, z) * voxel_size,
                    Vector3(x + 1, y + 1, z + 1) * voxel_size
                );

                total_aabb.Extend(voxel_aabb);

                grid.voxels[voxel_index] = Voxel(
                    voxel_aabb,
                    false
                );
            }
        }
    }

    // const Vector3 ratio = (mesh_aabb.GetExtent() / total_aabb.GetExtent()).Normalize();

    UInt num_filled_voxels = 0;

    // filling in data
    const Array<Vertex> &vertices = mesh->GetVertices();
    const Array<Mesh::Index> &indices = mesh->GetIndices();

    AssertThrow(indices.Size() % 3 == 0);

    for (SizeType index = 0; index < indices.Size(); index += 3) {
        const Triangle triangle(
            vertices[indices[index]].GetPosition(),
            vertices[indices[index + 1]].GetPosition(),
            vertices[indices[index + 2]].GetPosition()
        );

        BoundingBox triangle_aabb = triangle.GetBoundingBox();

        const Vector3 min_in_aabb = triangle_aabb.GetMin() / mesh_aabb.GetExtent() + 0.5f;//::Max(triangle_aabb.GetMin(), Vector3::zero);
        const Vector3 max_in_aabb = triangle_aabb.GetMax() / mesh_aabb.GetExtent() + 0.5f;//Vector3::Min(triangle_aabb.GetMax(), Vector3::one);

        AssertThrow(triangle_aabb.GetMin().Min() >= triangle_aabb.GetMin().Min());
        AssertThrow(triangle_aabb.GetMax().Max() <= triangle_aabb.GetMax().Max());

        const UInt min_x = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(min_in_aabb.x * (Float(voxel_grid_size.x) - 1.0f), 0.0f, Float(voxel_grid_size.x) - 1.0f));
        const UInt min_y = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(min_in_aabb.y * (Float(voxel_grid_size.y) - 1.0f), 0.0f, Float(voxel_grid_size.y) - 1.0f));
        const UInt min_z = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(min_in_aabb.z * (Float(voxel_grid_size.z) - 1.0f), 0.0f, Float(voxel_grid_size.z) - 1.0f));

        const UInt max_x = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(max_in_aabb.x * (Float(voxel_grid_size.x) - 1.0f), 0.0f, Float(voxel_grid_size.x) - 1.0f));
        const UInt max_y = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(max_in_aabb.y * (Float(voxel_grid_size.y) - 1.0f), 0.0f, Float(voxel_grid_size.y) - 1.0f));
        const UInt max_z = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(max_in_aabb.z * (Float(voxel_grid_size.z) - 1.0f), 0.0f, Float(voxel_grid_size.z) - 1.0f));

        for (UInt x = min_x; x <= max_x; x++) {
            for (UInt y = min_y; y <= max_y; y++) {
                for (UInt z = min_z; z <= max_z; z++) {
                    const UInt voxel_index = grid.GetIndex(x, y, z);

                    AssertThrow(voxel_index < grid.voxels.Size());

                    const Vector3 v0 = triangle[0].GetPosition() / mesh_aabb.GetExtent() + 0.5f;
                    const Vector3 v1 = triangle[1].GetPosition() / mesh_aabb.GetExtent() + 0.5f;
                    const Vector3 v2 = triangle[2].GetPosition() / mesh_aabb.GetExtent() + 0.5f;

                    const Vector3 v0_scaled = v0 * voxel_size;
                    const Vector3 v1_scaled = v1 * voxel_size;
                    const Vector3 v2_scaled = v2 * voxel_size;

                    const Triangle triangle_scaled(v0_scaled, v1_scaled, v2_scaled);

                    grid.voxels[voxel_index].filled |= triangle_scaled.ContainsPoint(grid.voxels[voxel_index].aabb.GetCenter());
                    ++num_filled_voxels;
                }
            }
        }
    }

    // for (const auto &vertex : mesh->GetVertices()) {
    //     // x,y,z from 0..1 
    //     const Vector3 vertex_position_in_aabb = (vertex.GetPosition() / mesh_aabb.GetExtent()) + 0.5f;

    //     const UInt x = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(vertex_position_in_aabb.x * (Float(voxel_grid_size.x) - 1.0f), 0.0f, Float(voxel_grid_size.x) - 1.0f));
    //     const UInt y = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(vertex_position_in_aabb.y * (Float(voxel_grid_size.y) - 1.0f), 0.0f, Float(voxel_grid_size.y) - 1.0f));
    //     const UInt z = MathUtil::Floor<Float, UInt>(MathUtil::Clamp(vertex_position_in_aabb.z * (Float(voxel_grid_size.z) - 1.0f), 0.0f, Float(voxel_grid_size.z) - 1.0f));

    //     const UInt voxel_index = grid.GetIndex(x, y, z);

    //     grid.voxels[voxel_index].filled = true;
    //     ++num_filled_voxels;
    // }

    return grid;
}

Handle<Mesh> MeshBuilder::BuildVoxelMesh(VoxelGrid voxel_grid)
{
    Handle<Mesh> mesh;

    for (UInt x = 0; x < voxel_grid.size_x; x++) {
        for (UInt y = 0; y < voxel_grid.size_y; y++) {
            for (UInt z = 0; z < voxel_grid.size_z; z++) {
                UInt index = voxel_grid.GetIndex(x, y, z);

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

    if (!mesh) {
        mesh = Cube();
    }

    return mesh;
}

} // namespace hyperion::v2
