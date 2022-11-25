#include "Mesh.hpp"

#include "../Engine.hpp"

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <vector>
#include <unordered_map>
#include <cstring>

#define HYP_MESH_AABB_USE_MULTITHREADING 0

#if HYP_MESH_AABB_USE_MULTITHREADING
#include <thread>
#endif

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(UploadMeshData) : RenderCommandBase2
{
    std::vector<Float> vertex_data;
    std::vector<Mesh::Index> index_data;
    renderer::VertexBuffer *vbo;
    renderer::IndexBuffer *ibo;

    RENDER_COMMAND(UploadMeshData)(
        const std::vector<Float> &vertex_data,
        const std::vector<Mesh::Index> &index_data,
        renderer::VertexBuffer *vbo,
        renderer::IndexBuffer *ibo
    ) : vertex_data(vertex_data),
        index_data(index_data),
        vbo(vbo),
        ibo(ibo)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        auto *instance = engine->GetInstance();
        auto *device  = engine->GetDevice();

        const SizeType packed_buffer_size = vertex_data.size() * sizeof(Float);
        const SizeType packed_indices_size = index_data.size() * sizeof(Mesh::Index);

        HYPERION_BUBBLE_ERRORS(vbo->Create(device, packed_buffer_size));
        HYPERION_BUBBLE_ERRORS(ibo->Create(device, packed_indices_size));

        HYPERION_BUBBLE_ERRORS(instance->GetStagingBufferPool().Use(
            device,
            [&](renderer::StagingBufferPool::Context &holder) {
                auto commands = instance->GetSingleTimeCommands();

                auto *staging_buffer_vertices = holder.Acquire(packed_buffer_size);
                staging_buffer_vertices->Copy(device, packed_buffer_size, vertex_data.data());

                auto *staging_buffer_indices = holder.Acquire(packed_indices_size);
                staging_buffer_indices->Copy(device, packed_indices_size, index_data.data());

                commands.Push([&](CommandBuffer *cmd) {
                    vbo->CopyFrom(cmd, staging_buffer_vertices, packed_buffer_size);

                    HYPERION_RETURN_OK;
                });
            
                commands.Push([&](CommandBuffer *cmd) {
                    ibo->CopyFrom(cmd, staging_buffer_indices, packed_indices_size);

                    HYPERION_RETURN_OK;
                });
            
                HYPERION_BUBBLE_ERRORS(commands.Execute(device));

                HYPERION_RETURN_OK;
            }));
    
        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyMeshData) : RenderCommandBase2
{
    renderer::VertexBuffer *vbo;
    renderer::IndexBuffer *ibo;

    RENDER_COMMAND(DestroyMeshData)(
        renderer::VertexBuffer *vbo,
        renderer::IndexBuffer *ibo
    ) : vbo(vbo),
        ibo(ibo)
    {
    }

    virtual Result operator()(Engine *engine)
    {
        auto result = renderer::Result::OK;

        auto *device = engine->GetDevice();
        
        HYPERION_PASS_ERRORS(vbo->Destroy(device), result);
        HYPERION_PASS_ERRORS(ibo->Destroy(device), result);

        return result;
    }
};

std::pair<std::vector<Vertex>, std::vector<Mesh::Index>>
Mesh::CalculateIndices(const std::vector<Vertex> &vertices)
{
    std::unordered_map<Vertex, Index> index_map;

    std::vector<Index> indices;
    indices.reserve(vertices.size());

    /* This will be our resulting buffer with only the vertices we need. */
    std::vector<Vertex> new_vertices;
    new_vertices.reserve(vertices.size());

    for (const auto &vertex : vertices) {
        /* Check if the vertex already exists in our map */
        auto it = index_map.find(vertex);

        /* If it does, push to our indices */
        if (it != index_map.end()) {
            indices.push_back(it->second);

            continue;
        }

        const auto mesh_index = static_cast<Index>(new_vertices.size());

        /* The vertex is unique, so we push it. */
        new_vertices.push_back(vertex);
        indices.push_back(mesh_index);

        index_map[vertex] = mesh_index;
    }

    return std::make_pair(new_vertices, indices);
}

