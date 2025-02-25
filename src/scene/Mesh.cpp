/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Mesh.hpp>

#include <rendering/RenderMesh.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <scene/BVH.hpp>

#include <Engine.hpp>

#include <unordered_map>
#include <cstring>

namespace hyperion {

using renderer::GPUBufferType;

#pragma region Mesh

Pair<Array<Vertex>, Array<uint32>> Mesh::CalculateIndices(const Array<Vertex> &vertices)
{
    std::unordered_map<Vertex, uint32> index_map;

    Array<uint32> indices;
    indices.Reserve(vertices.Size());

    /* This will be our resulting buffer with only the vertices we need. */
    Array<Vertex> new_vertices;
    new_vertices.Reserve(vertices.Size());

    for (const auto &vertex : vertices) {
        /* Check if the vertex already exists in our map */
        auto it = index_map.find(vertex);

        /* If it does, push to our indices */
        if (it != index_map.end()) {
            indices.PushBack(it->second);

            continue;
        }

        const uint32 mesh_index = uint32(new_vertices.Size());

        /* The vertex is unique, so we push it. */
        new_vertices.PushBack(vertex);
        indices.PushBack(mesh_index);

        index_map[vertex] = mesh_index;
    }

    return { std::move(new_vertices), std::move(indices) };
}

Mesh::Mesh()
    : HypObject(),
      m_mesh_attributes {
          .vertex_attributes    = static_mesh_vertex_attributes,
          .topology             = Topology::TRIANGLES
      },
      m_aabb(BoundingBox::Empty()),
      m_render_resource(nullptr)
{
}

Mesh::Mesh(
    RC<StreamedMeshData> streamed_mesh_data,
    Topology topology,
    const VertexAttributeSet &vertex_attributes
) : HypObject(),
    m_mesh_attributes {
        .vertex_attributes  = vertex_attributes,
        .topology           = topology
    },
    m_streamed_mesh_data(std::move(streamed_mesh_data)),
    m_aabb(BoundingBox::Empty()),
    m_render_resource(nullptr)
{
    CalculateAABB();
}

Mesh::Mesh(
    RC<StreamedMeshData> streamed_mesh_data,
    Topology topology
) : Mesh(
        std::move(streamed_mesh_data),
        topology,
        static_mesh_vertex_attributes | skeleton_vertex_attributes
    )
{
}

Mesh::Mesh(
    Array<Vertex> vertices,
    Array<uint32> indices,
    Topology topology
) : Mesh(
        std::move(vertices),
        std::move(indices),
        topology,
        static_mesh_vertex_attributes | skeleton_vertex_attributes
    )
{
}

Mesh::Mesh(
    Array<Vertex> vertices,
    Array<uint32> indices,
    Topology topology,
    const VertexAttributeSet &vertex_attributes
) : HypObject(),
    m_mesh_attributes {
        .vertex_attributes = vertex_attributes,
        .topology = topology
    },
    m_streamed_mesh_data(MakeRefCountedPtr<StreamedMeshData>(MeshData {
        std::move(vertices),
        std::move(indices)
    })),
    m_aabb(BoundingBox::Empty()),
    m_render_resource(nullptr)
{
    auto ref = m_streamed_mesh_data->AcquireRef();
    const MeshData &mesh_data = ref->GetMeshData();

    CalculateAABB();
}

Mesh::Mesh(Mesh &&other) noexcept
    : HypObject(static_cast<HypObject &&>(other)),
      m_mesh_attributes(other.m_mesh_attributes),
      m_streamed_mesh_data(std::move(other.m_streamed_mesh_data)),
      m_aabb(other.m_aabb),
      m_render_resource(other.m_render_resource)
{
    other.m_aabb = BoundingBox::Empty();
    other.m_render_resource = nullptr;
}

Mesh &Mesh::operator=(Mesh &&other) noexcept
{
    if (&other == this) {
        return *this;
    }

    if (m_render_resource != nullptr) {
        FreeResource(m_render_resource);
    }

    m_mesh_attributes = other.m_mesh_attributes;
    m_streamed_mesh_data = std::move(other.m_streamed_mesh_data);
    m_aabb = other.m_aabb;
    m_render_resource = other.m_render_resource;

    other.m_aabb = BoundingBox::Empty();
    other.m_render_resource = nullptr;

    return *this;
}

Mesh::~Mesh()
{
    if (IsInitCalled()) {
        SetReady(false);

        m_always_claimed_render_resources_handle.Reset();

        if (m_render_resource != nullptr) {
            FreeResource(m_render_resource);
        }
    }
}

void Mesh::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        if (m_render_resource != nullptr) {
            FreeResource(m_render_resource);

            m_render_resource = nullptr;
        }
    }));

    AssertThrowMsg(GetVertexAttributes() != 0, "No vertex attributes set on mesh");

    m_render_resource = AllocateResource<MeshRenderResource>(this);

    {
        HYP_MT_CHECK_RW(m_data_race_detector, "Streamed mesh data");

        if (!m_streamed_mesh_data) {
            m_streamed_mesh_data.Emplace();
        }

        m_render_resource->SetVertexAttributes(GetVertexAttributes());
        m_render_resource->SetStreamedMeshData(m_streamed_mesh_data);
    }

    SetReady(true);
}

