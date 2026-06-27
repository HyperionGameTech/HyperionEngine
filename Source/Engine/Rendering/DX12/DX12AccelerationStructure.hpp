/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/AccelerationStructure.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Core/Math/Mat4f.hpp>

#include <Rendering/DX12/DX12GpuBuffer.hpp>

namespace Hyperion {

class DX12AccelerationGeometry final
{
    friend class DX12GpuTlas;
    friend class DX12GpuBlas;

public:
    DX12AccelerationGeometry(
        const DX12GpuBufferRef& packedVerticesBuffer,
        const DX12GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material);

    DX12AccelerationGeometry(DX12AccelerationGeometry&& other) noexcept = default;
    DX12AccelerationGeometry& operator=(DX12AccelerationGeometry&& other) noexcept = default;

    ~DX12AccelerationGeometry();

    HYP_FORCE_INLINE const DX12GpuBufferRef& GetPackedVerticesBuffer() const
    {
        return m_packedVerticesBuffer;
    }

    HYP_FORCE_INLINE const DX12GpuBufferRef& GetPackedIndicesBuffer() const
    {
        return m_packedIndicesBuffer;
    }

    HYP_FORCE_INLINE uint32 NumVertices() const
    {
        return m_numVertices;
    }

    HYP_FORCE_INLINE uint32 NumIndices() const
    {
        return m_numIndices;
    }

    HYP_FORCE_INLINE const Handle<Material>& GetMaterial() const
    {
        return m_material;
    }

    bool IsCreated() const;

    RendererResult Create();

private:
    Handle<Material> m_material;

    DX12GpuBufferRef m_packedVerticesBuffer;
    DX12GpuBufferRef m_packedIndicesBuffer;

    uint32 m_numVertices;
    uint32 m_numIndices;

    bool m_isCreated;
};

class DX12AccelerationStructureBase
{
protected:
    DX12AccelerationStructureBase(const Mat4f& transform = Mat4f::Identity());
    ~DX12AccelerationStructureBase();

public:
    HYP_FORCE_INLINE const DX12GpuBufferRef& GetBuffer() const
    {
        return m_buffer;
    }

    HYP_FORCE_INLINE AccelerationStructureFlags GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE void SetFlag(AccelerationStructureFlagBits flag)
    {
        m_flags = AccelerationStructureFlags(m_flags | flag);
    }

    HYP_FORCE_INLINE void ClearFlag(AccelerationStructureFlagBits flag)
    {
        m_flags = AccelerationStructureFlags(m_flags & ~flag);
    }

    HYP_FORCE_INLINE const TList<DX12AccelerationGeometry, DX12Allocator>& GetGeometries() const
    {
        return m_geometries;
    }

    HYP_FORCE_INLINE const Mat4f& GetTransform() const
    {
        return m_transform;
    }

    HYP_FORCE_INLINE void SetTransform(const Mat4f& transform)
    {
        if (m_transform == transform)
        {
            return;
        }

        m_transform = transform;
        SetTransformUpdateFlag();
    }

protected:
#if HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name);
#endif

    HYP_FORCE_INLINE void SetTransformUpdateFlag()
    {
        SetFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);
    }

    HYP_FORCE_INLINE void SetNeedsRebuildFlag()
    {
        SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    }

    DX12GpuBufferRef m_buffer;
    DX12GpuBufferRef m_scratchBuffer;
    TList<DX12AccelerationGeometry, DX12Allocator> m_geometries;
    Mat4f m_transform;
    AccelerationStructureFlags m_flags;

    Name m_debugName;
};

using DX12AccelerationStructureRef = Handle<DX12AccelerationStructureBase>;

HYP_CLASS(NoScriptBindings)
class DX12GpuBlas final : public GpuBlasBase, public DX12AccelerationStructureBase
{
    HYP_OBJECT_BODY(DX12GpuBlas);

public:
    friend class DX12GpuTlas;

    DX12GpuBlas(
        const DX12GpuBufferRef& packedVerticesBuffer,
        const DX12GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform);
    ~DX12GpuBlas() override;

    bool IsCreated() const override;

    RendererResult Create() override;

    void SetTransform(const Mat4f& transform) override;

    void SetMaterialBinding(uint32 materialBinding) override;

    RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags);

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

private:
    RendererResult Rebuild(RTUpdateStateFlags& outUpdateStateFlags);

    DX12GpuBufferRef m_packedVerticesBuffer;
    DX12GpuBufferRef m_packedIndicesBuffer;
};

HYP_CLASS(NoScriptBindings)
class DX12GpuTlas final : public GpuTlasBase, public DX12AccelerationStructureBase
{
    HYP_OBJECT_BODY(DX12GpuTlas);

public:
    DX12GpuTlas();
    ~DX12GpuTlas() override;

    bool IsCreated() const override;

    void AddGpuBlas(uint64 key, GpuBlas* blas) override;
    void RemoveGpuBlas(uint64 key) override;
    bool HasGpuBlas(uint64 key) override;

    RendererResult Create() override;

    RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags) override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override;
#endif

private:
    static constexpr uint32 StorageIdDirtyBit = 0x80000000u;

    RendererResult Rebuild(RTUpdateStateFlags& outUpdateStateFlags);

    RendererResult BuildInstancesBuffer();
    RendererResult BuildInstancesBuffer(uint32 first, uint32 last);

    RendererResult BuildMeshDescriptionsBuffer();
    RendererResult BuildMeshDescriptionsBuffer(uint32 first, uint32 last);

    Array<DX12GpuBlas*, DX12Allocator> m_blases;
    Array<uint64, DX12Allocator> m_keys;
    TMap<uint64, Pair<DX12GpuBlas*, uint32>, DX12Allocator> m_keyToBlasAndStorageId;

    DX12GpuBufferRef m_instancesBuffer;
};

} // namespace Hyperion
