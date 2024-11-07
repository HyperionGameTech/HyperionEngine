/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/Mesh.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

#include <unordered_map>
#include <cstring>

namespace hyperion {

using renderer::Result;
using renderer::GPUBufferType;

#pragma region Render commands

struct RENDER_COMMAND(UploadMeshData) : renderer::RenderCommand
{
    Array<float>    vertex_data;
    Array<uint32>   index_data;
    GPUBufferRef    vbo;
    GPUBufferRef    ibo;

    RENDER_COMMAND(UploadMeshData)(
        Array<float> vertex_data,
        Array<uint32> index_data,
        GPUBufferRef vbo,
        GPUBufferRef ibo
    ) : vertex_data(std::move(vertex_data)),
        index_data(std::move(index_data)),
        vbo(std::move(vbo)),
        ibo(std::move(ibo))
    {
        // Ensure vertex buffer is not empty
        if (this->vertex_data.Empty()) {
            this->vertex_data.Resize(1);
        }

        // Ensure indices exist and are a multiple of 3
        if (this->index_data.Empty()) {
            this->index_data.Resize(3);
        } else if (this->index_data.Size() % 3 != 0) {
            this->index_data.Resize(this->index_data.Size() + (3 - (this->index_data.Size() % 3)));
        }
    }

    virtual ~RENDER_COMMAND(UploadMeshData)() override = default;

    virtual Result operator()() override
    {
        auto *instance = g_engine->GetGPUInstance();
        auto *device = g_engine->GetGPUDevice();

        const SizeType packed_buffer_size = vertex_data.Size() * sizeof(float);
        const SizeType packed_indices_size = index_data.Size() * sizeof(uint32);

        HYPERION_BUBBLE_ERRORS(vbo->Create(device, packed_buffer_size));
        HYPERION_BUBBLE_ERRORS(ibo->Create(device, packed_indices_size));

        HYPERION_BUBBLE_ERRORS(instance->GetStagingBufferPool().Use(
            device,
            [&](renderer::StagingBufferPool::Context &holder)
            {
                renderer::SingleTimeCommands commands { device };

                auto *staging_buffer_vertices = holder.Acquire(packed_buffer_size);
                staging_buffer_vertices->Copy(device, packed_buffer_size, vertex_data.Data());

                auto *staging_buffer_indices = holder.Acquire(packed_indices_size);
                staging_buffer_indices->Copy(device, packed_indices_size, index_data.Data());

                commands.Push([&](CommandBuffer *cmd)
                {
                    vbo->CopyFrom(cmd, staging_buffer_vertices, packed_buffer_size);

                    HYPERION_RETURN_OK;
                });

                commands.Push([&](CommandBuffer *cmd)
                {
                    ibo->CopyFrom(cmd, staging_buffer_indices, packed_indices_size);

                    HYPERION_RETURN_OK;
                });

                HYPERION_BUBBLE_ERRORS(commands.Execute());

                HYPERION_RETURN_OK;
            }));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(SetStreamedMeshData) : renderer::RenderCommand
{
    Handle<Mesh>            mesh;
    RC<StreamedMeshData>    streamed_mesh_data;

    RENDER_COMMAND(SetStreamedMeshData)(
        Handle<Mesh> mesh,
        RC<StreamedMeshData> streamed_mesh_data
    ) : mesh(std::move(mesh)),
        streamed_mesh_data(std::move(streamed_mesh_data))
    {
        AssertThrow(this->mesh.IsValid());
        AssertThrow(this->streamed_mesh_data != nullptr);
    }

    virtual ~RENDER_COMMAND(SetStreamedMeshData)() override = default;

    virtual Result operator()() override
    {
        mesh->SetStreamedMeshData(std::move(streamed_mesh_data));

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

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
    : BasicObject(),
      m_vbo(MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_VERTEX_BUFFER)),
      m_ibo(MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_INDEX_BUFFER)),
      m_mesh_attributes {
          .vertex_attributes    = static_mesh_vertex_attributes,
          .topology             = Topology::TRIANGLES
      },
      m_aabb(BoundingBox::Empty())
{
}

Mesh::Mesh(
    RC<StreamedMeshData> streamed_mesh_data,
    Topology topology,
    const VertexAttributeSet &vertex_attributes
) : BasicObject(),
    m_vbo(MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_VERTEX_BUFFER)),
    m_ibo(MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_INDEX_BUFFER)),
    m_mesh_attributes {
        .vertex_attributes  = vertex_attributes,
        .topology           = topology
    },
    m_streamed_mesh_data(std::move(streamed_mesh_data)),
    m_aabb(BoundingBox::Empty())
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
) : BasicObject(),
    m_vbo(MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_VERTEX_BUFFER)),
    m_ibo(MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_INDEX_BUFFER)),
    m_mesh_attributes {
        .vertex_attributes = vertex_attributes,
        .topology = topology
    },
    m_streamed_mesh_data(MakeRefCountedPtr<StreamedMeshData>(MeshData {
        std::move(vertices),
        std::move(indices)
    })),
    m_aabb(BoundingBox::Empty())
{
    auto ref = m_streamed_mesh_data->AcquireRef();
    const MeshData &mesh_data = ref->GetMeshData();

    m_indices_count = mesh_data.indices.Size();

    CalculateAABB();
}