void Mesh::SetVertices(Span<const Vertex> vertices)
{
    Array<uint32> indices;
    indices.Resize(vertices.Size());

    for (SizeType index = 0; index < vertices.Size(); index++) {
        indices[index] = uint32(index);
    }

    SetStreamedMeshData(StreamedMeshData::FromMeshData({
        Array<Vertex>(vertices),
        std::move(indices)
    }));
}

void Mesh::SetVertices(Span<const Vertex> vertices, Span<const uint32> indices)
{
    SetStreamedMeshData(StreamedMeshData::FromMeshData({
        Array<Vertex>(vertices),
        Array<uint32>(indices)
    }));
}

const RC<StreamedMeshData> &Mesh::GetStreamedMeshData() const
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_data_race_detector, "Streamed mesh data");

    return m_streamed_mesh_data;
}

void Mesh::SetStreamedMeshData(RC<StreamedMeshData> streamed_mesh_data)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_data_race_detector, "Streamed mesh data");

    m_streamed_mesh_data = std::move(streamed_mesh_data);

    CalculateAABB();

    if (IsInitCalled()) {
        if (!m_streamed_mesh_data) {
            // Create empty buffers
            m_streamed_mesh_data = StreamedMeshData::FromMeshData({
                Array<Vertex> { },
                Array<uint32> { }
            });
        }

        m_render_resource->SetStreamedMeshData(m_streamed_mesh_data);
    }
}

uint32 Mesh::NumIndices() const
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_data_race_detector, "Streamed mesh data");

    return m_streamed_mesh_data != nullptr
        ? m_streamed_mesh_data->NumIndices()
        : 0;
}

void Mesh::SetVertexAttributes(const VertexAttributeSet &vertex_attributes)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_data_race_detector, "Attributes");

    m_mesh_attributes.vertex_attributes = vertex_attributes;

    if (IsInitCalled()) {
        m_render_resource->SetVertexAttributes(vertex_attributes);
    }
}

void Mesh::SetMeshAttributes(const MeshAttributes &attributes)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_data_race_detector, "Attributes");

    m_mesh_attributes = attributes;

    if (IsInitCalled()) {
        m_render_resource->SetVertexAttributes(attributes.vertex_attributes);
    }
}

void Mesh::SetPersistentRenderResourceEnabled(bool enabled)
{
    AssertIsInitCalled();

    HYP_MT_CHECK_RW(m_data_race_detector, "m_always_claimed_render_resources_handle");

    if (enabled) {
        m_always_claimed_render_resources_handle = ResourceHandle(*m_render_resource);
    } else {
        m_always_claimed_render_resources_handle.Reset();
    }
}