Mesh::Mesh(
    const std::vector<Vertex> &vertices,
    const std::vector<Index> &indices,
    Topology topology,
    const VertexAttributeSet &vertex_attributes,
    Flags flags
) : EngineComponentBase(),
    m_vbo(std::make_unique<VertexBuffer>()),
    m_ibo(std::make_unique<IndexBuffer>()),
    m_mesh_attributes {
        .vertex_attributes = vertex_attributes,
        .topology = topology
    },
    m_vertices(vertices),
    m_indices(indices),
    m_flags(flags)
{
}

Mesh::Mesh(
    const std::vector<Vertex> &vertices,
    const std::vector<Index> &indices,
    Topology topology,
    Flags flags
) : Mesh(
        vertices,
        indices,
        topology,
        renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes,
        flags
    )
{
}

Mesh::~Mesh()
{
    Teardown();
}

void Mesh::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    AssertThrowMsg(GetVertexAttributes() != 0, "No vertex attributes set on mesh");

    if (m_indices.size() < 3) {
        DebugLog(
            LogType::Warn,
            "Attempt to create Mesh #%u with empty vertices or indices list; setting vertices to be 1 empty vertex\n",
            m_id.value
        );

        if (m_vertices.empty()) {
            /* set to 1 vertex / index to prevent explosions */
            m_vertices = { Vertex() };
        }

        m_indices = { 0, 0, 0 };
    } else if (m_indices.size() % 3 != 0) {
        DebugLog(
            LogType::Warn,
            "Index count not a multiple of 3! Padding with extra indices...",
            m_id.value
        );

        if (m_vertices.empty()) {
            /* set to 1 vertex / index to prevent explosions */
            m_vertices = { Vertex() };
        }

        const SizeType remainder = m_indices.size() % 3;

        for (SizeType i = 0; i < remainder; i++) {
            m_indices.push_back(0);
        }
    }

    m_indices_count = m_indices.size();

    RenderCommands::Push<RENDER_COMMAND(UploadMeshData)>(
        BuildVertexBuffer(),
        m_indices,
        m_vbo.get(),
        m_ibo.get()
    );
            
    SetReady(true);

    OnTeardown([this]() {
        auto *engine = GetEngine();

        DebugLog(
            LogType::Debug,
            "Destroy mesh with id %u\n",
            GetID().value
        );

        m_indices_count = 0;

        RenderCommands::Push<RENDER_COMMAND(DestroyMeshData)>(
            m_vbo.get(),
            m_ibo.get()
        );
        
        HYP_FLUSH_RENDER_QUEUE(engine);

        SetReady(false);
    });
}

/* Copy our values into the packed vertex buffer, and increase the index for the next possible
 * mesh attribute. This macro helps keep the code cleaner and easier to maintain. */
#define PACKED_SET_ATTR(raw_values, arg_size)                                                    \
    do {                                                                                         \
        Memory::Copy((void *)(raw_buffer + current_offset), (raw_values), (arg_size) * sizeof(float)); \
        current_offset += (arg_size);                                                            \
    } while (0)