Mesh::Mesh(Mesh &&other) noexcept
    : m_vbo(std::move(other.m_vbo)),
      m_ibo(std::move(other.m_ibo)),
      m_indices_count(other.m_indices_count),
      m_mesh_attributes(other.m_mesh_attributes),
      m_streamed_mesh_data(std::move(other.m_streamed_mesh_data)),
      m_aabb(other.m_aabb)
{
    other.m_aabb = BoundingBox::Empty();
    other.m_indices_count = 0;
}

Mesh &Mesh::operator=(Mesh &&other) noexcept
{
    if (&other == this) {
        return *this;
    }

    m_vbo = std::move(other.m_vbo);
    m_ibo = std::move(other.m_ibo);
    m_mesh_attributes = other.m_mesh_attributes;
    m_streamed_mesh_data = std::move(other.m_streamed_mesh_data);
    m_aabb = other.m_aabb;
    m_indices_count = other.m_indices_count;

    other.m_aabb = BoundingBox::Empty();
    other.m_indices_count = 0;

    return *this;
}

Mesh::~Mesh()
{
    if (IsInitCalled()) {
        SetReady(false);

        SafeRelease(std::move(m_vbo));
        SafeRelease(std::move(m_ibo));
    }
}

void Mesh::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        SafeRelease(std::move(m_vbo));
        SafeRelease(std::move(m_ibo));
    }));

    AssertThrowMsg(GetVertexAttributes() != 0, "No vertex attributes set on mesh");

    {
        HYP_MT_CHECK_RW(m_data_race_detector, "Streamed mesh data");

        if (!m_streamed_mesh_data) {
            m_streamed_mesh_data.Emplace();
        }

        // upload mesh data
        auto ref = m_streamed_mesh_data->AcquireRef();
        const MeshData &mesh_data = ref->GetMeshData();

        m_indices_count = uint32(mesh_data.indices.Size());

        PUSH_RENDER_COMMAND(
            UploadMeshData,
            BuildVertexBuffer(GetVertexAttributes(), mesh_data),
            mesh_data.indices,
            m_vbo,
            m_ibo
        );
        
        m_streamed_mesh_data->Unpage();
    }

    SetReady(true);
}

void Mesh::SetVertices(Array<Vertex> vertices)
{
    Array<uint32> indices;
    indices.Resize(vertices.Size());

    for (SizeType index = 0; index < vertices.Size(); index++) {
        indices[index] = uint32(index);
    }

    SetVertices(std::move(vertices), std::move(indices));
}

void Mesh::SetVertices(Array<Vertex> vertices, Array<uint32> indices)
{
    SetStreamedMeshData(StreamedMeshData::FromMeshData({
       std::move(vertices),
       std::move(indices)
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
        SafeRelease(std::move(m_vbo));
        SafeRelease(std::move(m_ibo));

        m_vbo = MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_VERTEX_BUFFER);
        m_ibo = MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_INDEX_BUFFER);

        if (!m_streamed_mesh_data) {
            // Create empty buffers
            m_streamed_mesh_data = StreamedMeshData::FromMeshData({
                Array<Vertex> { },
                Array<uint32> { }
            });
        }

        auto ref = m_streamed_mesh_data->AcquireRef();
        const MeshData &mesh_data = ref->GetMeshData();

        m_indices_count = mesh_data.indices.Size();

        // Execute render command inline
        PUSH_RENDER_COMMAND(
            UploadMeshData,
            BuildVertexBuffer(GetVertexAttributes(), mesh_data),
            mesh_data.indices,
            m_vbo,
            m_ibo
        );
    }
}

void Mesh::SetStreamedMeshData_ThreadSafe(RC<StreamedMeshData> streamed_mesh_data)
{
    HYP_SCOPE;

    AssertIsInitCalled();

    PUSH_RENDER_COMMAND(
        SetStreamedMeshData,
        HandleFromThis(),
        std::move(streamed_mesh_data)
    );
}

/* Copy our values into the packed vertex buffer, and increase the index for the next possible
 * mesh attribute. This macro helps keep the code cleaner and easier to maintain. */
#define PACKED_SET_ATTR(raw_values, arg_size)                                                    \
    do {                                                                                         \
        Memory::MemCpy((void *)(raw_buffer + current_offset), (raw_values), (arg_size) * sizeof(float)); \
        current_offset += (arg_size);                                                            \
    } while (0)

