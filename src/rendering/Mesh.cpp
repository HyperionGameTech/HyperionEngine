#include "Mesh.hpp"

#include "../Engine.hpp"

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <script/ScriptApi.hpp>
#include <script/ScriptBindingDef.generated.hpp>

#include <vector>
#include <unordered_map>
#include <cstring>

namespace hyperion::v2 {

using renderer::Result;
using renderer::GPUBufferType;

struct RENDER_COMMAND(UploadMeshData) : renderer::RenderCommand
{
    Array<Float> vertex_data;
    Array<Mesh::Index> index_data;
    GPUBufferRef vbo;
    GPUBufferRef ibo;

    RENDER_COMMAND(UploadMeshData)(
        const Array<Float> &vertex_data,
        const Array<Mesh::Index> &index_data,
        const GPUBufferRef &vbo,
        const GPUBufferRef &ibo
    ) : vertex_data(vertex_data),
        index_data(index_data),
        vbo(vbo),
        ibo(ibo)
    {
    }

    virtual Result operator()()
    {
        auto *instance = g_engine->GetGPUInstance();
        auto *device = g_engine->GetGPUDevice();

        const SizeType packed_buffer_size = vertex_data.Size() * sizeof(Float);
        const SizeType packed_indices_size = index_data.Size() * sizeof(Mesh::Index);

        HYPERION_BUBBLE_ERRORS(vbo->Create(device, packed_buffer_size));
        HYPERION_BUBBLE_ERRORS(ibo->Create(device, packed_indices_size));

        HYPERION_BUBBLE_ERRORS(instance->GetStagingBufferPool().Use(
            device,
            [&](renderer::StagingBufferPool::Context &holder) {
                auto commands = instance->GetSingleTimeCommands();

                auto *staging_buffer_vertices = holder.Acquire(packed_buffer_size);
                staging_buffer_vertices->Copy(device, packed_buffer_size, vertex_data.Data());

                auto *staging_buffer_indices = holder.Acquire(packed_indices_size);
                staging_buffer_indices->Copy(device, packed_indices_size, index_data.Data());

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

Pair<Array<Vertex>, Array<Mesh::Index>>
Mesh::CalculateIndices(const Array<Vertex> &vertices)
{
    std::unordered_map<Vertex, Index> index_map;

    Array<Index> indices;
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

        const auto mesh_index = static_cast<Index>(new_vertices.Size());

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
          .vertex_attributes = renderer::static_mesh_vertex_attributes,
          .topology = Topology::TRIANGLES
      }
{
}

Mesh::Mesh(
    const Array<Vertex> &vertices,
    const Array<Index> &indices,
    Topology topology,
    const VertexAttributeSet &vertex_attributes
) : BasicObject(),
    m_vbo(MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_VERTEX_BUFFER)),
    m_ibo(MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_INDEX_BUFFER)),
    m_mesh_attributes {
        .vertex_attributes = vertex_attributes,
        .topology = topology
    },
    m_vertices(vertices),
    m_indices(indices),
    m_aabb(BoundingBox::empty)
{
    CalculateAABB();
}

Mesh::Mesh(
    const Array<Vertex> &vertices,
    const Array<Index> &indices,
    Topology topology
) : Mesh(
        vertices,
        indices,
        topology,
        renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes
    )
{
}

Mesh::Mesh(Mesh &&other) noexcept
    : m_vbo(std::move(other.m_vbo)),
      m_ibo(std::move(other.m_ibo)),
      m_mesh_attributes(other.m_mesh_attributes),
      m_vertices(std::move(other.m_vertices)),
      m_indices(std::move(other.m_indices)),
      m_aabb(other.m_aabb)
{
    other.m_aabb = BoundingBox::empty;
}

Mesh &Mesh::operator=(Mesh &&other) noexcept
{
    if (&other == this) {
        return *this;
    }

    m_vbo = std::move(other.m_vbo);
    m_ibo = std::move(other.m_ibo);
    m_mesh_attributes = other.m_mesh_attributes;
    m_vertices = std::move(other.m_vertices);
    m_indices = std::move(other.m_indices);
    m_aabb = other.m_aabb;

    other.m_aabb = BoundingBox::empty;

    return *this;
}

Mesh::~Mesh()
{
    if (IsInitCalled()) {
        SetReady(false);

        DebugLog(
            LogType::Debug,
            "Destroy mesh with id %u\n",
            GetID().Value()
        );

        m_indices_count = 0;

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

    AssertThrowMsg(GetVertexAttributes() != 0, "No vertex attributes set on mesh");

    if (m_indices.Size() < 3) {
        DebugLog(
            LogType::Warn,
            "Attempt to create Mesh #%u with empty vertices or indices list; setting vertices to be 1 empty vertex\n",
            m_id.value
        );

        if (m_vertices.Empty()) {
            /* set to 1 vertex / index to prevent explosions */
            m_vertices = { Vertex() };
        }

        m_indices = { 0, 0, 0 };
    } else if (m_indices.Size() % 3 != 0) {
        DebugLog(
            LogType::Warn,
            "Index count not a multiple of 3! Padding with extra indices...",
            m_id.value
        );

        if (m_vertices.Empty()) {
            /* set to 1 vertex / index to prevent explosions */
            m_vertices = { Vertex() };
        }

        const SizeType remainder = m_indices.Size() % 3;

        for (SizeType i = 0; i < remainder; i++) {
            m_indices.PushBack(0);
        }
    }

    m_indices_count = UInt(m_indices.Size());

    PUSH_RENDER_COMMAND(
        UploadMeshData,
        BuildVertexBuffer(),
        m_indices,
        m_vbo,
        m_ibo
    );

    //m_vertices.clear();
    //m_indices.clear();
            
    SetReady(true);
}

void Mesh::SetVertices(const Array<Vertex> &vertices)
{
    m_vertices.Resize(vertices.Size());
    m_indices.Resize(vertices.Size());

    for (SizeType index = 0; index < vertices.Size(); index++) {
        m_vertices[index] = vertices[index];
        m_indices[index] = Index(index);
    }

    CalculateAABB();
}

void Mesh::SetIndices(const Array<Index> &indices)
{
    m_indices.Resize(indices.Size());

    Memory::MemCpy(m_indices.Data(), indices.Data(), indices.Size() * sizeof(Index));
}

/* Copy our values into the packed vertex buffer, and increase the index for the next possible
 * mesh attribute. This macro helps keep the code cleaner and easier to maintain. */
#define PACKED_SET_ATTR(raw_values, arg_size)                                                    \
    do {                                                                                         \
        Memory::MemCpy((void *)(raw_buffer + current_offset), (raw_values), (arg_size) * sizeof(float)); \
        current_offset += (arg_size);                                                            \
    } while (0)

Array<Float> Mesh::BuildVertexBuffer()
{
    const SizeType vertex_size = GetVertexAttributes().CalculateVertexSize();

    Array<Float> packed_buffer;
    packed_buffer.Resize(vertex_size * m_vertices.Size());

    /* Raw buffer that is used with our helper macro. */
    Float *raw_buffer = packed_buffer.Data();
    SizeType current_offset = 0;

    for (SizeType i = 0; i < m_vertices.Size(); i++) {
        const Vertex &vertex = m_vertices[i];
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

void Mesh::Render(CommandBuffer *cmd, UInt num_instances) const
{
    cmd->BindVertexBuffer(m_vbo);
    cmd->BindIndexBuffer(m_ibo);

    cmd->DrawIndexed(m_indices_count, num_instances);
}

void Mesh::RenderIndirect(
    CommandBuffer *cmd,
    const GPUBuffer *indirect_buffer,
    UInt32 buffer_offset
) const
{
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
    Array<PackedVertex> packed_vertices;
    packed_vertices.Resize(m_vertices.Size());

    for (SizeType i = 0; i < m_vertices.Size(); i++) {
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

Array<PackedIndex> Mesh::BuildPackedIndices() const
{
    AssertThrow(m_indices.Size() % 3 == 0);

    return Array<PackedIndex>(m_indices.Begin(), m_indices.End());
}

void Mesh::CalculateNormals(bool weighted)
{
    if (m_indices.Empty()) {
        DebugLog(LogType::Warn, "Cannot calculate normals before indices are generated!\n");

        return;
    }

    std::unordered_map<Index, Array<Vector3>> normals;

    // compute per-face normals (facet normals)
    for (SizeType i = 0; i < m_indices.Size(); i += 3) {
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

    for (SizeType i = 0; i < m_vertices.Size(); i++) {
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

    for (SizeType i = 0; i < m_indices.Size(); i += 3) {
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
        for (SizeType j = 0; j < m_indices.Size(); j += 3) {
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

    for (SizeType i = 0; i < m_vertices.Size(); i++) {
        m_vertices[i].SetNormal(normals[i].Sum().Normalized());
    }

    normals.clear();

}

void Mesh::CalculateTangents()
{
    struct TangentBitangentPair
    {
        Vector3 tangent;
        Vector3 bitangent;
    };

    std::unordered_map<Index, Array<TangentBitangentPair>> data;

    for (SizeType i = 0; i < m_indices.Size();) {
        const SizeType count = MathUtil::Min(3, m_indices.Size() - i);

        Vertex v[3];
        Vector2 uv[3];

        for (UInt32 j = 0; j < count; j++) {
            v[j] = m_vertices[m_indices[i + j]];
            uv[j] = v[j].GetTexCoord0();
        }

        Index i0 = m_indices[i];
        Index i1 = m_indices[i + 1];
        Index i2 = m_indices[i + 2];
        
        const Vector3 edge1 = v[1].GetPosition() - v[0].GetPosition();
        const Vector3 edge2 = v[2].GetPosition() - v[0].GetPosition();
        const Vector2 edge1uv = uv[1] - uv[0];
        const Vector2 edge2uv = uv[2] - uv[0];
        
        const Float cp = edge1uv.x * edge2uv.y - edge1uv.y * edge2uv.x;

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

    for (SizeType i = 0; i < m_vertices.Size(); i++) {
        const auto &tangent_bitangents = data[i];

        // find average
        Vector3 average_tangent, average_bitangent;

        for (const auto &item : tangent_bitangents) {
            average_tangent += item.tangent * (1.0f / tangent_bitangents.Size());
            average_bitangent += item.bitangent * (1.0f / tangent_bitangents.Size());
        }

        average_tangent.Normalize();
        average_bitangent.Normalize();

        m_vertices[i].SetTangent(average_tangent);
        m_vertices[i].SetBitangent(average_bitangent);
    }
    
    m_mesh_attributes.vertex_attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT;
    m_mesh_attributes.vertex_attributes |= VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT;
}

void Mesh::InvertNormals()
{
    for (Vertex &vertex : m_vertices) {
        vertex.SetNormal(vertex.GetNormal() * -1.0f);
    }
}

void Mesh::CalculateAABB()
{
    BoundingBox aabb;

    for (const Vertex &vertex : m_vertices) {
        aabb.Extend(vertex.GetPosition());
    }

    m_aabb = aabb;
}

static struct MeshScriptBindings : ScriptBindingsBase
{
    MeshScriptBindings()
        : ScriptBindingsBase(TypeID::ForType<Mesh>())
    {
    }

    virtual void Generate(APIInstance &api_instance) override
    {
        api_instance.Module(Config::global_module_name)
            .Class<Handle<Mesh>>(
                "Mesh",
                {
                    API::NativeMemberDefine("__intern", BuiltinTypes::ANY, vm::Value(vm::Value::HEAP_POINTER, { .ptr = nullptr })),
                    API::NativeMemberDefine(
                        "$construct",
                        BuiltinTypes::ANY,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxFn< Handle<Mesh>, void *, ScriptCreateObject<Mesh> >
                    ),
                    API::NativeMemberDefine(
                        "GetID",
                        BuiltinTypes::UNSIGNED_INT,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxFn< UInt32, const Handle<Mesh> &, ScriptGetHandleIDValue<Mesh> >
                    ),
                    API::NativeMemberDefine(
                        "Init",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFnWrapped< void, Handle<Mesh>, Mesh, &Mesh::Init >
                    ),
                    API::NativeMemberDefine(
                        "NumIndices",
                        BuiltinTypes::UNSIGNED_INT,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFnWrapped< UInt32, Handle<Mesh>, Mesh, &Mesh::NumIndices >
                    ),
                    API::NativeMemberDefine(
                        "NumVertices",
                        BuiltinTypes::UNSIGNED_INT,
                        {
                            { "self", BuiltinTypes::ANY }
                        },
                        CxxMemberFnWrapped< UInt32, Handle<Mesh>, Mesh, &Mesh::NumVertices >
                    ),
                    API::NativeMemberDefine(
                        "SetVertices",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "vertices", BuiltinTypes::ARRAY }
                        },
                        CxxMemberFnWrapped< void, Handle<Mesh>, Mesh, const Array<Vertex> &, &Mesh::SetVertices >
                    ),
                    API::NativeMemberDefine(
                        "SetIndices",
                        BuiltinTypes::VOID_TYPE,
                        {
                            { "self", BuiltinTypes::ANY },
                            { "indices", BuiltinTypes::ARRAY }
                        },
                        CxxMemberFnWrapped< void, Handle<Mesh>, Mesh, const Array<Mesh::Index> &, &Mesh::SetIndices >
                    )
                }
            );
    }
} mesh_script_bindings = { };

} // namespace hyperion::v2
