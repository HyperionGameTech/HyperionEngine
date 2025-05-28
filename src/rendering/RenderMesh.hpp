/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_MESH_HPP
#define HYPERION_RENDERING_MESH_HPP

#include <core/object/HypObject.hpp>

#include <core/utilities/Pair.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vertex.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderResource.hpp>
#include <rendering/Shader.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <cstdint>

namespace hyperion {

using renderer::IndirectDrawCommand;
using renderer::PackedVertex;
using renderer::Topology;

class Mesh;
class Material;
class RHICommandList;

class RenderMesh final : public RenderResourceBase
{
public:
    RenderMesh(Mesh* mesh);
    RenderMesh(RenderMesh&& other) noexcept;
    virtual ~RenderMesh() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GPUBufferRef& GetVertexBuffer() const
    {
        return m_vbo;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GPUBufferRef& GetIndexBuffer() const
    {
        return m_ibo;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE uint32 NumIndices() const
    {
        return m_num_indices;
    }

    void SetVertexAttributes(const VertexAttributeSet& vertex_attributes);
    void SetStreamedMeshData(const RC<StreamedMeshData>& streamed_mesh_data);

    void Render(RHICommandList& cmd, uint32 num_instances = 1, uint32 instance_index = 0) const;
    void RenderIndirect(RHICommandList& cmd, const GPUBufferRef& indirect_buffer, uint32 buffer_offset = 0) const;

    void PopulateIndirectDrawCommand(IndirectDrawCommand& out);

    // Build raytracing acceleration structure
    BLASRef BuildBLAS(const Handle<Material>& material) const;

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

private:
    Array<PackedVertex> BuildPackedVertices() const;
    Array<uint32> BuildPackedIndices() const;

    static Array<float> BuildVertexBuffer(const VertexAttributeSet& vertex_attributes, const MeshData& mesh_data);

    void UploadMeshData();

    Mesh* m_mesh;

    VertexAttributeSet m_vertex_attributes;

    RC<StreamedMeshData> m_streamed_mesh_data;
    mutable ResourceHandle m_streamed_mesh_data_handle;

    GPUBufferRef m_vbo;
    GPUBufferRef m_ibo;

    uint32 m_num_indices;
};

} // namespace hyperion

#endif // HYPERION_MESH_HPP
