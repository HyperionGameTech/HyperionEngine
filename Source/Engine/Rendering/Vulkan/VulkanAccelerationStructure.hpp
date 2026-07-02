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

#include <Rendering/Vulkan/VulkanGpuBuffer.hpp>
#include <Rendering/Shared.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Types.hpp>

#include <Vulkan/vulkan.h>

namespace Hyperion {

class Entity;

extern Pool* g_vulkanPool;

class VulkanAccelerationGeometry final
{
    friend class VulkanTopLevelAS;
    friend class VulkanBottomLevelAS;

public:
    VulkanAccelerationGeometry(
        const VulkanGpuBufferRef& packedVerticesBuffer,
        const VulkanGpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material);

    VulkanAccelerationGeometry(VulkanAccelerationGeometry&& other) noexcept
        : m_material(std::move(other.m_material)),
          m_packedVerticesBuffer(std::move(other.m_packedIndicesBuffer)),
          m_packedIndicesBuffer(std::move(other.m_packedIndicesBuffer)),
          m_numVertices(other.m_numVertices),
          m_numIndices(other.m_numIndices),
          m_geometry(other.m_geometry),
          m_isCreated(other.m_isCreated)
    {
        other.m_isCreated = false;
    }

    VulkanAccelerationGeometry& operator=(VulkanAccelerationGeometry&& other) noexcept = delete;

    ~VulkanAccelerationGeometry();

    HYP_FORCE_INLINE bool operator==(const VulkanAccelerationGeometry& other) const
    {
        return std::memcmp(&m_geometry, &other.m_geometry, sizeof(VkAccelerationStructureGeometryKHR)) == 0;
    }

    HYP_FORCE_INLINE const VulkanGpuBufferRef& GetPackedVerticesBuffer() const
    {
        return m_packedVerticesBuffer;
    }

    HYP_FORCE_INLINE const VulkanGpuBufferRef& GetPackedIndicesBuffer() const
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

    VulkanGpuBufferRef m_packedVerticesBuffer;
    VulkanGpuBufferRef m_packedIndicesBuffer;

    uint32 m_numVertices;
    uint32 m_numIndices;

    VkAccelerationStructureGeometryKHR m_geometry;

    bool m_isCreated : 1;
};

class VulkanASBase
{
protected:
    VulkanASBase(const Mat4f& transform = Mat4f::Identity());
    ~VulkanASBase();

public:
    HYP_FORCE_INLINE const VulkanGpuBufferRef& GetBuffer() const
    {
        return m_buffer;
    }

    HYP_FORCE_INLINE const VkAccelerationStructureKHR& GetVulkanHandle() const
    {
        return m_accelerationStructure;
    }

    HYP_FORCE_INLINE uint64 GetDeviceAddress() const
    {
        return m_deviceAddress;
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

    HYP_FORCE_INLINE const List<VulkanAccelerationGeometry, VulkanAllocator>& GetGeometries() const
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
            // same transforms, don't set the flag
            return;
        }

        m_transform = transform;
        SetTransformUpdateFlag();
    }

protected:
    void SetDebugName(Name name);

    HYP_FORCE_INLINE void SetTransformUpdateFlag()
    {
        SetFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);
    }

    HYP_FORCE_INLINE void SetNeedsRebuildFlag()
    {
        SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    }

    RendererResult CreateAccelerationStructure(
        AccelerationStructureType type,
        Span<const VkAccelerationStructureGeometryKHR> geometries,
        Span<const uint32> primitiveCounts,
        bool update,
        RTUpdateStateFlags& outUpdateStateFlags);

    VulkanGpuBufferRef m_buffer;
    VulkanGpuBufferRef m_scratchBuffer;

    List<VulkanAccelerationGeometry, VulkanAllocator> m_geometries;

    Mat4f m_transform;
    VkAccelerationStructureKHR m_accelerationStructure;
    uint64 m_deviceAddress;
    AccelerationStructureFlags m_flags;

    Name m_debugName;
};

using VulkanAccelerationStructureRef = Handle<VulkanASBase>;
using VulkanAccelerationStructureWeakRef = WeakHandle<VulkanASBase>;

HYP_CLASS(NoScriptBindings)
class VulkanBottomLevelAS final : public BottomLevelASBase, public VulkanASBase
{
    HYP_OBJECT_BODY(VulkanBottomLevelAS);

public:
    friend class VulkanTopLevelAS;

    VulkanBottomLevelAS(
        const VulkanGpuBufferRef& packedVerticesBuffer,
        const VulkanGpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform);
    ~VulkanBottomLevelAS() override;

    bool IsCreated() const override;

    RendererResult Create() override;

    void SetTransform(const Mat4f& transform) override
    {
        VulkanASBase::SetTransform(transform);
    }

    void SetMaterialBinding(uint32 materialBinding) override
    {
        if (m_materialBinding == materialBinding)
        {
            return;
        }

        m_materialBinding = materialBinding;

        if (!IsCreated())
        {
            return;
        }

        m_flags |= ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE;
    }

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags);

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override
    {
        VulkanASBase::SetDebugName(name);
    }
#endif

private:
    RendererResult Rebuild(RTUpdateStateFlags& outUpdateStateFlags);

    VulkanGpuBufferRef m_packedVerticesBuffer;
    VulkanGpuBufferRef m_packedIndicesBuffer;
};

HYP_CLASS(NoScriptBindings)
class VulkanTopLevelAS final : public TopLevelASBase, public VulkanASBase
{
    HYP_OBJECT_BODY(VulkanTopLevelAS);

public:
    explicit VulkanTopLevelAS(const ASResourceCallbacks& callbacks);
    ~VulkanTopLevelAS() override;

    bool IsCreated() const override;

    void AddBLAS(uint64 key, VulkanBottomLevelAS* blas) override;
    bool RemoveBLAS(uint64 key) override;
    bool ContainsBLAS(uint64 key) override;

    RendererResult Create() override;

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags) override;

#ifdef HYP_RHI_DEBUG_NAMES
    void SetDebugName(Name name) override
    {
        VulkanASBase::SetDebugName(name);
    }
#endif

private:
    static constexpr uint32 StorageIdDirtyBit = 0x80000000u;

    RendererResult Rebuild(RTUpdateStateFlags& outUpdateStateFlags);

    Array<VkAccelerationStructureGeometryKHR, VulkanTempAllocator> GetGeometries() const;
    Array<uint32, VulkanTempAllocator> GetPrimitiveCounts() const;

    RendererResult BuildInstancesBuffer();
    RendererResult BuildInstancesBuffer(uint32 first, uint32 last);

    RendererResult BuildMeshDescriptionsBuffer();
    RendererResult BuildMeshDescriptionsBuffer(uint32 first, uint32 last);

    Array<VulkanBottomLevelAS*, VulkanAllocator> m_blases;
    Array<uint64, VulkanAllocator> m_keys;

    FlatMap<uint64, Pair<VulkanBottomLevelAS*, uint32>, VulkanAllocator> m_keyToBlasAndStorageId;

    VulkanGpuBufferRef m_instancesBuffer;
};

} // namespace Hyperion
