/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMED_MESH_DATA_HPP
#define HYPERION_STREAMED_MESH_DATA_HPP

#include <streaming/StreamedData.hpp>

#include <core/math/Vertex.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/Array.hpp>
#include <core/utilities/Optional.hpp>

#include <core/object/HypObject.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_STRUCT()
struct MeshData
{
    HYP_FIELD()
    Array<Vertex>   vertices;

    HYP_FIELD()
    Array<uint32>   indices;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        // @FIXME: Find a better way to hash it without needing to hash the entire mesh data

        HashCode hc;
        hc.Add(vertices.GetHashCode());
        hc.Add(indices.GetHashCode());

        return hc;
    }
};

HYP_CLASS()
class HYP_API StreamedMeshData final : public StreamedData
{
    HYP_OBJECT_BODY(StreamedMeshData);

    StreamedMeshData(StreamedDataState initial_state, MeshData mesh_data);

public:
    static RC<StreamedMeshData> FromMeshData(MeshData);

    StreamedMeshData();
    StreamedMeshData(const MeshData &mesh_data);
    StreamedMeshData(MeshData &&mesh_data);
    StreamedMeshData(const StreamedMeshData &other)                 = delete;
    StreamedMeshData &operator=(const StreamedMeshData &other)      = delete;
    StreamedMeshData(StreamedMeshData &&other) noexcept             = delete;
    StreamedMeshData &operator=(StreamedMeshData &&other) noexcept  = delete;
    virtual ~StreamedMeshData() override;

    const MeshData &GetMeshData() const;

    HYP_FORCE_INLINE SizeType NumVertices() const
        { return m_num_vertices; }

    HYP_FORCE_INLINE SizeType NumIndices() const
        { return m_num_indices; }

protected:
    virtual bool IsInMemory_Internal() const override;
    
    virtual const ByteBuffer &Load_Internal() const override;
    virtual void Unpage_Internal() override;
    
private:
    void LoadMeshData(const ByteBuffer &byte_buffer) const;
    
    RC<StreamedData>            m_streamed_data;

    SizeType                    m_num_vertices;
    SizeType                    m_num_indices;

    mutable Optional<MeshData>  m_mesh_data;
};

} // namespace hyperion

#endif