void Mesh::CalculateNormals(bool weighted)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_data_race_detector, "Streamed mesh data");

    if (!m_streamed_mesh_data) {
        HYP_LOG(Mesh, Warning, "Cannot calculate normals before mesh data is set!");

        return;
    }

    auto ref = m_streamed_mesh_data->AcquireRef();
    MeshData mesh_data = ref->GetMeshData();

    if (mesh_data.indices.Empty()) {
        HYP_LOG(Mesh, Warning, "Cannot calculate normals before indices are generated!");

        return;
    }

    std::unordered_map<uint32, Array<Vec3f>> normals;

    // compute per-face normals (facet normals)
    for (SizeType i = 0; i < mesh_data.indices.Size(); i += 3) {
        const uint32 i0 = mesh_data.indices[i];
        const uint32 i1 = mesh_data.indices[i + 1];
        const uint32 i2 = mesh_data.indices[i + 2];

        const Vec3f &p0 = mesh_data.vertices[i0].GetPosition();
        const Vec3f &p1 = mesh_data.vertices[i1].GetPosition();
        const Vec3f &p2 = mesh_data.vertices[i2].GetPosition();

        const Vec3f u = p2 - p0;
        const Vec3f v = p1 - p0;
        const Vec3f n = v.Cross(u).Normalize();

        normals[i0].PushBack(n);
        normals[i1].PushBack(n);
        normals[i2].PushBack(n);
    }

    for (SizeType i = 0; i < mesh_data.vertices.Size(); i++) {
        if (weighted) {
            mesh_data.vertices[i].SetNormal(normals[i].Sum());
        } else {
            mesh_data.vertices[i].SetNormal(normals[i].Sum().Normalize());
        }
    }

    if (!weighted) {
        m_streamed_mesh_data.Emplace(std::move(mesh_data));

        return;
    }

    normals.clear();

    // weighted (smooth) normals

    for (SizeType i = 0; i < mesh_data.indices.Size(); i += 3) {
        const uint32 i0 = mesh_data.indices[i];
        const uint32 i1 = mesh_data.indices[i + 1];
        const uint32 i2 = mesh_data.indices[i + 2];

        const Vec3f &p0 = mesh_data.vertices[i0].GetPosition();
        const Vec3f &p1 = mesh_data.vertices[i1].GetPosition();
        const Vec3f &p2 = mesh_data.vertices[i2].GetPosition();

        const Vec3f &n0 = mesh_data.vertices[i0].GetNormal();
        const Vec3f &n1 = mesh_data.vertices[i1].GetNormal();
        const Vec3f &n2 = mesh_data.vertices[i2].GetNormal();

        // Vector3 n = FixedArray { n0, n1, n2 }.Avg();

        FixedArray<Vec3f, 3> weighted_normals { n0, n1, n2 };

        // nested loop through faces to get weighted neighbours
        // any code that uses this really should bake the normals in
        // especially for any production code. this is an expensive process
        for (SizeType j = 0; j < mesh_data.indices.Size(); j += 3) {
            if (j == i) {
                continue;
            }

            const uint32 j0 = mesh_data.indices[j];
            const uint32 j1 = mesh_data.indices[j + 1];
            const uint32 j2 = mesh_data.indices[j + 2];

            const FixedArray<Vec3f, 3> face_positions {
                mesh_data.vertices[j0].GetPosition(),
                mesh_data.vertices[j1].GetPosition(),
                mesh_data.vertices[j2].GetPosition()
            };

            const FixedArray<Vec3f, 3> face_normals {
                mesh_data.vertices[j0].GetNormal(),
                mesh_data.vertices[j1].GetNormal(),
                mesh_data.vertices[j2].GetNormal()
            };

            const Vec3f a = p1 - p0;
            const Vec3f b = p2 - p0;
            const Vec3f c = a.Cross(b);

            const float area = 0.5f * MathUtil::Sqrt(c.Dot(c));

            if (face_positions.Contains(p0)) {
                const float angle = (p0 - p1).AngleBetween(p0 - p2);
                weighted_normals[0] += face_normals.Avg() * area * angle;
            }

            if (face_positions.Contains(p1)) {
                const float angle = (p1 - p0).AngleBetween(p1 - p2);
                weighted_normals[1] += face_normals.Avg() * area * angle;
            }

            if (face_positions.Contains(p2)) {
                const float angle = (p2 - p0).AngleBetween(p2 - p1);
                weighted_normals[2] += face_normals.Avg() * area * angle;
            }

            // if (face_positions.Contains(p0)) {
            //     weighted_normals[0] += face_normals.Avg();
            // }

            // if (face_positions.Contains(p1)) {
            //     weighted_normals[1] += face_normals.Avg();
            // }

            // if (face_positions.Contains(p2)) {
            //     weighted_normals[2] += face_normals.Avg();
            // }
        }

        normals[i0].PushBack(weighted_normals[0].Normalized());
        normals[i1].PushBack(weighted_normals[1].Normalized());
        normals[i2].PushBack(weighted_normals[2].Normalized());
    }

    for (SizeType i = 0; i < mesh_data.vertices.Size(); i++) {
        mesh_data.vertices[i].SetNormal(normals[i].Sum().Normalized());
    }

    normals.clear();

    m_streamed_mesh_data.Emplace(std::move(mesh_data));
}

