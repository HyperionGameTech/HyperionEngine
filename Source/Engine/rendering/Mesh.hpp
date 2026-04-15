/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/utilities/Pair.hpp>
#include <Core/utilities/EnumFlags.hpp>

#include <Core/containers/Array.hpp>

#include <Core/threading/DataRaceDetector.hpp>
#include <Core/threading/Semaphore.hpp>

#include <Core/math/BoundingBox.hpp>

#include <Core/io/ByteReader.hpp>
#include <Core/io/ByteWriter.hpp>

#include <scene/BVH.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/Shared.hpp>
#include <rendering/Vertex.hpp>
#include <rendering/RenderObject.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetReference.hpp>

#include <cstdint>

namespace Hyperion {

class BVHNode;
class RenderMesh;
class Material;

HYP_ENUM()
enum class MeshFlags : uint32
{
    None = 0x0,
    ViewIndependent = 0x1 //!< keep GPU data around even if mesh is not used by any View
};

HYP_MAKE_ENUM_FLAGS(MeshFlags)

class MeshGpuUploadSemaphore
{
public:
    MeshGpuUploadSemaphore() = default;
    MeshGpuUploadSemaphore(const MeshGpuUploadSemaphore&) = delete;
    MeshGpuUploadSemaphore& operator=(const MeshGpuUploadSemaphore&) = delete;
    MeshGpuUploadSemaphore(MeshGpuUploadSemaphore&& other) noexcept = delete;
    MeshGpuUploadSemaphore& operator=(MeshGpuUploadSemaphore&& other) noexcept = delete;
    ~MeshGpuUploadSemaphore() = default;

    HYP_FORCE_INLINE void Wait() const
    {
        m_semaphore.Acquire();
    }

    HYP_FORCE_INLINE void Signal()
    {
        m_semaphore.Produce();
    }

    HYP_FORCE_INLINE void Reset()
    {
        m_semaphore.SetValue(0);
    }

    HYP_FORCE_INLINE bool IsSignaled() const
    {
        return m_semaphore.IsInSignalState();
    }

private:
    Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE> m_semaphore;
};

HYP_STRUCT()
struct MeshDesc
{
    HYP_STRUCT_BODY(MeshDesc);

    HYP_FIELD(Serialize)
    MeshAttributes meshAttributes;

    HYP_FIELD(Serialize)
    uint32 numVertices = 0;

    HYP_FIELD(Serialize)
    uint32 numIndices = 0;
};

/*! \brief Represents a 3D mesh in the engine, containing vertex data, indices, and rendering attributes. */
HYP_CLASS()
class HYP_API Mesh final : public AssetObject
{
    HYP_OBJECT_BODY(Mesh);

public:
    using Index = uint32;

    Mesh();

    Mesh(const VertexArrayView& vertexData, const ByteBuffer& indexData, Topology topology, const VertexInputLayoutDesc& inputLayout);
    Mesh(const VertexArrayView& vertexData, const ByteBuffer& indexData, Topology topology = TOP_TRIANGLES);

    ~Mesh() override;

    HYP_METHOD()
    EnumFlags<MeshFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD()
    void SetFlags(EnumFlags<MeshFlags> flags);

    HYP_METHOD()
    virtual Result Rename(Name name) override;

    void SetMeshData(
        const MeshDesc& meshDesc,
        const VertexArrayView& vertices,
        Span<const ubyte> indices);

    HYP_METHOD()
    HYP_FORCE_INLINE uint32 NumIndices() const
    {
        return m_meshDesc.numIndices;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GpuBufferRef& GetVertexBuffer() const
    {
        return m_vertexBuffer;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const GpuBufferRef& GetIndexBuffer() const
    {
        return m_indexBuffer;
    }

    HYP_FORCE_INLINE MeshAttributes GetMeshAttributes() const
    {
        return m_meshDesc.meshAttributes;
    }

    /*! \brief Get the axis-aligned bounding box for the mesh. */
    HYP_METHOD(Property = "AABB", Editor = true)
    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    /*! \brief Manually set the AABB for the mesh */
    HYP_METHOD(Property = "AABB", Editor = true)
    HYP_FORCE_INLINE void SetAABB(const BoundingBox& aabb)
    {
        m_aabb = aabb;
    }

    HYP_FORCE_INLINE const BVHNode& GetBVH() const
    {
        return m_bvh;
    }

    bool BuildBVH(int maxDepth = 3);

    void UploadGpuData();
    void ReleaseGpuData();
    
    HYP_FORCE_INLINE const MeshDesc& GetMeshDesc() const
    {
        return m_meshDesc;
    }

    VertexArrayView GetVertexData() const;
    void SetVertexData(const VertexArrayView& view);

    HYP_FORCE_INLINE Span<ubyte> GetIndexData()
    {
        Assert(m_indexData.raw != nullptr, "Index data not loaded!");
        return Span<ubyte>(reinterpret_cast<ubyte*>(m_indexData.raw), m_indexData.size);
    }

    HYP_FORCE_INLINE Span<const ubyte> GetIndexData() const
    {
        return const_cast<Mesh*>(this)->GetIndexData();
    }

    void SetIndexData(Span<const ubyte> indexData);

    BoundingBox CalculateAABB() const;

    Array<float> BuildVertexBuffer(const VertexInputLayoutDesc& inputLayout) const;

    void CalculateNormals(bool weighted = false);

    bool BuildBVH(BVHNode& bvhNode, int maxDepth = 3) const;

    MeshGpuUploadSemaphore gpuUploadSemaphore;

protected:
    void PageBlobData() override;
    void UnpageBlobData() override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        outReferences.EmplaceBack("VB", 1, &m_vertexData);
        outReferences.EmplaceBack("IB", 1, &m_indexData);
    }

private:
    void Init() override;

    HYP_FIELD(Serialize)
    MeshDesc m_meshDesc;

    HYP_FIELD(Serialize)
    BlobDataReference m_vertexData;
    
    HYP_FIELD(Serialize)
    BlobDataReference m_indexData;

    HYP_FIELD(Property = "AABB")
    mutable BoundingBox m_aabb;

    HYP_FIELD(Transient)
    BVHNode m_bvh;

    HYP_FIELD()
    EnumFlags<MeshFlags> m_flags;
    
    GpuBufferRef m_vertexBuffer;
    GpuBufferRef m_indexBuffer;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace Hyperion
