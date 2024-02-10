#ifndef HYPERION_V2_STREAMED_MESH_DATA_HPP
#define HYPERION_V2_STREAMED_MESH_DATA_HPP

#include <streaming/StreamedData.hpp>

#include <math/Vertex.hpp>

#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/DynArray.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

struct MeshData
{
    Array<Vertex> vertices;
    Array<uint32> indices;
};

class StreamedMeshData : public StreamedData
{
public:
    static RC<StreamedMeshData> FromMeshData(const MeshData &);
    
    StreamedMeshData();
    StreamedMeshData(RC<StreamedData> streamed_data);
    StreamedMeshData(const StreamedMeshData &other)                 = default;
    StreamedMeshData &operator=(const StreamedMeshData &other)      = default;
    StreamedMeshData(StreamedMeshData &&other) noexcept             = default;
    StreamedMeshData &operator=(StreamedMeshData &&other) noexcept  = default;
    virtual ~StreamedMeshData() override                            = default;

    const MeshData &GetMeshData() const;

    StreamedDataRef<StreamedMeshData> AcquireRef()
        { return { this }; }

    virtual bool IsNull() const override;
    virtual bool IsInMemory() const override;
    virtual void Unpage() override;
    virtual const ByteBuffer &Load() const override;

private:
    void LoadMeshData(const ByteBuffer &byte_buffer) const;

    RC<StreamedData>    m_streamed_data;
    mutable MeshData    m_mesh_data;
    mutable bool        m_mesh_data_loaded;
};

} // namespace hyperion::v2

#endif