void Mesh::CalculateTangents()
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_data_race_detector, "Streamed mesh data");

    if (!m_streamed_mesh_data) {
        HYP_LOG(Mesh, Warning, "Cannot calculate normals before mesh data is set!");

        return;
    }

    auto ref = m_streamed_mesh_data->AcquireRef();
    MeshData mesh_data = ref->GetMeshData();

    struct TangentBitangentPair
    {
        Vec3f tangent;
        Vec3f bitangent;
    };

    std::unordered_map<uint32, Array<TangentBitangentPair>> data;

    for (SizeType i = 0; i < mesh_data.indices.Size();) {
        const SizeType count = MathUtil::Min(3, mesh_data.indices.Size() - i);

        Vertex v[3];
        Vec2f uv[3];

        for (uint32 j = 0; j < count; j++) {
            v[j] = mesh_data.vertices[mesh_data.indices[i + j]];
            uv[j] = v[j].GetTexCoord0();
        }

        uint32 i0 = mesh_data.indices[i];
        uint32 i1 = mesh_data.indices[i + 1];
        uint32 i2 = mesh_data.indices[i + 2];

        const Vec3f edge1 = v[1].GetPosition() - v[0].GetPosition();
        const Vec3f edge2 = v[2].GetPosition() - v[0].GetPosition();
        const Vec2f edge1uv = uv[1] - uv[0];
        const Vec2f edge2uv = uv[2] - uv[0];

        const float cp = edge1uv.x * edge2uv.y - edge1uv.y * edge2uv.x;

        if (cp != 0.0f) {
            const float mul = 1.0f / cp;

            const TangentBitangentPair tangent_bitangent {
                .tangent = ((edge1 * edge2uv.y - edge2 * edge1uv.y) * mul).Normalize(),
                .bitangent = ((edge1 * edge2uv.x - edge2 * edge1uv.x) * mul).Normalize()
            };

            data[i0].PushBack(tangent_bitangent);
            data[i1].PushBack(tangent_bitangent);
            data[i2].PushBack(tangent_bitangent);
        }

        i += count;
    }

    for (SizeType i = 0; i < mesh_data.vertices.Size(); i++) {
        const auto &tangent_bitangents = data[i];

        // find average
        Vec3f average_tangent, average_bitangent;

        for (const auto &item : tangent_bitangents) {
            average_tangent += item.tangent * (1.0f / tangent_bitangents.Size());
            average_bitangent += item.bitangent * (1.0f / tangent_bitangents.Size());
        }

        average_tangent.Normalize();
        average_bitangent.Normalize();

        mesh_data.vertices[i].SetTangent(average_tangent);
        mesh_data.vertices[i].SetBitangent(average_bitangent);
    }

    m_mesh_attributes.vertex_attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT;
    m_mesh_attributes.vertex_attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT;

    m_streamed_mesh_data.Emplace(std::move(mesh_data));
}

