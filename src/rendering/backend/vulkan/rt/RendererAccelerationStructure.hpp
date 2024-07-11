/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef RENDERER_ACCELERATION_STRUCTURE_HPP
#define RENDERER_ACCELERATION_STRUCTURE_HPP

#include <math/Matrix4.hpp>

#include <core/containers/Array.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <streaming/StreamedMeshData.hpp>

#include <Types.hpp>

#include <vector>
#include <memory>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class AccelerationStructure;

template <PlatformType PLATFORM>
class TopLevelAccelerationStructure;

template <PlatformType PLATFORM>
class BottomLevelAccelerationStructure;

template <PlatformType PLATFORM>
class AccelerationGeometry
{
public:
    friend class TopLevelAccelerationStructure<PLATFORM>;
    friend class BottomLevelAccelerationStructure<PLATFORM>;

    static constexpr PlatformType platform = PLATFORM;

    HYP_API AccelerationGeometry(
        const Array<PackedVertex> &packed_vertices,
        const Array<uint32> &packed_indices,
        uint entity_index,
        uint material_index
    );

    AccelerationGeometry(const AccelerationGeometry &other)             = delete;
    AccelerationGeometry &operator=(const AccelerationGeometry &other)  = delete;
    HYP_API ~AccelerationGeometry();

    HYP_FORCE_INLINE const Array<PackedVertex> &GetPackedVertices() const
        { return m_packed_vertices; }

    HYP_FORCE_INLINE const Array<uint32> &GetPackedIndices() const
        { return m_packed_indices; }

    HYP_FORCE_INLINE const GPUBufferRef<PLATFORM> &GetPackedVertexStorageBuffer() const
        { return m_packed_vertex_buffer; }

    HYP_FORCE_INLINE const GPUBufferRef<PLATFORM> &GetPackedIndexStorageBuffer() const
        { return m_packed_index_buffer; }

    HYP_FORCE_INLINE uint GetEntityIndex() const
        { return m_entity_index; }

    // must set proper flag on the parent BLAS
    // for it to take effect
    HYP_FORCE_INLINE void SetEntityIndex(uint entity_index)
        { m_entity_index = entity_index; }

    HYP_FORCE_INLINE uint GetMaterialIndex() const
        { return m_material_index; }

    // must set proper flag on the parent BLAS
    // for it to take effect
    HYP_FORCE_INLINE void SetMaterialIndex(uint material_index)
        { m_material_index = material_index; }

    HYP_API Result Create(Device<PLATFORM> *device, Instance<PLATFORM> *instance);
    /* Remove from the parent acceleration structure */
    HYP_API Result Destroy(Device<PLATFORM> *device);

private:
    Array<PackedVertex>                         m_packed_vertices;
    Array<uint32>                               m_packed_indices;

    GPUBufferRef<PLATFORM>                      m_packed_vertex_buffer;
    GPUBufferRef<PLATFORM>                      m_packed_index_buffer;
    
    uint                                        m_entity_index;
    uint                                        m_material_index;

    VkAccelerationStructureGeometryKHR          m_geometry;
};

template <PlatformType PLATFORM>
class AccelerationStructure
{
public:
    static constexpr PlatformType platform = PLATFORM;

    HYP_API AccelerationStructure(const Matrix4 &transform = Matrix4::Identity());
    AccelerationStructure(const AccelerationStructure &other)               = delete;
    AccelerationStructure &operator=(const AccelerationStructure &other)    = delete;
    HYP_API ~AccelerationStructure();

    HYP_FORCE_INLINE const GPUBufferRef<PLATFORM> &GetBuffer() const
        { return m_buffer; }

    HYP_FORCE_INLINE const VkAccelerationStructureKHR &GetAccelerationStructure() const
        { return m_acceleration_structure; }

    HYP_FORCE_INLINE uint64 GetDeviceAddress() const
        { return m_device_address; }

    HYP_FORCE_INLINE AccelerationStructureFlags GetFlags() const
        { return m_flags; }

    HYP_FORCE_INLINE void SetFlag(AccelerationStructureFlagBits flag)
        { m_flags = AccelerationStructureFlags(m_flags | flag); }

    HYP_FORCE_INLINE void ClearFlag(AccelerationStructureFlagBits flag)
        { m_flags = AccelerationStructureFlags(m_flags & ~flag); }

    HYP_FORCE_INLINE const Array<AccelerationGeometryRef<PLATFORM>> &GetGeometries() const
        { return m_geometries; }

    HYP_FORCE_INLINE void AddGeometry(AccelerationGeometryRef<PLATFORM> geometry)
        { m_geometries.PushBack(std::move(geometry)); SetNeedsRebuildFlag(); }

    HYP_API void RemoveGeometry(uint index);

    /*! \brief Remove the geometry from the internal list of Nodes and set a flag that the
     * structure needs to be rebuilt. Will not automatically rebuild.
     */
    HYP_API void RemoveGeometry(const AccelerationGeometryRef<PLATFORM> &geometry);

