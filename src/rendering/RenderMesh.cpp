/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderMesh.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <scene/Mesh.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

#include <unordered_map>
#include <cstring>

namespace hyperion {

using renderer::GPUBufferType;

#pragma region MeshRenderResource

MeshRenderResource::MeshRenderResource(Mesh *mesh)
    : m_mesh(mesh),
      m_num_indices(0)
{
}

MeshRenderResource::MeshRenderResource(MeshRenderResource &&other) noexcept
    : RenderResourceBase(static_cast<RenderResourceBase &&>(other)),
      m_mesh(other.m_mesh),
      m_vertex_attributes(other.m_vertex_attributes),
      m_streamed_mesh_data(std::move(other.m_streamed_mesh_data)),
      m_vbo(std::move(other.m_vbo)),
      m_ibo(std::move(other.m_ibo)),
      m_num_indices(other.m_num_indices)
{
    other.m_mesh = nullptr;
}

MeshRenderResource::~MeshRenderResource() = default;

void MeshRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_mesh != nullptr);
    
    UploadMeshData();
}

void MeshRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

    HYP_LOG(Rendering, Debug, "Destroying mesh render resource for mesh {}.", m_mesh->GetName());

    SafeRelease(std::move(m_vbo));
    SafeRelease(std::move(m_ibo));

    m_streamed_mesh_data_handle.Reset();
}

void MeshRenderResource::Update_Internal()
{
    HYP_SCOPE;

    AssertThrow(m_mesh != nullptr);
    
    UploadMeshData();
}

void MeshRenderResource::UploadMeshData()
{
    HYP_SCOPE;
    
    // upload mesh data
    Array<float> vertex_buffer;
    Array<uint32> index_buffer;
    
    if (m_streamed_mesh_data != nullptr) {
        if (!m_streamed_mesh_data_handle) {
            m_streamed_mesh_data_handle = ResourceHandle(*m_streamed_mesh_data);
        }

        const MeshData &mesh_data = m_streamed_mesh_data->GetMeshData();

        vertex_buffer = BuildVertexBuffer(m_vertex_attributes, mesh_data);
        index_buffer = mesh_data.indices;
        
        m_streamed_mesh_data_handle.Reset();
    }

    // Ensure vertex buffer is not empty
    if (vertex_buffer.Empty()) {
        vertex_buffer.Resize(1);
    }

    // Ensure indices exist and are a multiple of 3
    if (index_buffer.Empty()) {
        index_buffer.Resize(3);
    } else if (index_buffer.Size() % 3 != 0) {
        index_buffer.Resize(index_buffer.Size() + (3 - (index_buffer.Size() % 3)));
    }

    m_num_indices = index_buffer.Size();

    auto *instance = g_engine->GetGPUInstance();
    auto *device = g_engine->GetGPUDevice();

    const SizeType packed_buffer_size = vertex_buffer.ByteSize();
    const SizeType packed_indices_size = index_buffer.ByteSize();

    if (!m_vbo.IsValid() || m_vbo->Size() != packed_buffer_size) {
        SafeRelease(std::move(m_vbo));

        m_vbo = MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_VERTEX_BUFFER);
    }

    if (!m_vbo->IsCreated()) {
        HYPERION_ASSERT_RESULT(m_vbo->Create(device, packed_buffer_size));
    }

    if (!m_ibo.IsValid() || m_ibo->Size() != packed_indices_size) {
        SafeRelease(std::move(m_ibo));

        m_ibo = MakeRenderObject<GPUBuffer>(GPUBufferType::MESH_INDEX_BUFFER);
    }

    if (!m_ibo->IsCreated()) {
        HYPERION_ASSERT_RESULT(m_ibo->Create(device, packed_indices_size));
    }

    HYPERION_ASSERT_RESULT(instance->GetStagingBufferPool().Use(
        device,
        [&](renderer::StagingBufferPool::Context &holder)
        {
            renderer::SingleTimeCommands commands { device };

            auto *staging_buffer_vertices = holder.Acquire(packed_buffer_size);
            staging_buffer_vertices->Copy(device, packed_buffer_size, vertex_buffer.Data());

            auto *staging_buffer_indices = holder.Acquire(packed_indices_size);
            staging_buffer_indices->Copy(device, packed_indices_size, index_buffer.Data());

            commands.Push([&](CommandBuffer *cmd)
            {
                m_vbo->CopyFrom(cmd, staging_buffer_vertices, packed_buffer_size);

                HYPERION_RETURN_OK;
            });

            commands.Push([&](CommandBuffer *cmd)
            {
                m_ibo->CopyFrom(cmd, staging_buffer_indices, packed_indices_size);

                HYPERION_RETURN_OK;
            });

            HYPERION_BUBBLE_ERRORS(commands.Execute());

            HYPERION_RETURN_OK;
        }));
}

