/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_MESH_HPP
#define HYPERION_MESH_HPP

#include <core/object/HypObject.hpp>

#include <core/utilities/Pair.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <math/BoundingBox.hpp>
#include <math/Vertex.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/Shader.hpp>

#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <cstdint>

namespace hyperion {

using renderer::PackedVertex;
using renderer::Topology;
using renderer::IndirectDrawCommand;

struct RENDER_COMMAND(SetStreamedMeshData);

HYP_STRUCT(Size=104)
struct MeshInstanceData
{
    static constexpr uint32 max_buffers = 8;

    HYP_FIELD(Property="NumInstances", Serialize=true)
    uint32                          num_instances = 0;

    HYP_FIELD(Property="Buffers", Serialize=true)
    Array<Array<ubyte>, 0>          buffers;

    HYP_FIELD(Property="BufferStructSizes", Serialize=true)
    FixedArray<uint32, max_buffers> buffer_struct_sizes;

    HYP_FIELD(Property="BufferStructAlignments", Serialize=true)
    FixedArray<uint32, max_buffers> buffer_struct_alignments;

    HYP_FORCE_INLINE bool operator==(const MeshInstanceData &other) const
    {
        return num_instances == other.num_instances
            && buffers == other.buffers
            && buffer_struct_sizes == other.buffer_struct_sizes
            && buffer_struct_alignments == other.buffer_struct_alignments;
    }

    HYP_FORCE_INLINE bool operator!=(const MeshInstanceData &other) const
    {
        return num_instances != other.num_instances
            || buffers != other.buffers
            || buffer_struct_sizes != other.buffer_struct_sizes
            || buffer_struct_alignments != other.buffer_struct_alignments;
    }

    HYP_FORCE_INLINE uint32 NumInstances() const
        { return num_instances; }

    template <class StructType>
    void SetBufferData(int buffer_index, StructType *ptr, SizeType count)
    {
        static_assert(IsPODType<StructType>, "Struct type must a POD type");

        AssertThrowMsg(buffer_index < max_buffers, "Buffer index %d must be less than maximum number of buffers (%u)", buffer_index, max_buffers);

        if (buffers.Size() <= buffer_index) {
            buffers.Resize(buffer_index + 1);
        }

        buffer_struct_sizes[buffer_index] = sizeof(StructType);
        buffer_struct_alignments[buffer_index] = alignof(StructType);

        buffers[buffer_index].Resize(sizeof(StructType) * count);
        Memory::MemCpy(buffers[buffer_index].Data(), ptr, sizeof(StructType) * count);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(num_instances);
        hc.Add(buffers);
        hc.Add(buffer_struct_sizes);
        hc.Add(buffer_struct_alignments);

        return hc;
    }
};

HYP_CLASS()
class HYP_API Mesh final : public BasicObject<Mesh>
{
    HYP_OBJECT_BODY(Mesh);

    HYP_PROPERTY(ID, &Mesh::GetID, { { "serialize", false } });
    
public:
    friend struct RENDER_COMMAND(SetStreamedMeshData);

    using Index = uint32;

    static Pair<Array<Vertex>, Array<uint32>> CalculateIndices(const Array<Vertex> &vertices);

    Mesh();

    Mesh(
        RC<StreamedMeshData> streamed_mesh_data,
        Topology topology,
        const VertexAttributeSet &vertex_attributes
    );

    Mesh(
        RC<StreamedMeshData> streamed_mesh_data,
        Topology topology = Topology::TRIANGLES
    );

    Mesh(
        Array<Vertex> vertices,
        Array<uint32> indices,
        Topology topology,
        const VertexAttributeSet &vertex_attributes
    );

    Mesh(
        Array<Vertex> vertices,
        Array<uint32> indices,
        Topology topology = Topology::TRIANGLES
    );

    Mesh(const Mesh &other)             = delete;
    Mesh &operator=(const Mesh &other)  = delete;

