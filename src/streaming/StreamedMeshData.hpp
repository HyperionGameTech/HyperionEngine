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
    Array<Vertex> vertices;

    HYP_FIELD()
    Array<uint32> indices;
    MeshData() = default;

    MeshData(Array<Vertex>&& vertices, Array<uint32>&& indices)
        : vertices(std::move(vertices)),
          indices(std::move(indices))
    {
        // DEBUGGING
        AssertThrowMsg(this->vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

    MeshData(const Array<Vertex>& vertices, const Array<uint32>& indices)
        : vertices(vertices),
          indices(indices)
    {
        // DEBUGGING
        AssertThrowMsg(this->vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

    MeshData(const MeshData& other)
        : vertices(other.vertices),
          indices(other.indices)
    {
        // DEBUGGING
        AssertThrowMsg(vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

    MeshData(MeshData&& other) noexcept
        : vertices(std::move(other.vertices)),
          indices(std::move(other.indices))
    {
        // DEBUGGING
        AssertThrowMsg(this->vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

    MeshData& operator=(const MeshData& other)
    {
        if (this != &other)
        {
            vertices = other.vertices;
            indices = other.indices;
        }
        // DEBUGGING
        AssertThrowMsg(vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");

        return *this;
    }

    MeshData& operator=(MeshData&& other) noexcept
    {
        if (this != &other)
        {
            vertices = std::move(other.vertices);
            indices = std::move(other.indices);
        }
        // DEBUGGING
        AssertThrowMsg(vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");

        return *this;
    }

    ~MeshData()
    {
        // DEBUGGING
        AssertThrowMsg(vertices.GetAllocation().magic == 0xBADA55u, "MeshData vertices allocation is not valid");
    }

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
class HYP_API StreamedMeshData final : public StreamedDataBase
{
    HYP_OBJECT_BODY(StreamedMeshData);

    StreamedMeshData(StreamedDataState initial_state, MeshData&& mesh_data, ResourceHandle& out_resource_handle);

public:
    StreamedMeshData();
    StreamedMeshData(const MeshData& mesh_data, ResourceHandle& out_resource_handle);
    StreamedMeshData(MeshData&& mesh_data, ResourceHandle& out_resource_handle);

    StreamedMeshData(const StreamedMeshData& other) = delete;
    StreamedMeshData& operator=(const StreamedMeshData& other) = delete;
    StreamedMeshData(StreamedMeshData&& other) noexcept = delete;
    StreamedMeshData& operator=(StreamedMeshData&& other) noexcept = delete;
    virtual ~StreamedMeshData() override;

    const MeshData& GetMeshData() const;

    HYP_FORCE_INLINE SizeType NumVertices() const
    {
        return m_num_vertices;
    }

    HYP_FORCE_INLINE SizeType NumIndices() const
    {
        return m_num_indices;
    }

    virtual HashCode GetDataHashCode() const override
    {
        return m_streamed_data ? m_streamed_data->GetDataHashCode() : HashCode(0);
    }

protected:
    virtual bool IsInMemory_Internal() const override;

    virtual void Load_Internal() const override;
    virtual void Unpage_Internal() override;

private:
    void LoadMeshData(const ByteBuffer& byte_buffer) const;

    RC<StreamedDataBase> m_streamed_data;

    SizeType m_num_vertices;
    SizeType m_num_indices;

    mutable Optional<MeshData> m_mesh_data;

    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

} // namespace hyperion

#endif