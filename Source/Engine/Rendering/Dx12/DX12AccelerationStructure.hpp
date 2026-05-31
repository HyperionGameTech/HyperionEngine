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

HYP_CLASS(NoScriptBindings)
class DX12AccelerationGeometry final : public ObjectBase
{
    HYP_OBJECT_BODY(DX12AccelerationGeometry);

    friend class DX12GpuTlas;
    friend class DX12GpuBlas;

public:
    static Pool* GetAllocator() { return g_rhiPool; }

    DX12AccelerationGeometry(
        const DX12GpuBufferRef& packedVerticesBuffer,
        const DX12GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<MaterialInstance>& material);

    ~DX12AccelerationGeometry() override;

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

    HYP_FORCE_INLINE const Handle<MaterialInstance>& GetMaterial() const
    {
        return m_material;
    }

    bool IsCreated() const;

    RendererResult Create();

private:
    bool m_isCreated;

    DX12GpuBufferRef m_packedVerticesBuffer;
    DX12GpuBufferRef m_packedIndicesBuffer;

    uint32 m_numVertices;
    uint32 m_numIndices;

    Handle<MaterialInstance> m_material;
};

using DX12AccelerationGeometryRef = Handle<DX12AccelerationGeometry>;

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

    HYP_FORCE_INLINE const Array<DX12AccelerationGeometryRef, RenderAllocator>& GetGeometries() const
    {
        return m_geometries;
    }

    HYP_FORCE_INLINE void AddGeometry(const DX12AccelerationGeometryRef& geometry)
    {
        if (!geometry || m_geometries.Contains(geometry))
        {
            return;
        }

        m_geometries.PushBack(geometry);
        SetNeedsRebuildFlag();
    }

    void RemoveGeometry(uint32 index);
    void RemoveGeometry(const DX12AccelerationGeometryRef& geometry);

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
#if HYP_DEBUG_MODE
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
    Array<DX12AccelerationGeometryRef, RenderAllocator> m_geometries;
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
        const Handle<MaterialInstance>& material,
        const Mat4f& transform);
    ~DX12GpuBlas() override;

    bool IsCreated() const override;

    RendererResult Create() override;

    void SetTransform(const Mat4f& transform) override;

    void SetMaterialBinding(uint32 materialBinding) override;

    RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags);

#ifdef HYP_DEBUG_MODE
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

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    static constexpr uint32 StorageIdDirtyBit = 0x80000000u;

    RendererResult Rebuild(RTUpdateStateFlags& outUpdateStateFlags);

    RendererResult BuildInstancesBuffer();
    RendererResult BuildInstancesBuffer(uint32 first, uint32 last);

    RendererResult BuildMeshDescriptionsBuffer();
    RendererResult BuildMeshDescriptionsBuffer(uint32 first, uint32 last);

    Array<DX12GpuBlas*, RenderAllocator> m_blases;
    Array<uint64, RenderAllocator> m_keys;
    TMap<uint64, Pair<DX12GpuBlas*, uint32>, RenderAllocator> m_keyToBlasAndStorageId;

    DX12GpuBufferRef m_instancesBuffer;
};

} // namespace Hyperion