    Mesh(Mesh &&other) noexcept;
    Mesh &operator=(Mesh &&other) noexcept;

    ~Mesh();

    HYP_METHOD(Property="Name", Serialize=true, Editor=true)
    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    HYP_METHOD(Property="Name", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetName(Name name)
        { m_name = name; }

    HYP_FORCE_INLINE const GPUBufferRef &GetVertexBuffer() const
        { return m_vbo; }

    HYP_FORCE_INLINE const GPUBufferRef &GetIndexBuffer() const
        { return m_ibo; }

    void SetVertices(Array<Vertex> vertices);
    void SetVertices(Array<Vertex> vertices, Array<Index> indices);

    HYP_METHOD()
    HYP_FORCE_INLINE uint32 NumIndices() const
        { return m_indices_count; }

    HYP_METHOD(Property="StreamedMeshData")
    const RC<StreamedMeshData> &GetStreamedMeshData() const;

    HYP_METHOD(Property="StreamedMeshData")
    void SetStreamedMeshData(RC<StreamedMeshData> streamed_mesh_data);

    void SetStreamedMeshData_ThreadSafe(RC<StreamedMeshData> streamed_mesh_data);

    HYP_METHOD(Property="VertexAttributes")
    HYP_FORCE_INLINE const VertexAttributeSet &GetVertexAttributes() const
        { return m_mesh_attributes.vertex_attributes; }

    HYP_METHOD(Property="VertexAttributes")
    HYP_FORCE_INLINE void SetVertexAttributes(const VertexAttributeSet &attributes)
        { m_mesh_attributes.vertex_attributes = attributes; }

    HYP_FORCE_INLINE const MeshAttributes &GetMeshAttributes() const
        { return m_mesh_attributes; }

    HYP_FORCE_INLINE void SetMeshAttributes(const MeshAttributes &attributes)
        { m_mesh_attributes = attributes; }

    HYP_METHOD(Property="Topology")
    HYP_FORCE_INLINE Topology GetTopology() const
        { return m_mesh_attributes.topology; }

    Array<PackedVertex> BuildPackedVertices() const;
    Array<uint32> BuildPackedIndices() const;

    void CalculateNormals(bool weighted = false);
    void CalculateTangents();

    HYP_METHOD()
    void InvertNormals();

    /*! \brief Get the axis-aligned bounding box for the mesh.
        If the mesh has not been initialized, the AABB will be invalid, unless SetAABB() has been called.
        Otherwise, the AABB will be calculated from the mesh vertices. */
    HYP_METHOD(Property="AABB", Serialize=true, Editor=true)
    HYP_FORCE_INLINE const BoundingBox &GetAABB() const
        { return m_aabb; }

    /*! \brief Manually set the AABB for the mesh. If CalculateAABB is called after this, or the mesh data is changed, the
        manually set AABB will be overwritten. */
    HYP_METHOD(Property="AABB", Serialize=true, Editor=true)
    HYP_FORCE_INLINE void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; }

    void Init();

    void Render(CommandBuffer *cmd, uint32 num_instances = 1, uint32 instance_index = 0) const;

    void RenderIndirect(
        CommandBuffer *cmd,
        const GPUBuffer *indirect_buffer,
        uint32 buffer_offset = 0
    ) const;

    void PopulateIndirectDrawCommand(IndirectDrawCommand &out);

private:
    void CalculateAABB();

    static Array<float> BuildVertexBuffer(const VertexAttributeSet &vertex_attributes, const MeshData &mesh_data);

    Name                    m_name;

    GPUBufferRef            m_vbo;
    GPUBufferRef            m_ibo;

    uint32                  m_indices_count = 0;

    MeshAttributes          m_mesh_attributes;

    RC<StreamedMeshData>    m_streamed_mesh_data;

    mutable BoundingBox     m_aabb;

    DataRaceDetector        m_data_race_detector;
};

} // namespace hyperion

#endif //HYPERION_MESH_HPP
