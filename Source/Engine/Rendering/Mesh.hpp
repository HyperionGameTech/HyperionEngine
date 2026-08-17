/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Utilities/Pair.hpp>
#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Threading/DataRaceDetector.hpp>
#include <Core/Threading/AtomicFlag.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Scene/BVH.hpp>

#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/Shared.hpp>
#include <Rendering/Vertex.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Asset/AssetObject.hpp>
#include <Asset/AssetReference.hpp>

#include <cstdint>

namespace Hyperion {

class BVHNode;
class RenderMesh;

HYP_ENUM()
enum class MeshFlags : uint32
{
    None = 0x0,
    ViewIndependent = 0x1 //!< keep GPU data around even if mesh is not used by any View
};

HYP_MAKE_ENUM_FLAGS(MeshFlags);

static constexpr uint8 MaxMeshLods = 4;

HYP_STRUCT()
struct MeshLodDesc
{
    HYP_STRUCT_BODY(MeshLodDesc);

    HYP_FIELD(Serialize)
    uint32 numVertices = 0;

    HYP_FIELD(Serialize)
    uint32 numIndices = 0;
};

HYP_STRUCT()
struct MeshDesc
{
    HYP_STRUCT_BODY(MeshDesc);

    HYP_FIELD(Serialize)
    MeshAttributes meshAttributes;

    HYP_FIELD(Serialize)
    FixedArray<MeshLodDesc, MaxMeshLods> lods = {};

    HYP_FORCE_INLINE uint8 GetNumLods() const
    {
        for (uint8 i = 0; i < MaxMeshLods; i++)
        {
            if (lods[i].numIndices == 0)
            {
                return i + 1;
            }
        }

        return MaxMeshLods;
    }
};

HYP_STRUCT()
struct MeshLodData
{
    HYP_STRUCT_BODY(MeshLodData);

    HYP_FIELD(Serialize)
    BlobDataReference vertexData;

    HYP_FIELD(Serialize)
    BlobDataReference indexData;
};

struct MeshDataView
{
    VertexArrayView vertices[MaxMeshLods];
    ConstByteView indices[MaxMeshLods];
};

/*! \brief Contains vertices and indices for all associated levels-of-detail of a section of a model */
HYP_CLASS(AssetBucket = "Meshes")
class ENGINE_API Mesh final : public AssetObject
{
    HYP_OBJECT_BODY(Mesh);

public:
    using Index = uint32;

    Mesh();

    Mesh(const MeshDataView& meshData, Topology topology, const VertexInputLayoutDesc& inputLayout);
    explicit Mesh(const MeshDataView& meshData, Topology topology = TOP_TRIANGLES);

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
        const MeshDataView& meshData);

    HYP_METHOD()
    uint32 NumIndices(uint8 lodIndex) const
    {
        return m_meshDesc.lods[lodIndex].numIndices;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetVertexBuffer(uint8 lodIndex = UINT8_MAX) const
    {
        return m_vertexBuffers[lodIndex < MaxMeshLods ? lodIndex : m_currentLodIndex];
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetIndexBuffer(uint8 lodIndex = UINT8_MAX) const
    {
        return m_indexBuffers[lodIndex < MaxMeshLods ? lodIndex : m_currentLodIndex];
    }

    HYP_FORCE_INLINE MeshAttributes GetMeshAttributes() const
    {
        return m_meshDesc.meshAttributes;
    }

    /*! \brief Get the axis-aligned bounding box for the mesh. */
    HYP_METHOD(Property = "AABB", Editor = true)
    const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    /*! \brief Manually set the AABB for the mesh */
    HYP_METHOD(Property = "AABB", Editor = true)
    void SetAABB(const BoundingBox& aabb)
    {
        m_aabb = aabb;
    }

    HYP_FORCE_INLINE const BVHNode& GetBVH() const
    {
        return m_bvh;
    }

    void SetBVH(BVHNode&& bvh);

    HYP_FORCE_INLINE const BlobDataReference& GetBVHDataReference() const
    {
        return m_bvhData;
    }

    void UploadGpuData();
    void ReleaseGpuData();

    HYP_FORCE_INLINE const MeshDesc& GetMeshDesc() const
    {
        return m_meshDesc;
    }

    VertexArrayView GetVertexData(uint8 lodIndex) const;
    void SetVertexData(uint8 lodIndex, const VertexArrayView& view);

    HYP_FORCE_INLINE Span<ubyte> GetIndexData(uint8 lodIndex)
    {
        // Data may be missing; PageBlobData() already logged why.
        if (!m_lodData[lodIndex].indexData.raw)
        {
            return Span<ubyte>();
        }

        return Span<ubyte>(reinterpret_cast<ubyte*>(m_lodData[lodIndex].indexData.raw), m_lodData[lodIndex].indexData.size);
    }

    HYP_FORCE_INLINE Span<const ubyte> GetIndexData(uint8 lodIndex) const
    {
        return const_cast<Mesh*>(this)->GetIndexData(lodIndex);
    }

    void SetIndexData(uint8 lodIndex, Span<const ubyte> indexData);

    BoundingBox CalculateAABB() const;

    template <class AllocatorType>
    void BuildVertexBuffer(
        const VertexInputLayoutDesc& inputLayout,
        uint8 lodIndex,
        Array<float, AllocatorType>& outData) const;

    void CalculateNormals(bool weighted = false);

    void BuildBVH(BVHNode& bvhNode, int maxDepth = 3) const;

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly, EditorAction = "Regenerate Normals")
    void RegenerateNormals();
    
    HYP_METHOD(EditorOnly, EditorAction = "Rebuild BVH")
    void RebuildBVH();

    HYP_METHOD(EditorOnly, EditorAction = "Recalculate Bounds")
    void RecalculateBounds();
#endif // HYP_EDITOR

    AtomicFlag isUploaded;

protected:
    void PageBlobData() override;
    void UnpageBlobData() override;

    void CollectBlobDataReferences(Array<Tuple<const char*, uint16, BlobDataReference*>>& outReferences) override
    {
        for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
        {
            if (m_lodData[lodIndex].vertexData.size != 0)
            {
                outReferences.EmplaceBack("VB", 1, &m_lodData[lodIndex].vertexData);
            }

            if (m_lodData[lodIndex].indexData.size != 0)
            {
                outReferences.EmplaceBack("IB", 1, &m_lodData[lodIndex].indexData);
            }
        }

        if (m_bvhData.size != 0)
        {
            outReferences.EmplaceBack("BVH", 1, &m_bvhData);
        }
    }

private:
    HYP_FIELD(Serialize)
    MeshDesc m_meshDesc;

    HYP_FIELD(Serialize)
    FixedArray<MeshLodData, MaxMeshLods> m_lodData;

    HYP_FIELD(Serialize)
    BlobDataReference m_bvhData;

    HYP_FIELD(Property = "AABB")
    mutable BoundingBox m_aabb;

    HYP_FIELD(Transient)
    BVHNode m_bvh;

    HYP_FIELD()
    EnumFlags<MeshFlags> m_flags;

    FixedArray<GpuBufferRef, MaxMeshLods> m_vertexBuffers;
    FixedArray<GpuBufferRef, MaxMeshLods> m_indexBuffers;

    uint8 m_currentLodIndex;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace Hyperion
