/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef RENDERER_ACCELERATION_STRUCTURE_H
#define RENDERER_ACCELERATION_STRUCTURE_H

#include <math/Matrix4.hpp>

#include <core/lib/DynArray.hpp>

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>

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

    AccelerationGeometry(
        Array<PackedVertex> &&packed_vertices,
        Array<PackedIndex> &&packed_indices,
        uint entity_index,
        uint material_index
    );

    AccelerationGeometry(const AccelerationGeometry &other)             = delete;
    AccelerationGeometry &operator=(const AccelerationGeometry &other)  = delete;
    ~AccelerationGeometry();

    const Array<PackedVertex> &GetPackedVertices() const { return m_packed_vertices; }
    const Array<PackedIndex> &GetPackedIndices() const { return m_packed_indices; }

    const GPUBufferRef<PLATFORM> &GetPackedVertexStorageBuffer() const { return m_packed_vertex_buffer; }
    const GPUBufferRef<PLATFORM> &GetPackedIndexStorageBuffer() const { return m_packed_index_buffer; }

    uint GetEntityIndex() const
        { return m_entity_index; }

    // must set proper flag on the parent BLAS
    // for it to take effect
    void SetEntityIndex(uint entity_index)
        { m_entity_index = entity_index; }

    uint GetMaterialIndex() const
        { return m_material_index; }

    // must set proper flag on the parent BLAS
    // for it to take effect
    void SetMaterialIndex(uint material_index)
        { m_material_index = material_index; }

    Result Create(Device<PLATFORM> *device, Instance<PLATFORM> *instance);
    /* Remove from the parent acceleration structure */
    Result Destroy(Device<PLATFORM> *device);

private:
    Array<PackedVertex>                         m_packed_vertices;
    Array<PackedIndex>                          m_packed_indices;

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
    AccelerationStructure();
    AccelerationStructure(const AccelerationStructure &other)               = delete;
    AccelerationStructure &operator=(const AccelerationStructure &other)    = delete;
    ~AccelerationStructure();

    const GPUBufferRef<PLATFORM> &GetBuffer() const
        { return m_buffer; }

    VkAccelerationStructureKHR &GetAccelerationStructure()
        { return m_acceleration_structure; }

    const VkAccelerationStructureKHR &GetAccelerationStructure() const
        { return m_acceleration_structure; }

    uint64 GetDeviceAddress() const
        { return m_device_address; }

    AccelerationStructureFlags GetFlags() const
        { return m_flags; }

    void SetFlag(AccelerationStructureFlagBits flag)
        { m_flags = AccelerationStructureFlags(m_flags | flag); }

    void ClearFlag(AccelerationStructureFlagBits flag)
        { m_flags = AccelerationStructureFlags(m_flags & ~flag); }

    Array<AccelerationGeometryRef<PLATFORM>> &GetGeometries()
        { return m_geometries; }

    const Array<AccelerationGeometryRef<PLATFORM>> &GetGeometries() const
        { return m_geometries; }

    void AddGeometry(AccelerationGeometryRef<PLATFORM> geometry)
        { m_geometries.PushBack(std::move(geometry)); SetNeedsRebuildFlag(); }

    void RemoveGeometry(uint index);

    /*! \brief Remove the geometry from the internal list of Nodes and set a flag that the
     * structure needs to be rebuilt. Will not automatically rebuild.
     */
    void RemoveGeometry(const AccelerationGeometryRef<PLATFORM> &geometry);

    const Matrix4 &GetTransform() const
        { return m_transform; }

    void SetTransform(const Matrix4 &transform)
        { m_transform = transform; SetTransformUpdateFlag(); }

    Result Destroy(Device<PLATFORM> *device);

protected:
    static VkAccelerationStructureTypeKHR ToVkAccelerationStructureType(AccelerationStructureType);
    
    void SetTransformUpdateFlag()
        { SetFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE); }

    void SetNeedsRebuildFlag()
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
    BottomLevelAccelerationStructure();
    BottomLevelAccelerationStructure(const BottomLevelAccelerationStructure &other)             = delete;
    BottomLevelAccelerationStructure &operator=(const BottomLevelAccelerationStructure &other)  = delete;
    ~BottomLevelAccelerationStructure();

    AccelerationStructureType GetType() const { return AccelerationStructureType::BOTTOM_LEVEL; }
    
    Result Create(Device<PLATFORM> *device, Instance<PLATFORM> *instance);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    Result UpdateStructure(Instance<PLATFORM> *instance, RTUpdateStateFlags &out_update_state_flags);

private:
    Result Rebuild(Instance<PLATFORM> *instance, RTUpdateStateFlags &out_update_state_flags);
};

template <PlatformType PLATFORM>
class TopLevelAccelerationStructure : public AccelerationStructure<PLATFORM>
{
public:
    TopLevelAccelerationStructure();
    TopLevelAccelerationStructure(const TopLevelAccelerationStructure &other)               = delete;
    TopLevelAccelerationStructure &operator=(const TopLevelAccelerationStructure &other)    = delete;
    ~TopLevelAccelerationStructure();

    AccelerationStructureType GetType() const { return AccelerationStructureType::TOP_LEVEL; }

    const GPUBufferRef<PLATFORM> &GetMeshDescriptionsBuffer() const
        { return m_mesh_descriptions_buffer; }

    void AddBLAS(BLASRef<PLATFORM> blas);
    void RemoveBLAS(const BLASRef<PLATFORM> &blas);
    
    Result Create(
        Device<PLATFORM> *device,
        Instance<PLATFORM> *instance,
        Array<BLASRef<PLATFORM>> &&blas
    );

    Result Destroy(Device<PLATFORM> *device);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    Result UpdateStructure(Instance<PLATFORM> *instance, RTUpdateStateFlags &out_update_state_flags);

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
