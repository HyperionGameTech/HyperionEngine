/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/Pair.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/containers/Array.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/math/BoundingBox.hpp>
#include <core/math/Vertex.hpp>

#include <scene/BVH.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/Shared.hpp>
#include <rendering/RenderObject.hpp>

#include <asset/AssetObject.hpp>
#include <asset/AssetReference.hpp>
#include <asset/MeshAsset.hpp>

#include <cstdint>

namespace hyperion {

class BVHNode;
class RenderMesh;
class Material;

HYP_ENUM()
enum MeshFlags : uint32
{
    MF_NONE = 0x0,
    MF_VIEW_INDEPENDENT = 0x1 //!< keep GPU data around even if mesh is not used by any View
};

HYP_MAKE_ENUM_FLAGS(MeshFlags)

class MeshGpuUploadFence
{
public:
    MeshGpuUploadFence() = default;
    MeshGpuUploadFence(const MeshGpuUploadFence&) = delete;
    MeshGpuUploadFence& operator=(const MeshGpuUploadFence&) = delete;
    MeshGpuUploadFence(MeshGpuUploadFence&& other) noexcept = delete;
    MeshGpuUploadFence& operator=(MeshGpuUploadFence&& other) noexcept = delete;
    ~MeshGpuUploadFence() = default;

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

/*! \brief Represents a 3D mesh in the engine, containing vertex data, indices, and rendering attributes. */
HYP_CLASS()
class HYP_API Mesh final : public AssetObject
{
    HYP_OBJECT_BODY(Mesh);

public:
    using Index = uint32;

    static Pair<Array<Vertex>, Array<uint32>> CalculateIndices(const Array<Vertex>& vertices);

    Mesh();

    Mesh(const Handle<MeshAsset>& asset, Topology topology, const VertexAttributeSet& vertexAttributes);
    Mesh(const Handle<MeshAsset>& asset, Topology topology = TOP_TRIANGLES);
    Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology, const VertexAttributeSet& vertexAttributes);
    Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology = TOP_TRIANGLES);

    ~Mesh();

    HYP_METHOD()
    EnumFlags<MeshFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_METHOD()
    void SetFlags(EnumFlags<MeshFlags> flags);

    HYP_METHOD()
    virtual Result Rename(Name name) override;

    void SetMeshData(const MeshDesc& meshDesc, const MeshData& meshData);

    HYP_METHOD()
    uint32 NumIndices() const;

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

    HYP_METHOD(Property = "VertexAttributes", Transient)
    HYP_FORCE_INLINE VertexAttributeSet GetVertexAttributes() const
    {
        const Handle<MeshAsset>& asset = GetAsset();
        return asset ? asset->GetMeshDesc().meshAttributes.vertexAttributes : VertexAttributeSet();
    }

    HYP_FORCE_INLINE MeshAttributes GetMeshAttributes() const
    {
        const Handle<MeshAsset>& asset = GetAsset();
        return asset ? asset->GetMeshDesc().meshAttributes : MeshAttributes();
    }

    HYP_METHOD(Property = "Topology", Transient)
    HYP_FORCE_INLINE Topology GetTopology() const
    {
        return GetMeshAttributes().topology;
    }

    HYP_FORCE_INLINE const Handle<MeshAsset>& GetAsset() const
    {
        return m_meshAsset.Resolve();
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

    MeshGpuUploadFence gpuUploadFence;

private:
    void Init() override;

    /*! \internal Serialization only */
    HYP_METHOD(Property = "MeshAsset")
    const AssetReference& GetMeshAsset() const
    {
        return m_meshAsset;
    }

    /*! \internal Serialization only */
    HYP_METHOD(Property = "MeshAsset")
    void SetMeshAsset(const AssetReference& assetReference);

    HYP_FIELD(Property = "AABB")
    mutable BoundingBox m_aabb;

    HYP_FIELD(Transient)
    BVHNode m_bvh; /// \todo : Move to MeshAsset to serialize there, serialization on Mesh is creating too large files.

    HYP_FIELD()
    EnumFlags<MeshFlags> m_flags;

    TAssetReference<MeshAsset> m_meshAsset;

    GpuBufferRef m_vertexBuffer;
    GpuBufferRef m_indexBuffer;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace hyperion
