/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/raytracing/RenderAccelerationStructure.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/Shared.hpp>

#include <core/math/Mat4f.hpp>

#include <core/containers/Array.hpp>

#include <core/utilities/Span.hpp>

#include <core/reflection/Handle.hpp>

#include <core/Types.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

class Entity;
class Material;

HYP_CLASS(NoScriptBindings)
class HYP_API VulkanAccelerationGeometry final : public ObjectBase
{
    HYP_OBJECT_BODY(VulkanAccelerationGeometry);

public:
    friend class VulkanGpuTlas;
    friend class VulkanGpuBlas;

    VulkanAccelerationGeometry(
        const VulkanGpuBufferRef& packedVerticesBuffer,
        const VulkanGpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material);

    virtual ~VulkanAccelerationGeometry() override;

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
    bool m_isCreated;

    VulkanGpuBufferRef m_packedVerticesBuffer;
    VulkanGpuBufferRef m_packedIndicesBuffer;

    uint32 m_numVertices;
    uint32 m_numIndices;

    Handle<Material> m_material;

    VkAccelerationStructureGeometryKHR m_geometry;
};

using VulkanAccelerationGeometryRef = Handle<VulkanAccelerationGeometry>;
using VulkanAccelerationGeometryWeakRef = WeakHandle<VulkanAccelerationGeometry>;

class HYP_API VulkanAccelerationStructureBase
{
protected:
    VulkanAccelerationStructureBase(const Mat4f& transform = Mat4f::Identity());
    ~VulkanAccelerationStructureBase();

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

    HYP_FORCE_INLINE const Array<VulkanAccelerationGeometryRef>& GetGeometries() const
    {
        return m_geometries;
    }

    HYP_FORCE_INLINE void AddGeometry(const VulkanAccelerationGeometryRef& geometry)
    {
        if (!geometry || m_geometries.Contains(geometry))
        {
            return;
        }

        m_geometries.PushBack(geometry);
        SetNeedsRebuildFlag();
    }

    void RemoveGeometry(uint32 index);

    /*! \brief Remove the geometry from the internal list of Nodes and set a flag that the
     * structure needs to be rebuilt. Will not automatically rebuild.
     */
    void RemoveGeometry(const VulkanAccelerationGeometryRef& geometry);

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
    Array<VulkanAccelerationGeometryRef> m_geometries;
    Mat4f m_transform;
    VkAccelerationStructureKHR m_accelerationStructure;
    uint64 m_deviceAddress;
    AccelerationStructureFlags m_flags;

    Name m_debugName;
};

using VulkanAccelerationStructureRef = Handle<VulkanAccelerationStructureBase>;
using VulkanAccelerationStructureWeakRef = WeakHandle<VulkanAccelerationStructureBase>;

HYP_CLASS(NoScriptBindings)
class VulkanGpuBlas final : public GpuBlasBase, public VulkanAccelerationStructureBase
{
    HYP_OBJECT_BODY(VulkanGpuBlas);

public:
    friend class VulkanGpuTlas;

    VulkanGpuBlas(
        const VulkanGpuBufferRef& packedVerticesBuffer,
        const VulkanGpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Mat4f& transform);
    virtual ~VulkanGpuBlas() override;

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

    virtual void SetTransform(const Mat4f& transform) override
    {
        VulkanAccelerationStructureBase::SetTransform(transform);
    }

    virtual void SetMaterialBinding(uint32 materialBinding) override
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

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override
    {
        VulkanAccelerationStructureBase::SetDebugName(name);
    }
#endif

private:
    RendererResult Rebuild(RTUpdateStateFlags& outUpdateStateFlags);

    VulkanGpuBufferRef m_packedVerticesBuffer;
    VulkanGpuBufferRef m_packedIndicesBuffer;
};

HYP_CLASS(NoScriptBindings)
class VulkanGpuTlas final : public GpuTlasBase, public VulkanAccelerationStructureBase
{
    HYP_OBJECT_BODY(VulkanGpuTlas);

public:
    VulkanGpuTlas();
    virtual ~VulkanGpuTlas() override;

    virtual bool IsCreated() const override;

    virtual void AddGpuBlas(const VulkanGpuBlasRef& blas) override;
    virtual void RemoveGpuBlas(const VulkanGpuBlasRef& blas) override;
    virtual bool HasGpuBlas(const VulkanGpuBlasRef& blas) override;

    virtual RendererResult Create() override;

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    virtual RendererResult UpdateStructure(RTUpdateStateFlags& outUpdateStateFlags) override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override
    {
        VulkanAccelerationStructureBase::SetDebugName(name);
    }
#endif

private:
    RendererResult Rebuild(RTUpdateStateFlags& outUpdateStateFlags);

    Array<VkAccelerationStructureGeometryKHR> GetGeometries() const;
    Array<uint32> GetPrimitiveCounts() const;

    RendererResult BuildInstancesBuffer();
    RendererResult BuildInstancesBuffer(uint32 first, uint32 last);

    RendererResult BuildMeshDescriptionsBuffer();
    RendererResult BuildMeshDescriptionsBuffer(uint32 first, uint32 last);

    Array<VulkanGpuBlasRef> m_blas;
    VulkanGpuBufferRef m_instancesBuffer;
};

} // namespace Hyperion