std::vector<float> Mesh::BuildVertexBuffer()
{
    const size_t vertex_size = GetVertexAttributes().CalculateVertexSize();

    std::vector<float> packed_buffer(vertex_size * m_vertices.size());

    /* Raw buffer that is used with our helper macro. */
    float *raw_buffer = packed_buffer.data();
    SizeType current_offset = 0;

    for (size_t i = 0; i < m_vertices.size(); i++) {
        auto &vertex = m_vertices[i];
        /* Offset aligned to the current vertex */
        //current_offset = i * vertex_size;

        /* Position and normals */
        if (GetVertexAttributes() & VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION)  PACKED_SET_ATTR(vertex.GetPosition().values, 3);
        if (GetVertexAttributes() & VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL)    PACKED_SET_ATTR(vertex.GetNormal().values,   3);
        /* Texture coordinates */
        if (GetVertexAttributes() & VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0) PACKED_SET_ATTR(vertex.GetTexCoord0().values, 2);
        if (GetVertexAttributes() & VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1) PACKED_SET_ATTR(vertex.GetTexCoord1().values, 2);
        /* Tangents and Bitangents */
        if (GetVertexAttributes() & VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT)   PACKED_SET_ATTR(vertex.GetTangent().values,   3);
        if (GetVertexAttributes() & VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT) PACKED_SET_ATTR(vertex.GetBitangent().values, 3);

        /* TODO: modify GetBoneIndex/GetBoneWeight to return a Vector4. */
        if (GetVertexAttributes() & VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS) {
            float weights[4] = {
                vertex.GetBoneWeight(0), vertex.GetBoneWeight(1),
                vertex.GetBoneWeight(2), vertex.GetBoneWeight(3)
            };
            PACKED_SET_ATTR(weights, std::size(weights));
        }

        if (GetVertexAttributes() & VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES) {
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

void Mesh::Render(Engine *, CommandBuffer *cmd) const
{
    //Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    AssertThrow(m_vbo != nullptr && m_ibo != nullptr);

    m_vbo->Bind(cmd);
    m_ibo->Bind(cmd);

    cmd->DrawIndexed(static_cast<UInt32>(m_indices_count));
}

void Mesh::RenderIndirect(
    Engine *engine,
    CommandBuffer *cmd,
    const IndirectBuffer *indirect_buffer,
    UInt32 buffer_offset
) const
{
    AssertReady();

    AssertThrow(m_vbo != nullptr && m_ibo != nullptr);

    m_vbo->Bind(cmd);
    m_ibo->Bind(cmd);

    cmd->DrawIndexedIndirect(
        indirect_buffer,
        buffer_offset
    );
}

void Mesh::PopulateIndirectDrawCommand(IndirectDrawCommand &out)
{
#if HYP_VULKAN
    out = {
        .command = {
            .indexCount = static_cast<UInt32>(m_indices_count)
        }
    };
#else
    #error Not implemented for this platform!
#endif
}

std::vector<PackedVertex> Mesh::BuildPackedVertices() const
{
    std::vector<PackedVertex> packed_vertices;
    packed_vertices.resize(m_vertices.size());

    for (SizeType i = 0; i < m_vertices.size(); i++) {
        const auto &vertex = m_vertices[i];

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

std::vector<PackedIndex> Mesh::BuildPackedIndices() const
{
    AssertThrow(m_indices.size() % 3 == 0);
    return std::vector<PackedIndex>(m_indices.begin(), m_indices.end());
}

void Mesh::CalculateNormals(bool weighted)
{
    if (m_indices.empty()) {
        DebugLog(LogType::Warn, "Cannot calculate normals before indices are generated!\n");

        return;
    }

    std::unordered_map<Index, Array<Vector3>> normals;

    // compute per-face normals (facet normals)
    for (SizeType i = 0; i < m_indices.size(); i += 3) {
        const Index i0 = m_indices[i];
        const Index i1 = m_indices[i + 1];
        const Index i2 = m_indices[i + 2];

        const Vector3 &p0 = m_vertices[i0].GetPosition();
        const Vector3 &p1 = m_vertices[i1].GetPosition();
        const Vector3 &p2 = m_vertices[i2].GetPosition();

        const Vector3 u = p2 - p0;
        const Vector3 v = p1 - p0;
        const Vector3 n = v.Cross(u);//.Normalize();

        normals[i0].PushBack(n);
        normals[i1].PushBack(n);
        normals[i2].PushBack(n);
    }

    for (SizeType i = 0; i < m_vertices.size(); i++) {
        if (weighted) {
            m_vertices[i].SetNormal(normals[i].Sum());
        } else {
            m_vertices[i].SetNormal(normals[i].Sum().Normalize());
        }
    }

    if (!weighted) {
        return;
    }

    normals.clear();

    // weighted (smooth) normals

    for (SizeType i = 0; i < m_indices.size(); i += 3) {
        const Index i0 = m_indices[i];
        const Index i1 = m_indices[i + 1];
        const Index i2 = m_indices[i + 2];

        const auto &p0 = m_vertices[i0].GetPosition();
        const auto &p1 = m_vertices[i1].GetPosition();
        const auto &p2 = m_vertices[i2].GetPosition();

        const auto &n0 = m_vertices[i0].GetNormal();
        const auto &n1 = m_vertices[i1].GetNormal();
        const auto &n2 = m_vertices[i2].GetNormal();

        // Vector3 n = FixedArray { n0, n1, n2 }.Avg();

        FixedArray weighted_normals { n0, n1, n2 };

        // nested loop through faces to get weighted neighbours
        // any code that uses this really should bake the normals in
        // especially for any production code. this is an expensive process
        for (size_t j = 0; j < m_indices.size(); j += 3) {
            if (j == i) {
                continue;
            }

            const Index j0 = m_indices[j];
            const Index j1 = m_indices[j + 1];
            const Index j2 = m_indices[j + 2];

            const FixedArray face_positions {
                m_vertices[j0].GetPosition(),
                m_vertices[j1].GetPosition(),
                m_vertices[j2].GetPosition()
            };

            const FixedArray face_normals {
                m_vertices[j0].GetNormal(),
                m_vertices[j1].GetNormal(),
                m_vertices[j2].GetNormal()
            };

            const auto a = p1 - p0;
            const auto b = p2 - p0;
            const auto c = a.Cross(b);

            const auto area = 0.5f * MathUtil::Sqrt(c.Dot(c));

            if (face_positions.Contains(p0)) {
                const auto angle = (p0 - p1).AngleBetween(p0 - p2);
                weighted_normals[0] += face_normals.Avg() * area * angle;
            }

            if (face_positions.Contains(p1)) {
                const auto angle = (p1 - p0).AngleBetween(p1 - p2);
                weighted_normals[1] += face_normals.Avg() * area * angle;
            }

            if (face_positions.Contains(p2)) {
                const auto angle = (p2 - p0).AngleBetween(p2 - p1);
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

    for (size_t i = 0; i < m_vertices.size(); i++) {
        m_vertices[i].SetNormal(normals[i].Sum().Normalized());
    }

    normals.clear();

}

void Mesh::CalculateTangents()
{
#if 1
    struct TangentBitangentPair
    {
        Vector3 tangent;
        Vector3 bitangent;
    };

    std::unordered_map<Index, std::vector<TangentBitangentPair>> data;

    for (SizeType i = 0; i < m_indices.size();) {
        const auto count = MathUtil::Min(3, m_indices.size() - i);

        Vertex v[3];
        Vector2 uv[3];

        for (UInt32 j = 0; j < count; j++) {
            v[j]  = m_vertices[m_indices[i + j]];
            uv[j] = v[j].GetTexCoord0();
        }

        Index i0 = m_indices[i];
        Index i1 = m_indices[i + 1];
        Index i2 = m_indices[i + 2];
        
        const Vector3 edge1   = v[1].GetPosition() - v[0].GetPosition();
        const Vector3 edge2   = v[2].GetPosition() - v[0].GetPosition();
        const Vector2 edge1uv = uv[1] - uv[0];
        const Vector2 edge2uv = uv[2] - uv[0];
        
        const float cp = edge1uv.x * edge2uv.y - edge1uv.y * edge2uv.x;

        if (!MathUtil::ApproxEqual(cp, 0.0f)) {
            const float mul = 1.0f / cp;

            const TangentBitangentPair tangent_bitangent {
                .tangent   = ((edge1 * edge2uv.y - edge2 * edge1uv.y) * mul).Normalize(),
                .bitangent = ((edge1 * edge2uv.x - edge2 * edge1uv.x) * mul).Normalize()
            };

            data[i0].push_back(tangent_bitangent);
            data[i1].push_back(tangent_bitangent);
            data[i2].push_back(tangent_bitangent);
        }

        i += count;
    }

    for (SizeType i = 0; i < m_vertices.size(); i++) {
        const auto &tangent_bitangents = data[i];

        // find average
        Vector3 average_tangent,
                average_bitangent;

        for (const auto &item : tangent_bitangents) {
            average_tangent   += item.tangent * (1.0f / tangent_bitangents.size());
            average_bitangent += item.bitangent * (1.0f / tangent_bitangents.size());
        }

        average_tangent.Normalize();
        average_bitangent.Normalize();

        m_vertices[i].SetTangent(average_tangent);
        m_vertices[i].SetBitangent(average_bitangent);
    }
#else
    Vertex v[3];
    Vector2 uv[3];

    for (auto &vertex : m_vertices) {
        vertex.SetTangent(Vector3(0.0f));
        vertex.SetBitangent(Vector3(0.0f));
    }

    std::vector<Vector3> new_tangents(m_vertices.size());
    std::vector<Vector3> new_bitangents(m_vertices.size());

    for (SizeType i = 0; i < m_indices.size();) {
        const auto count = MathUtil::Min(3, m_indices.size() - i);

        for (UInt32 j = 0; j < count; j++) {
            v[j]  = m_vertices[m_indices[i + j]];
            uv[j] = v[j].GetTexCoord0();
        }

        const Vector3 edge1   = v[1].GetPosition() - v[0].GetPosition();
        const Vector3 edge2   = v[2].GetPosition() - v[0].GetPosition();
        const Vector2 edge1uv = uv[1] - uv[0];
        const Vector2 edge2uv = uv[2] - uv[0];
        
        const float mul = 1.0f / (edge1uv.x * edge2uv.y - edge1uv.y * edge2uv.x);

        const Vector3 tangent   = (edge1 * edge2uv.y - edge2 * edge1uv.y) * mul;
        const Vector3 bitangent = (edge1 * edge2uv.x - edge2 * edge1uv.x) * mul;

        for (UInt32 j = 0; j < count; j++) {
            new_tangents[m_indices[i + j]]   += tangent;
            new_bitangents[m_indices[i + j]] += bitangent;
        }

        i += count;
    }

    for (SizeType i = 0; i < m_vertices.size(); i++) {
        Vector3 n = m_vertices[i].GetNormal();
        Vector3 tangent = (new_tangents[i] - (n * n.Dot(new_tangents[i])));
        Vector3 cross = n.Cross(new_tangents[i]);

        Vector3 bitangent = cross * MathUtil::Sign(cross.Dot(new_bitangents[i]));

        m_vertices[i].SetTangent(tangent);
        m_vertices[i].SetBitangent(bitangent);
    }

#endif
}

void Mesh::InvertNormals()
{
    for (Vertex &vertex : m_vertices) {
        vertex.SetNormal(vertex.GetNormal() * -1.0f);
    }
}

BoundingBox Mesh::CalculateAABB() const
{
    BoundingBox aabb;

#if HYP_MESH_AABB_USE_MULTITHREADING
    constexpr size_t max_threads = 8;
    constexpr size_t vertex_count_threshold = 512;

    if (m_vertices.size() > vertex_count_threshold) {
        std::vector<std::thread> threads;
        threads.reserve(max_threads);

        const int vertex_stride = MathUtil::Ceil(double(m_vertices.size()) / double(max_threads));

        std::array<BoundingBox, max_threads> working_aabbs;

        for (size_t i = 0; i < max_threads; i++) {
            const size_t vertex_offset = vertex_stride * i;

            threads.emplace_back([this, &working_aabbs, vertex_offset, vertex_stride, i] {
                const size_t vertex_end = MathUtil::Min(m_vertices.size(), vertex_offset + vertex_stride);
                
                for (size_t j = vertex_offset; j < vertex_end; j++) {
                    working_aabbs[i].Extend(m_vertices[j].GetPosition());
                }
            });
        }

        for (size_t i = 0; i < threads.size(); i++) {
            threads[i].join();

            aabb.Extend(working_aabbs[i]);
        }
    } else {
#endif
        for (const Vertex &vertex : m_vertices) {
            aabb.Extend(vertex.GetPosition());
        }
#if HYP_MESH_AABB_USE_MULTITHREADING
    }
#endif

    return aabb;
}

}
