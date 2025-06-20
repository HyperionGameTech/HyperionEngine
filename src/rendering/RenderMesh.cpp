/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderMesh.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <scene/Mesh.hpp>

#include <core/threading/Task.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

#include <unordered_map>
#include <cstring>

namespace hyperion {

using renderer::GPUBufferType;

#pragma region RenderMesh

RenderMesh::RenderMesh(Mesh* mesh)
    : m_mesh(mesh),
      m_num_indices(0)
{
}

RenderMesh::RenderMesh(RenderMesh&& other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase&&>(other)),
      m_mesh(other.m_mesh),
      m_vertex_attributes(other.m_vertex_attributes),
      m_streamed_mesh_data(std::move(other.m_streamed_mesh_data)),
      m_vbo(std::move(other.m_vbo)),
      m_ibo(std::move(other.m_ibo)),
      m_num_indices(other.m_num_indices)
{
    other.m_mesh = nullptr;
}

RenderMesh::~RenderMesh() = default;

void RenderMesh::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_mesh != nullptr);

    UploadMeshData();
}

void RenderMesh::Destroy_Internal()
{
    HYP_SCOPE;

    SafeRelease(std::move(m_vbo));
    SafeRelease(std::move(m_ibo));

    m_streamed_mesh_data_handle.Reset();
}

void RenderMesh::Update_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_mesh != nullptr);

    UploadMeshData();
}

void RenderMesh::UploadMeshData()
{
    HYP_SCOPE;

    HYP_LOG(Rendering, Debug, "Uploading mesh data: {}", m_mesh->GetID());

    // upload mesh data
    Array<float> vertex_buffer;
    Array<uint32> index_buffer;

    if (m_streamed_mesh_data != nullptr)
    {
        if (!m_streamed_mesh_data_handle)
        {
            m_streamed_mesh_data_handle = ResourceHandle(*m_streamed_mesh_data);
        }

        const MeshData& mesh_data = m_streamed_mesh_data->GetMeshData();

        vertex_buffer = BuildVertexBuffer(m_vertex_attributes, mesh_data);
        index_buffer = mesh_data.indices;

        m_streamed_mesh_data_handle.Reset();
    }

    // Ensure vertex buffer is not empty
    if (vertex_buffer.Empty())
    {
        vertex_buffer.Resize(1);
    }

    // Ensure indices exist and are a multiple of 3
    if (index_buffer.Empty())
    {
        index_buffer.Resize(3);
    }
    else if (index_buffer.Size() % 3 != 0)
    {
        index_buffer.Resize(index_buffer.Size() + (3 - (index_buffer.Size() % 3)));
    }

    m_num_indices = index_buffer.Size();

    const SizeType packed_buffer_size = vertex_buffer.ByteSize();
    const SizeType packed_indices_size = index_buffer.ByteSize();

    if (!m_vbo.IsValid() || m_vbo->Size() != packed_buffer_size)
    {
        SafeRelease(std::move(m_vbo));

        m_vbo = g_rendering_api->MakeGPUBuffer(GPUBufferType::MESH_VERTEX_BUFFER, packed_buffer_size);

#ifdef HYP_DEBUG_MODE
        m_vbo->SetDebugName(NAME_FMT("RenderMesh_VertexBuffer_{}", m_mesh->GetID().Value()));
#endif
    }

    if (!m_vbo->IsCreated())
    {
        HYPERION_ASSERT_RESULT(m_vbo->Create());
    }

    if (!m_ibo.IsValid() || m_ibo->Size() != packed_indices_size)
    {
        SafeRelease(std::move(m_ibo));

        m_ibo = g_rendering_api->MakeGPUBuffer(GPUBufferType::MESH_INDEX_BUFFER, packed_indices_size);

#ifdef HYP_DEBUG_MODE
        m_ibo->SetDebugName(NAME_FMT("RenderMesh_IndexBuffer_{}", m_mesh->GetID().Value()));
#endif
    }

    if (!m_ibo->IsCreated())
    {
        HYPERION_ASSERT_RESULT(m_ibo->Create());
    }

    GPUBufferRef staging_buffer_vertices = g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STAGING_BUFFER, packed_buffer_size);
    HYPERION_ASSERT_RESULT(staging_buffer_vertices->Create());
    staging_buffer_vertices->Copy(packed_buffer_size, vertex_buffer.Data());

    GPUBufferRef staging_buffer_indices = g_rendering_api->MakeGPUBuffer(renderer::GPUBufferType::STAGING_BUFFER, packed_indices_size);
    HYPERION_ASSERT_RESULT(staging_buffer_indices->Create());
    staging_buffer_indices->Copy(packed_indices_size, index_buffer.Data());

    renderer::SingleTimeCommands commands;

    commands.Push([&](RHICommandList& cmd)
        {
            cmd.Add<CopyBuffer>(staging_buffer_vertices, m_vbo, packed_buffer_size);
        });

    commands.Push([&](RHICommandList& cmd)
        {
            cmd.Add<CopyBuffer>(staging_buffer_indices, m_ibo, packed_indices_size);
        });

    HYPERION_ASSERT_RESULT(commands.Execute());

    staging_buffer_vertices->Destroy();
    staging_buffer_indices->Destroy();
}

