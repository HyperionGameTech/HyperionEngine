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

Handle<Mesh> MeshBuilder::NormalizedCubeSphere(uint num_divisions)
{
    const float step = 1.0f / static_cast<float>(num_divisions);

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

    for (uint face = 0; face < 6; face++) {
        const Vector3 &origin = origins[face];
        const Vector3 &right = rights[face];
        const Vector3 &up = ups[face];

        for (uint j = 0; j < num_divisions + 1; j++) {
            for (uint i = 0; i < num_divisions + 1; i++) {
                const Vector3 point = (origin + Vector3(step) * (Vector3(i) * right + Vector3(j) * up)).Normalized();
                Vector3 position = point;
                Vector3 normal = point;

                const Vector2 uv(
                    static_cast<float>(j + (face * num_divisions)) / static_cast<float>(num_divisions * 6),
                    static_cast<float>(i + (face * num_divisions)) / static_cast<float>(num_divisions * 6)
                );

                vertices.PushBack(Vertex(position, uv));
            }
        }
    }

    const uint k = num_divisions + 1;

    for (uint face = 0; face < 6; face++) {
        for (uint j = 0; j < num_divisions; j++) {
            const bool is_bottom = j < (num_divisions / 2);

            for (uint i = 0; i < num_divisions; i++) {
                const bool is_left = i < (num_divisions / 2);

                const uint a = (face * k + j) * k + i;
                const uint b = (face * k + j) * k + i + 1;
                const uint c = (face * k + j + 1) * k + i;
                const uint d = (face * k + j + 1) * k + i + 1;

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

    const RC<StreamedMeshData> &streamed_mesh_data = mesh->GetStreamedMeshData();

    if (!streamed_mesh_data) {
        return Handle<Mesh> { };
    }

    auto mesh_data_ref = streamed_mesh_data->AcquireRef();

    Array<Vertex> new_vertices(mesh_data_ref->GetMeshData().vertices);

    const auto normal_matrix = transform.GetMatrix().Inverted().Transposed();

    for (Vertex &vertex : new_vertices) {
        vertex.SetPosition(transform.GetMatrix() * vertex.GetPosition());
        vertex.SetNormal(normal_matrix * vertex.GetNormal());
        vertex.SetTangent(normal_matrix * vertex.GetTangent());
        vertex.SetBitangent(normal_matrix * vertex.GetBitangent());
    }

    RC<StreamedMeshData> new_streamed_mesh_data = StreamedMeshData::FromMeshData(MeshData {
        std::move(new_vertices),
        mesh_data_ref->GetMeshData().indices
    });

    return CreateObject<Mesh>(
        std::move(new_streamed_mesh_data),
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

    RC<StreamedMeshData> streamed_mesh_datas[] = {
        transformed_meshes[0]->GetStreamedMeshData(),
        transformed_meshes[1]->GetStreamedMeshData()
    };

    AssertThrow(streamed_mesh_datas[0] != nullptr);
    AssertThrow(streamed_mesh_datas[1] != nullptr);

    StreamedDataRef<StreamedMeshData> streamed_mesh_data_refs[] = {
        streamed_mesh_datas[0]->AcquireRef(),
        streamed_mesh_datas[1]->AcquireRef()
    };
    
    const auto merged_vertex_attributes = a->GetVertexAttributes() | b->GetVertexAttributes();

    Array<Vertex> all_vertices;
    all_vertices.Resize(streamed_mesh_data_refs[0]->GetMeshData().vertices.Size() + streamed_mesh_data_refs[1]->GetMeshData().vertices.Size());
    
    Array<Mesh::Index> all_indices;
    all_indices.Resize(streamed_mesh_data_refs[0]->GetMeshData().indices.Size() + streamed_mesh_data_refs[1]->GetMeshData().indices.Size());

    SizeType vertex_offset = 0,
        index_offset = 0;

    for (SizeType mesh_index = 0; mesh_index < 2; mesh_index++) {
        const SizeType vertex_offset_before = vertex_offset;
        
        for (SizeType i = 0; i < streamed_mesh_data_refs[mesh_index]->GetMeshData().vertices.Size(); i++) {
            all_vertices[vertex_offset++] = streamed_mesh_data_refs[mesh_index]->GetMeshData().vertices[i];
        }

        for (SizeType i = 0; i < streamed_mesh_data_refs[mesh_index]->GetMeshData().indices.Size(); i++) {
            all_indices[index_offset++] = streamed_mesh_data_refs[mesh_index]->GetMeshData().indices[i] + vertex_offset_before;
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

} // namespace hyperion::v2