void Mesh::InvertNormals()
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_data_race_detector, "Streamed mesh data");

    if (!m_streamed_mesh_data) {
        HYP_LOG(Mesh, Warning, "Cannot invert normals before mesh data is set!");

        return;
    }

    auto ref = m_streamed_mesh_data->AcquireRef();
    MeshData mesh_data = ref->GetMeshData();

    for (Vertex &vertex : mesh_data.vertices) {
        vertex.SetNormal(vertex.GetNormal() * -1.0f);
    }

    m_streamed_mesh_data.Emplace(std::move(mesh_data));
}

void Mesh::CalculateAABB()
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_data_race_detector, "Streamed mesh data");

    if (!m_streamed_mesh_data) {
        HYP_LOG(Mesh, Warning, "Cannot calculate Mesh bounds before mesh data is set!");

        return;
    }

    auto ref = m_streamed_mesh_data->AcquireRef();
    const MeshData &mesh_data = ref->GetMeshData();

    BoundingBox aabb = BoundingBox::Empty();

    for (const Vertex &vertex : mesh_data.vertices) {
        aabb = aabb.Union(vertex.GetPosition());
    }

    m_aabb = aabb;
}

bool Mesh::BuildBVH(const Matrix4 &transform, BVHNode &out_bvh_node, int max_depth)
{
    if (!m_streamed_mesh_data) {
        return false;
    }

    auto ref = m_streamed_mesh_data->AcquireRef();
    const MeshData &mesh_data = ref->GetMeshData();
    
    out_bvh_node = BVHNode(m_aabb * transform);

    const Matrix4 normal_matrix = transform.Inverted().Transpose();

    for (uint32 i = 0; i < mesh_data.indices.Size(); i += 3) {
        Triangle triangle {
            mesh_data.vertices[mesh_data.indices[i + 0]],
            mesh_data.vertices[mesh_data.indices[i + 1]],
            mesh_data.vertices[mesh_data.indices[i + 2]]
        };

        triangle[0].position = transform * triangle[0].position;
        triangle[1].position = transform * triangle[1].position;
        triangle[2].position = transform * triangle[2].position;

        triangle[0].normal = (transform * Vec4f(triangle[0].normal.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[1].normal = (transform * Vec4f(triangle[1].normal.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[2].normal = (transform * Vec4f(triangle[2].normal.Normalized(), 0.0f)).GetXYZ().Normalize();

        triangle[0].tangent = (transform * Vec4f(triangle[0].tangent.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[1].tangent = (transform * Vec4f(triangle[1].tangent.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[2].tangent = (transform * Vec4f(triangle[2].tangent.Normalized(), 0.0f)).GetXYZ().Normalize();

        triangle[0].bitangent = (transform * Vec4f(triangle[0].bitangent.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[1].bitangent = (transform * Vec4f(triangle[1].bitangent.Normalized(), 0.0f)).GetXYZ().Normalize();
        triangle[2].bitangent = (transform * Vec4f(triangle[2].bitangent.Normalized(), 0.0f)).GetXYZ().Normalize();

        out_bvh_node.AddTriangle(triangle);
    }

    out_bvh_node.Split(max_depth);

    return true;
}

#pragma endregion Mesh

} // namespace hyperion