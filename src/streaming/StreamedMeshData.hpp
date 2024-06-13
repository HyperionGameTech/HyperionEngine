/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STREAMED_MESH_DATA_HPP
#define HYPERION_STREAMED_MESH_DATA_HPP

#include <streaming/StreamedData.hpp>

#include <math/Vertex.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/containers/Array.hpp>
#include <core/utilities/Optional.hpp>

#include <Types.hpp>

namespace hyperion {

struct MeshData
{
    Array<Vertex>   vertices;
    Array<uint32>   indices;
};

class HYP_API StreamedMeshData : public StreamedData
{
public:
    static RC<StreamedMeshData> FromMeshData(MeshData);
    
    StreamedMeshData();
    StreamedMeshData(MeshData &&mesh_data);
    // StreamedMeshData(const StreamedMeshData &other)                 = default;
    // StreamedMeshData &operator=(const StreamedMeshData &other)      = default;
    // StreamedMeshData(StreamedMeshData &&other) noexcept             = default;
    // StreamedMeshData &operator=(StreamedMeshData &&other) noexcept  = default;
    virtual ~StreamedMeshData() override                            = default;

    HYP_NODISCARD
    const MeshData &GetMeshData() const;

    HYP_NODISCARD HYP_FORCE_INLINE
    SizeType NumVertices() const
        { return m_num_vertices; }

    HYP_NODISCARD HYP_FORCE_INLINE
    SizeType NumIndices() const
        { return m_num_indices; }

    HYP_NODISCARD HYP_FORCE_INLINE
    StreamedDataRef<StreamedMeshData> AcquireRef()
        { return { RefCountedPtrFromThis().CastUnsafe<StreamedMeshData>() }; }

    virtual bool IsNull() const override;
    virtual bool IsInMemory() const override;

protected:
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