Array<float> Mesh::BuildVertexBuffer(const VertexAttributeSet &vertex_attributes, const MeshData &mesh_data)
{
    const SizeType vertex_size = vertex_attributes.CalculateVertexSize();

    Array<float> packed_buffer;
    packed_buffer.Resize(vertex_size * mesh_data.vertices.Size());

    /* Raw buffer that is used with our helper macro. */
    float *raw_buffer = packed_buffer.Data();
    SizeType current_offset = 0;

    for (SizeType i = 0; i < mesh_data.vertices.Size(); i++) {
        const Vertex &vertex = mesh_data.vertices[i];
        /* Offset aligned to the current vertex */
        //current_offset = i * vertex_size;

        /* Position and normals */
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION)  PACKED_SET_ATTR(vertex.GetPosition().values, 3);
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL)    PACKED_SET_ATTR(vertex.GetNormal().values,   3);
        /* Texture coordinates */
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0) PACKED_SET_ATTR(vertex.GetTexCoord0().values, 2);
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1) PACKED_SET_ATTR(vertex.GetTexCoord1().values, 2);
        /* Tangents and Bitangents */
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT)   PACKED_SET_ATTR(vertex.GetTangent().values,   3);
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT) PACKED_SET_ATTR(vertex.GetBitangent().values, 3);

        /* TODO: modify GetBoneIndex/GetBoneWeight to return a Vector4. */
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS) {
            float weights[4] = {
                vertex.GetBoneWeight(0), vertex.GetBoneWeight(1),
                vertex.GetBoneWeight(2), vertex.GetBoneWeight(3)
            };
            PACKED_SET_ATTR(weights, std::size(weights));
        }

        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES) {
            float indices[4] = {
                    (float)vertex.GetBoneIndex(0), (float)vertex.GetBoneIndex(1),
                    (float)vertex.GetBoneIndex(2), (float)vertex.GetBoneIndex(3)
            };
            PACKED_SET_ATTR(indices, std::size(indices));
        }
    }

    return packed_buffer;
}

#undef PACKED_SET_ATTR

void Mesh::Render(CommandBuffer *cmd, uint32 num_instances, uint32 instance_index) const
{
#ifdef HYP_DEBUG_MODE
    AssertReady();
#endif

    cmd->BindVertexBuffer(m_vbo);
    cmd->BindIndexBuffer(m_ibo);

    cmd->DrawIndexed(m_indices_count, num_instances, instance_index);
}

void Mesh::RenderIndirect(
    CommandBuffer *cmd,
    const GPUBuffer *indirect_buffer,
    uint32 buffer_offset
) const
{
#ifdef HYP_DEBUG_MODE
    AssertReady();
#endif

    cmd->BindVertexBuffer(m_vbo);
    cmd->BindIndexBuffer(m_ibo);

    cmd->DrawIndexedIndirect(
        indirect_buffer,
        buffer_offset
    );
}

void Mesh::PopulateIndirectDrawCommand(IndirectDrawCommand &out)
{
#if HYP_VULKAN
    out.command = {
        .indexCount = m_indices_count
    };
#else
    #error Not implemented for this platform!
#endif
}

Array<PackedVertex> Mesh::BuildPackedVertices() const
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_data_race_detector, "Streamed mesh data");

    auto ref = m_streamed_mesh_data->AcquireRef();
    const MeshData &mesh_data = ref->GetMeshData();

    Array<PackedVertex> packed_vertices;
    packed_vertices.Resize(mesh_data.vertices.Size());

    for (SizeType i = 0; i < mesh_data.vertices.Size(); i++) {
        const auto &vertex = mesh_data.vertices[i];

        packed_vertices[i] = PackedVertex {
            .position_x = vertex.GetPosition().x,
            .position_y = vertex.GetPosition().y,
            .position_z = vertex.GetPosition().z,
            .normal_x = vertex.GetNormal().x,
            .normal_y = vertex.GetNormal().y,
            .normal_z = vertex.GetNormal().z,
            .texcoord0_x = vertex.GetTexCoord0().x,
            .texcoord0_y = vertex.GetTexCoord0().y
        };
    }

    return packed_vertices;
}

Array<uint32> Mesh::BuildPackedIndices() const
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_data_race_detector, "Streamed mesh data");

    auto ref = m_streamed_mesh_data->AcquireRef();
    const MeshData &mesh_data = ref->GetMeshData();

    AssertThrow(mesh_data.indices.Size() % 3 == 0);

    return Array<uint32>(mesh_data.indices.Begin(), mesh_data.indices.End());
}

void Mesh::CalculateNormals(bool weighted)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_data_race_detector, "Streamed mesh data");

    if (!m_streamed_mesh_data) {
        HYP_LOG(Mesh, LogLevel::WARNING, "Cannot calculate normals before mesh data is set!");

        return;
    }

    auto ref = m_streamed_mesh_data->AcquireRef();
    MeshData mesh_data = ref->GetMeshData();

    if (mesh_data.indices.Empty()) {
        HYP_LOG(Mesh, LogLevel::WARNING, "Cannot calculate normals before indices are generated!");

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
        HYP_LOG(Mesh, LogLevel::WARNING, "Cannot calculate normals before mesh data is set!");

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
        HYP_LOG(Mesh, LogLevel::WARNING, "Cannot invert normals before mesh data is set!");

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
        HYP_LOG(Mesh, LogLevel::WARNING, "Cannot calculate Mesh bounds before mesh data is set!");

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

} // namespace hyperion