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
#include <Core/Threading/AtomicVar.hpp>

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
    ViewIndependent = 0x1,  //!< keep GPU data around even if mesh is not used by any View
    DynamicMesh = 0x2       //!< the mesh data will be updated dynamically, at runtime.
};

HYP_MAKE_ENUM_FLAGS(MeshFlags);

HYP_STRUCT()
struct MeshLodDesc
{
    HYP_STRUCT_BODY(MeshLodDesc);

    HYP_FIELD(Serialize)
    uint32 numVertices = 0;

    HYP_FIELD(Serialize)
    uint32 numIndices = 0;

    ///max distance any vertex on this LOD was moved from its LOD 0 position, in local space.
    ///used to size skirts so geomorphing (see Mesh LOD selection) never opens a gap at the LOD's own error bound.
    HYP_FIELD(Serialize)
    float geometricError = 0.0f;

    ///projected size (bounding sphere diameter as a fraction of screen height) at which this LOD takes over.
    ///ignored for LOD 0, which is always the closest LOD.
    HYP_FIELD(Serialize)
    float screenSize = 0.0f;
};

HYP_STRUCT()
struct MeshLodGenerationSettings
{
    HYP_STRUCT_BODY(MeshLodGenerationSettings);

    HYP_FIELD(Property = "NumLods", Serialize)
    uint8 numLods = MaxMeshLods;

    ///target triangle count of each LOD as a fraction of LOD 0
    HYP_FIELD(Property = "TriangleRatios", Serialize)
    FixedArray<float, MaxMeshLods> triangleRatios = { 1.0f, 0.5f, 0.25f, 0.125f };

    ///upper bound on simplification error, relative to mesh size.
    HYP_FIELD(Property = "MaxRelativeError", Serialize)
    float maxRelativeError = 1.0f;

    HYP_FIELD(Property = "NormalWeight", Serialize)
    float normalWeight = 0.5f;

    HYP_FIELD(Property = "UV0Weight", Serialize)
    float uv0Weight = 1.0f;

    HYP_FIELD(Property = "UV1Weight", Serialize)
    float uv1Weight = 0.5f;

    ///keeps vertices of different branches or leaf cards of a swaying tree from being merged
    HYP_FIELD(Property = "WindWeight", Serialize)
    float windWeight = 1.0f;

    HYP_FIELD(Property = "LockBorder", Serialize)
    bool lockBorder = false;

    ///drop components that are too small to matter.
    HYP_FIELD(Property = "Prune", Serialize)
    bool prune = false;

    ///error budget used to derive default screen sizes, in pixels at 1080p.
    HYP_FIELD(Property = "MaxScreenErrorPixels", Serialize)
    float maxScreenErrorPixels = 1.0f;

    ///hash of the LOD 0 data the current LODs were generated from
    HYP_FIELD(Property = "SourceDataHash", Serialize, Editor = false)
    uint64 sourceDataHash = 0;
};

HYP_STRUCT()
struct MeshLodInfo
{
    HYP_STRUCT_BODY(MeshLodInfo);

    HYP_FIELD(Property = "NumVertices", Serialize)
    uint32 numVertices = 0;

    HYP_FIELD(Property = "NumTriangles", Serialize)
    uint32 numTriangles = 0;

    HYP_FIELD(Property = "GeometricError", Serialize)
    float geometricError = 0.0f;

    HYP_FIELD(Property = "ScreenSize", Serialize)
    float screenSize = 0.0f;
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
        uint8 numLods = 0;

        for (uint8 i = 0; i < MaxMeshLods; i++)
        {
            if (lods[i].numIndices == 0)
            {
                break;
            }

            numLods = i + 1;
        }

        return numLods;
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

HYP_CLASS(AssetBucket = "Meshes")
class ENGINE_API Mesh final : public AssetObject
{
    HYP_OBJECT_BODY(Mesh);

public:
    using Index = uint32;

    static constexpr const char* GetVertexBufferBlobMagic(uint8 lodIndex)
    {
        constexpr const char* Magics[MaxMeshLods] = { "VB", "VB1", "VB2", "VB3" };

        return Magics[lodIndex];
    }

    static constexpr const char* GetIndexBufferBlobMagic(uint8 lodIndex)
    {
        constexpr const char* Magics[MaxMeshLods] = { "IB", "IB1", "IB2", "IB3" };

        return Magics[lodIndex];
    }

    Mesh();

    Mesh(const MeshDataView& meshData, Topology topology, const VertexInputLayoutDesc& inputLayout);
    explicit Mesh(const MeshDataView& meshData, Topology topology = Topology::Triangles);

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

    /*! \brief Replace the data for a single LOD > 1 */
    void SetLodData(
        uint8 lodIndex,
        const MeshLodDesc& lodDesc,
        const VertexArrayView& vertices,
        ConstByteView indices);

    void ClearLods(uint8 firstLodIndex);

    HYP_FORCE_INLINE uint32 GetLodDataVersion() const
    {
        return m_lodDataVersion.Get(MemoryOrder::ACQUIRE);
    }