void RenderMesh::SetVertexAttributes(const VertexAttributeSet& vertex_attributes)
{
    HYP_SCOPE;

    Execute([this, vertex_attributes]()
        {
            m_vertex_attributes = vertex_attributes;

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

void RenderMesh::SetStreamedMeshData(const RC<StreamedMeshData>& streamed_mesh_data)
{
    HYP_SCOPE;

    Execute([this, streamed_mesh_data, streamed_mesh_data_handle = streamed_mesh_data ? ResourceHandle(*streamed_mesh_data) : ResourceHandle()]()
        {
            m_streamed_mesh_data = streamed_mesh_data;

            m_streamed_mesh_data_handle = std::move(streamed_mesh_data_handle);

            if (IsInitialized())
            {
                SetNeedsUpdate();
            }
        });
}

/* Copy our values into the packed vertex buffer, and increase the index for the next possible
 * mesh attribute. This macro helps keep the code cleaner and easier to maintain. */
#define PACKED_SET_ATTR(raw_values, arg_size)                                                           \
    do                                                                                                  \
    {                                                                                                   \
        Memory::MemCpy((void*)(raw_buffer + current_offset), (raw_values), (arg_size) * sizeof(float)); \
        current_offset += (arg_size);                                                                   \
    }                                                                                                   \
    while (0)

Array<float> RenderMesh::BuildVertexBuffer(const VertexAttributeSet& vertex_attributes, const MeshData& mesh_data)
{
    const SizeType vertex_size = vertex_attributes.CalculateVertexSize();

    Array<float> packed_buffer;
    packed_buffer.Resize(vertex_size * mesh_data.vertices.Size());

    /* Raw buffer that is used with our helper macro. */
    float* raw_buffer = packed_buffer.Data();
    SizeType current_offset = 0;

    for (SizeType i = 0; i < mesh_data.vertices.Size(); i++)
    {
        const Vertex& vertex = mesh_data.vertices[i];
        /* Offset aligned to the current vertex */
        // current_offset = i * vertex_size;

        /* Position and normals */
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION)
            PACKED_SET_ATTR(vertex.GetPosition().values, 3);
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_NORMAL)
            PACKED_SET_ATTR(vertex.GetNormal().values, 3);
        /* Texture coordinates */
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD0)
            PACKED_SET_ATTR(vertex.GetTexCoord0().values, 2);
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_TEXCOORD1)
            PACKED_SET_ATTR(vertex.GetTexCoord1().values, 2);
        /* Tangents and Bitangents */
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_TANGENT)
            PACKED_SET_ATTR(vertex.GetTangent().values, 3);
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_BITANGENT)
            PACKED_SET_ATTR(vertex.GetBitangent().values, 3);

        /* TODO: modify GetBoneIndex/GetBoneWeight to return a Vector4. */
        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_WEIGHTS)
        {
            float weights[4] = {
                vertex.GetBoneWeight(0), vertex.GetBoneWeight(1),
                vertex.GetBoneWeight(2), vertex.GetBoneWeight(3)
            };
            PACKED_SET_ATTR(weights, std::size(weights));
        }

        if (vertex_attributes & VertexAttribute::MESH_INPUT_ATTRIBUTE_BONE_INDICES)
        {
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

Array<PackedVertex> RenderMesh::BuildPackedVertices() const
{
    HYP_SCOPE;

    if (!m_streamed_mesh_data)
    {
        return {};
    }

    ResourceHandle streamed_mesh_data_handle(*m_streamed_mesh_data);

    const MeshData& mesh_data = m_streamed_mesh_data->GetMeshData();

    Array<PackedVertex> packed_vertices;
    packed_vertices.Resize(mesh_data.vertices.Size());

    for (SizeType i = 0; i < mesh_data.vertices.Size(); i++)
    {
        const auto& vertex = mesh_data.vertices[i];

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

Array<uint32> RenderMesh::BuildPackedIndices() const
{
    HYP_SCOPE;

    if (!m_streamed_mesh_data)
    {
        return {};
    }

    ResourceHandle streamed_mesh_data_handle(*m_streamed_mesh_data);

    const MeshData& mesh_data = m_streamed_mesh_data->GetMeshData();

    AssertThrow(mesh_data.indices.Size() % 3 == 0);

    return Array<uint32>(mesh_data.indices.Begin(), mesh_data.indices.End());
}

BLASRef RenderMesh::BuildBLAS(const Handle<Material>& material) const
{
    Array<PackedVertex> packed_vertices = BuildPackedVertices();
    Array<uint32> packed_indices = BuildPackedIndices();

    if (packed_vertices.Empty() || packed_indices.Empty())
    {
        return nullptr;
    }

    // some assertions to prevent gpu faults down the line
    for (SizeType i = 0; i < packed_indices.Size(); i++)
    {
        AssertThrow(packed_indices[i] < packed_vertices.Size());
    }

    struct RENDER_COMMAND(BuildBLAS)
        : public renderer::RenderCommand
    {
        Task<BLASRef>* task;
        Array<PackedVertex> packed_vertices;
        Array<uint32> packed_indices;
        Handle<Material> material;

        RENDER_COMMAND(BuildBLAS)(
            Task<BLASRef>* task,
            Array<PackedVertex>&& packed_vertices,
            Array<uint32>&& packed_indices,
            const Handle<Material>& material)
            : task(task),
              packed_vertices(std::move(packed_vertices)),
              packed_indices(std::move(packed_indices)),
              material(material)
        {
            AssertThrow(task != nullptr);
        }

        virtual ~RENDER_COMMAND(BuildBLAS)() override = default;

        virtual RendererResult operator()() override
        {
            const SizeType packed_vertices_size = packed_vertices.Size() * sizeof(PackedVertex);
            const SizeType packed_indices_size = packed_indices.Size() * sizeof(uint32);

            GPUBufferRef vertices_staging_buffer;
            GPUBufferRef indices_staging_buffer;

            GPUBufferRef packed_vertices_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::RT_MESH_VERTEX_BUFFER, packed_vertices_size);
            GPUBufferRef packed_indices_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::RT_MESH_INDEX_BUFFER, packed_indices_size);

            BLASRef geometry = g_rendering_api->MakeBLAS(
                packed_vertices_buffer,
                packed_indices_buffer,
                material,
                Matrix4::identity);

            HYP_DEFER({
                if (task)
                {
                    task->Fulfill(geometry);
                }

                SafeRelease(std::move(packed_vertices_buffer));
                SafeRelease(std::move(packed_indices_buffer));
                SafeRelease(std::move(vertices_staging_buffer));
                SafeRelease(std::move(indices_staging_buffer));
            });

            HYPERION_BUBBLE_ERRORS(packed_vertices_buffer->Create());
            HYPERION_BUBBLE_ERRORS(packed_indices_buffer->Create());

            vertices_staging_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STAGING_BUFFER, packed_vertices_size);
            HYPERION_BUBBLE_ERRORS(vertices_staging_buffer->Create());
            vertices_staging_buffer->Memset(packed_vertices_size, 0); // zero out
            vertices_staging_buffer->Copy(packed_vertices.Size() * sizeof(PackedVertex), packed_vertices.Data());

            indices_staging_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::STAGING_BUFFER, packed_indices_size);
            HYPERION_BUBBLE_ERRORS(indices_staging_buffer->Create());
            indices_staging_buffer->Memset(packed_indices_size, 0); // zero out
            indices_staging_buffer->Copy(packed_indices.Size() * sizeof(uint32), packed_indices.Data());

            renderer::SingleTimeCommands commands;

            commands.Push([&](RHICommandList& cmd)
                {
                    cmd.Add<CopyBuffer>(vertices_staging_buffer, packed_vertices_buffer, packed_vertices_size);
                    cmd.Add<CopyBuffer>(indices_staging_buffer, packed_indices_buffer, packed_indices_size);
                });

            commands.Push([&](RHICommandList&)
                {
                    geometry->Create();
                });

            return commands.Execute();
        }
    };

    Task<BLASRef> task;

    PUSH_RENDER_COMMAND(BuildBLAS, &task, std::move(packed_vertices), std::move(packed_indices), material);

    return std::move(task).Await();
}

void RenderMesh::Render(RHICommandList& cmd, uint32 num_instances, uint32 instance_index) const
{
    cmd.Add<BindVertexBuffer>(GetVertexBuffer());
    cmd.Add<BindIndexBuffer>(GetIndexBuffer());
    cmd.Add<DrawIndexed>(NumIndices(), num_instances, instance_index);
}

void RenderMesh::RenderIndirect(RHICommandList& cmd, const GPUBufferRef& indirect_buffer, uint32 buffer_offset) const
{
    cmd.Add<BindVertexBuffer>(GetVertexBuffer());
    cmd.Add<BindIndexBuffer>(GetIndexBuffer());
    cmd.Add<DrawIndexedIndirect>(indirect_buffer, buffer_offset);
}

void RenderMesh::PopulateIndirectDrawCommand(IndirectDrawCommand& out)
{
#if HYP_VULKAN
    out.command = {
        .indexCount = NumIndices()
    };
#else
#error Not implemented for this platform!
#endif
}

#pragma endregion RenderMesh

} // namespace hyperion