void MeshRenderResource::SetVertexAttributes(const VertexAttributeSet &vertex_attributes)
{
    HYP_SCOPE;

    Execute([this, vertex_attributes]()
    {
        m_vertex_attributes = vertex_attributes;

        if (IsInitialized()) {
            SetNeedsUpdate();
        }
    });
}

void MeshRenderResource::SetStreamedMeshData(const RC<StreamedMeshData> &streamed_mesh_data)
{
    HYP_SCOPE;

    Execute([this, streamed_mesh_data, streamed_mesh_data_handle = streamed_mesh_data ? ResourceHandle(*streamed_mesh_data) : ResourceHandle()]()
    {
        m_streamed_mesh_data = streamed_mesh_data;

        m_streamed_mesh_data_handle = std::move(streamed_mesh_data_handle);

        if (IsInitialized()) {
            SetNeedsUpdate();
        }
    });
}

/* Copy our values into the packed vertex buffer, and increase the index for the next possible
 * mesh attribute. This macro helps keep the code cleaner and easier to maintain. */
#define PACKED_SET_ATTR(raw_values, arg_size)                                                    \
    do {                                                                                         \
        Memory::MemCpy((void *)(raw_buffer + current_offset), (raw_values), (arg_size) * sizeof(float)); \
        current_offset += (arg_size);                                                            \
    } while (0)

Array<float> MeshRenderResource::BuildVertexBuffer(const VertexAttributeSet &vertex_attributes, const MeshData &mesh_data)
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

Array<PackedVertex> MeshRenderResource::BuildPackedVertices() const
{
    HYP_SCOPE;

    if (!m_streamed_mesh_data) {
        return { };
    }

    ResourceHandle streamed_mesh_data_handle(*m_streamed_mesh_data);

    const MeshData &mesh_data = m_streamed_mesh_data->GetMeshData();

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

Array<uint32> MeshRenderResource::BuildPackedIndices() const
{
    HYP_SCOPE;

    if (!m_streamed_mesh_data) {
        return { };
    }

    ResourceHandle streamed_mesh_data_handle(*m_streamed_mesh_data);

    const MeshData &mesh_data = m_streamed_mesh_data->GetMeshData();

    AssertThrow(mesh_data.indices.Size() % 3 == 0);

    return Array<uint32>(mesh_data.indices.Begin(), mesh_data.indices.End());
}

void MeshRenderResource::Render(CommandBuffer *cmd, uint32 num_instances, uint32 instance_index) const
{
    cmd->BindVertexBuffer(GetVertexBuffer());
    cmd->BindIndexBuffer(GetIndexBuffer());

    cmd->DrawIndexed(NumIndices(), num_instances, instance_index);
}

void MeshRenderResource::RenderIndirect(
    CommandBuffer *cmd,
    const GPUBuffer *indirect_buffer,
    uint32 buffer_offset
) const
{
    cmd->BindVertexBuffer(GetVertexBuffer());
    cmd->BindIndexBuffer(GetIndexBuffer());

    cmd->DrawIndexedIndirect(
        indirect_buffer,
        buffer_offset
    );
}

void MeshRenderResource::PopulateIndirectDrawCommand(IndirectDrawCommand &out)
{
#if HYP_VULKAN
    out.command = {
        .indexCount = NumIndices()
    };
#else
    #error Not implemented for this platform!
#endif
}

#pragma endregion MeshRenderResource

} // namespace hyperion