    HYP_METHOD()
    uint32 NumIndices(uint8 lodIndex) const
    {
        return m_meshDesc.lods[lodIndex].numIndices;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetVertexBuffer(uint8 lodIndex) const
    {
        AssertDebug(lodIndex < MaxMeshLods);

        return m_vertexBuffers[lodIndex];
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetIndexBuffer(uint8 lodIndex) const
    {
        AssertDebug(lodIndex < MaxMeshLods);

        return m_indexBuffers[lodIndex];
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

    HYP_FORCE_INLINE const MeshDesc& GetMeshDesc() const
    {
        return m_meshDesc;
    }

    HYP_METHOD()
    bool IsDynamicMesh() const
    {
        return (m_flags & MeshFlags::DynamicMesh);
    }
    
    HYP_METHOD()
    void SetIsDynamicMesh(bool isDynamic);

    void UploadGpuData();
    void ReleaseGpuData();

    bool UploadLod(uint8 lodIndex);

    ///Dynamic Mesh stuff

    /// Dynamically set vertices for the LOD \p lodIndex starting at \p firstVertex.
    /// Must be a Dynamic Mesh (needs DynamicMesh flag on creation)
    void UpdateDynamicVertexData(uint8 lodIndex, uint32 firstVertex, const VertexArrayView& vertexRange);
    
    /// Update BVH after finished updating dynamic vertices
    /// Must be a Dynamic Mesh (needs DynamicMesh flag on creation)
    void UpdateDynamicBVH();

    ///LOD gen

    HYP_FORCE_INLINE const MeshLodGenerationSettings& GetLodGenerationSettings() const
    {
        return m_lodGenerationSettings;
    }

    void SetLodGenerationSettings(const MeshLodGenerationSettings& settings);

    bool AreLodsOutOfDate() const;

    uint64 ComputeLod0DataHash() const;

    ///Lightmap UVs

    HYP_FORCE_INLINE uint64 GetLightmapUVDataHash() const
    {
        return m_lightmapUvDataHash;
    }

    void SetLightmapUVDataHash(uint64 lightmapUvDataHash);

    bool HasValidLightmapUVs() const;

    ////////////////////

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

    BoundingBox CalculateAABB(uint8 lodIndex = 0) const;

    template <class AllocatorType>
    void BuildVertexBuffer(
        const VertexInputLayoutDesc& inputLayout,
        uint8 lodIndex,
        Array<float, AllocatorType>& outData) const;

    void CalculateNormals(bool weighted = false);

    void BuildBVH(BVHNode& bvhNode, int maxDepth = 3, uint8 lodIndex = 0) const;

    Handle<Mesh> Clone() const;

    HYP_METHOD()
    virtual Handle<AssetObject> CloneAsset() const override;

#ifdef HYP_EDITOR
    HYP_METHOD(EditorOnly, EditorAction = "Regenerate Normals")
    void RegenerateNormals();
    
    HYP_METHOD(EditorOnly, EditorAction = "Rebuild BVH")
    void RebuildBVH();

    HYP_METHOD(EditorOnly, EditorAction = "Recalculate Bounds")
    void RecalculateBounds();

    HYP_METHOD(EditorOnly, EditorAction = "Generate LODs", EditCondition = "CanGenerateLods")
    void GenerateLods();

    HYP_METHOD(EditorOnly, EditorAction = "Clear LODs", EditCondition = "HasGeneratedLods")
    void ClearGeneratedLods();

    HYP_METHOD()
    bool CanGenerateLods() const;

    HYP_METHOD()
    bool HasGeneratedLods() const
    {
        return m_meshDesc.GetNumLods() > 1;
    }
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
                outReferences.EmplaceBack(GetVertexBufferBlobMagic(lodIndex), 1, &m_lodData[lodIndex].vertexData);
            }

            if (m_lodData[lodIndex].indexData.size != 0)
            {
                outReferences.EmplaceBack(GetIndexBufferBlobMagic(lodIndex), 1, &m_lodData[lodIndex].indexData);
            }
        }

        if (m_bvhData.size != 0)
        {
            outReferences.EmplaceBack("BVH", 1, &m_bvhData);
        }
    }

private:
    /// Frees LOD data in place; callers hold the write scope and handle the version bump / MarkDirty.
    void ClearLodData(uint8 firstLodIndex);

    HYP_FIELD(Serialize)
    MeshDesc m_meshDesc;

    HYP_FIELD(Property = "LodGeneration", Serialize, Editor = false)
    MeshLodGenerationSettings m_lodGenerationSettings;

    HYP_FIELD(Serialize)
    FixedArray<MeshLodData, MaxMeshLods> m_lodData;

    HYP_FIELD(Serialize)
    BlobDataReference m_bvhData;

    HYP_FIELD(Property = "LightmapUVDataHash", Serialize, Editor = false)
    uint64 m_lightmapUvDataHash = 0;

    HYP_FIELD(Property = "AABB")
    mutable BoundingBox m_aabb;

    HYP_FIELD(Transient)
    BVHNode m_bvh;

    HYP_FIELD()
    EnumFlags<MeshFlags> m_flags;

    AtomicVar<uint32> m_lodDataVersion;

    FixedArray<GpuBufferRef, MaxMeshLods> m_vertexBuffers;
    FixedArray<GpuBufferRef, MaxMeshLods> m_indexBuffers;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace Hyperion