    HYP_FORCE_INLINE const Matrix4 &GetTransform() const
        { return m_transform; }

    HYP_FORCE_INLINE void SetTransform(const Matrix4 &transform)
        { m_transform = transform; SetTransformUpdateFlag(); }

    HYP_API Result Destroy(Device<PLATFORM> *device);

protected:
    static VkAccelerationStructureTypeKHR ToVkAccelerationStructureType(AccelerationStructureType);
    
    HYP_FORCE_INLINE void SetTransformUpdateFlag()
        { SetFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE); }

    HYP_FORCE_INLINE void SetNeedsRebuildFlag()
        { SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING); }

    Result CreateAccelerationStructure(
        Instance<PLATFORM> *instance,
        AccelerationStructureType type,
        const std::vector<VkAccelerationStructureGeometryKHR> &geometries,
        const std::vector<uint32> &primitive_counts,
        bool update,
        RTUpdateStateFlags &out_update_state_flags
    );
    
    GPUBufferRef<PLATFORM>                      m_buffer;
    GPUBufferRef<PLATFORM>                      m_instances_buffer;
    GPUBufferRef<PLATFORM>                      m_scratch_buffer;
    Array<AccelerationGeometryRef<PLATFORM>>    m_geometries;
    Matrix4                                     m_transform;
    VkAccelerationStructureKHR                  m_acceleration_structure;
    uint64                                      m_device_address;
    AccelerationStructureFlags                  m_flags;
};

template <PlatformType PLATFORM>
class BottomLevelAccelerationStructure : public AccelerationStructure<PLATFORM>
{
public:
    static constexpr PlatformType platform = PLATFORM;

    HYP_API BottomLevelAccelerationStructure(const Matrix4 &transform = Matrix4::Identity());
    BottomLevelAccelerationStructure(const BottomLevelAccelerationStructure &other)             = delete;
    BottomLevelAccelerationStructure &operator=(const BottomLevelAccelerationStructure &other)  = delete;
    HYP_API ~BottomLevelAccelerationStructure();

    HYP_FORCE_INLINE AccelerationStructureType GetType() const
        { return AccelerationStructureType::BOTTOM_LEVEL; }
    
    HYP_API Result Create(Device<PLATFORM> *device, Instance<PLATFORM> *instance);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    HYP_API Result UpdateStructure(Instance<PLATFORM> *instance, RTUpdateStateFlags &out_update_state_flags);

private:
    Result Rebuild(Instance<PLATFORM> *instance, RTUpdateStateFlags &out_update_state_flags);
};

template <PlatformType PLATFORM>
class TopLevelAccelerationStructure : public AccelerationStructure<PLATFORM>
{
public:
    static constexpr PlatformType platform = PLATFORM;

    HYP_API TopLevelAccelerationStructure();
    TopLevelAccelerationStructure(const TopLevelAccelerationStructure &other)               = delete;
    TopLevelAccelerationStructure &operator=(const TopLevelAccelerationStructure &other)    = delete;
    HYP_API ~TopLevelAccelerationStructure();

    HYP_FORCE_INLINE AccelerationStructureType GetType() const
        { return AccelerationStructureType::TOP_LEVEL; }

    HYP_FORCE_INLINE const GPUBufferRef<PLATFORM> &GetMeshDescriptionsBuffer() const
        { return m_mesh_descriptions_buffer; }

    HYP_API void AddBLAS(const BLASRef<PLATFORM> &blas);
    HYP_API void RemoveBLAS(const BLASRef<PLATFORM> &blas);
    
    HYP_API Result Create(
        Device<PLATFORM> *device,
        Instance<PLATFORM> *instance
    );

    HYP_API Result Destroy(Device<PLATFORM> *device);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    HYP_API Result UpdateStructure(Instance<PLATFORM> *instance, RTUpdateStateFlags &out_update_state_flags);

private:
    Result Rebuild(Instance<PLATFORM> *instance, RTUpdateStateFlags &out_update_state_flags);

    std::vector<VkAccelerationStructureGeometryKHR> GetGeometries(Instance<PLATFORM> *instance) const;
    std::vector<uint32> GetPrimitiveCounts() const;

    Result CreateOrRebuildInstancesBuffer(Instance<PLATFORM> *instance);
    Result UpdateInstancesBuffer(Instance<PLATFORM> *instance, uint first, uint last);
    
    Result CreateMeshDescriptionsBuffer(Instance<PLATFORM> *instance);
    Result UpdateMeshDescriptionsBuffer(Instance<PLATFORM> *instance);
    Result UpdateMeshDescriptionsBuffer(Instance<PLATFORM> *instance, uint first, uint last);
    Result RebuildMeshDescriptionsBuffer(Instance<PLATFORM> *instance);

    Array<BLASRef<PLATFORM>>    m_blas;
    GPUBufferRef<PLATFORM>      m_mesh_descriptions_buffer;
};

} // namespace platform

} // namespace renderer
} // namespace hyperion

#endif
