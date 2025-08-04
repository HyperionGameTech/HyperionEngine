/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/rt/RenderAccelerationStructure.hpp>


#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/Shared.hpp>

#include <core/math/Matrix4.hpp>

#include <core/containers/Array.hpp>

#include <core/utilities/Span.hpp>

#include <core/Handle.hpp>

#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

class Entity;
class Material;

class HYP_API VulkanAccelerationGeometry final : public RenderObject<VulkanAccelerationGeometry>
{
public:
    friend class VulkanTLAS;
    friend class VulkanBLAS;

    VulkanAccelerationGeometry(
        const GpuBufferRef& packedVerticesBuffer,
        const GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material);

    virtual ~VulkanAccelerationGeometry() override;

    HYP_FORCE_INLINE const GpuBufferRef& GetPackedVerticesBuffer() const
    {
        return m_packedVerticesBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetPackedIndicesBuffer() const
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
    /* Remove from the parent acceleration structure */
    RendererResult Destroy();

private:
    bool m_isCreated;

    GpuBufferRef m_packedVerticesBuffer;
    GpuBufferRef m_packedIndicesBuffer;

    uint32 m_numVertices;
    uint32 m_numIndices;

    Handle<Material> m_material;

    VkAccelerationStructureGeometryKHR m_geometry;
};

using VulkanAccelerationGeometryRef = RenderObjectHandle_Strong<VulkanAccelerationGeometry>;
using VulkanAccelerationGeometryWeakRef = RenderObjectHandle_Weak<VulkanAccelerationGeometry>;

class HYP_API VulkanAccelerationStructureBase
{
protected:
    VulkanAccelerationStructureBase(const Matrix4& transform = Matrix4::Identity());
     ~VulkanAccelerationStructureBase();

public:
    HYP_FORCE_INLINE const GpuBufferRef& GetBuffer() const
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

    HYP_FORCE_INLINE EnumFlags<AccelerationStructureFlags> GetFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE void SetFlag(EnumFlags<AccelerationStructureFlags> flag)
    {
        m_flags |= flag;
    }

    HYP_FORCE_INLINE void ClearFlag(EnumFlags<AccelerationStructureFlags> flag)
    {
        m_flags &= ~flag;
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

    HYP_FORCE_INLINE const Matrix4& GetTransform() const
    {
        return m_transform;
    }

    HYP_FORCE_INLINE void SetTransform(const Matrix4& transform)
    {
        if (m_transform == transform)
        {
            // same transforms, don't set the flag
            return;
        }

        m_transform = transform;
        SetTransformUpdateFlag();
    }

    RendererResult Destroy();

protected:
    HYP_FORCE_INLINE void SetTransformUpdateFlag()
    {
        SetFlag(ASF_TRANSFORM_UPDATE);
    }

    HYP_FORCE_INLINE void SetNeedsRebuildFlag()
    {
        SetFlag(ASF_NEEDS_REBUILDING);
    }

    RendererResult CreateAccelerationStructure(
        AccelerationStructureType type,
        Span<const VkAccelerationStructureGeometryKHR> geometries,
        Span<const uint32> primitiveCounts,
        bool update,
        EnumFlags<RaytracingUpdateFlags>& outUpdateStateFlags);

    GpuBufferRef m_buffer;
    GpuBufferRef m_scratchBuffer;
    Array<VulkanAccelerationGeometryRef> m_geometries;
    Matrix4 m_transform;
    VkAccelerationStructureKHR m_accelerationStructure;
    uint64 m_deviceAddress;
    EnumFlags<AccelerationStructureFlags> m_flags;
};

using VulkanAccelerationStructureRef = RenderObjectHandle_Strong<VulkanAccelerationStructureBase>;
using VulkanAccelerationStructureWeakRef = RenderObjectHandle_Weak<VulkanAccelerationStructureBase>;

class VulkanBLAS final : public BLASBase, public VulkanAccelerationStructureBase
{
    friend class VulkanTLAS;

public:
    VulkanBLAS(
        const GpuBufferRef& packedVerticesBuffer,
        const GpuBufferRef& packedIndicesBuffer,
        uint32 numVertices,
        uint32 numIndices,
        const Handle<Material>& material,
        const Matrix4& transform);
    virtual ~VulkanBLAS() override;

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;
    virtual RendererResult Destroy() override;

    virtual void SetTransform(const Matrix4& transform) override
    {
        VulkanAccelerationStructureBase::SetTransform(transform);
    }

    virtual void SetMaterialBinding(uint32 materialBinding) override;

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    RendererResult UpdateStructure(EnumFlags<RaytracingUpdateFlags>& outUpdateStateFlags);

private:
    RendererResult Rebuild(EnumFlags<RaytracingUpdateFlags>& outUpdateStateFlags);

    GpuBufferRef m_packedVerticesBuffer;
    GpuBufferRef m_packedIndicesBuffer;
};

class VulkanTLAS final : public TLASBase, public VulkanAccelerationStructureBase
{
public:
    VulkanTLAS();
    virtual ~VulkanTLAS() override;

    virtual bool IsCreated() const override;

    virtual void AddBLAS(const BLASRef& blas) override;
    virtual void RemoveBLAS(const BLASRef& blas) override;
    virtual bool HasBLAS(const BLASRef& blas) override;

    virtual RendererResult Create() override;
    virtual RendererResult Destroy() override;

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    virtual RendererResult UpdateStructure(EnumFlags<RaytracingUpdateFlags>& outUpdateStateFlags) override;

private:
    RendererResult Rebuild(EnumFlags<RaytracingUpdateFlags>& outUpdateStateFlags);

    Array<VkAccelerationStructureGeometryKHR> GetGeometries() const;
    Array<uint32> GetPrimitiveCounts() const;

    RendererResult BuildInstancesBuffer();
    RendererResult BuildInstancesBuffer(uint32 first, uint32 last);

    RendererResult BuildMeshDescriptionsBuffer();
    RendererResult BuildMeshDescriptionsBuffer(uint32 first, uint32 last);

    Array<VulkanBLASRef> m_blas;
    GpuBufferRef m_instancesBuffer;
};

} // namespace hyperion
