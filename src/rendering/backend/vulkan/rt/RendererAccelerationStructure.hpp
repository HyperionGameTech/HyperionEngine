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

} // namespace platform

using Device    = platform::Device<Platform::VULKAN>;
using Instance  = platform::Instance<Platform::VULKAN>;

class AccelerationStructure;

enum class AccelerationStructureType
{
    BOTTOM_LEVEL,
    TOP_LEVEL
};

using RTUpdateStateFlags = UInt;

enum RTUpdateStateFlagBits : RTUpdateStateFlags
{
    RT_UPDATE_STATE_FLAGS_NONE                          = 0x0,
    RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE = 0x1,
    RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS      = 0x2,
    RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES              = 0x4,
    RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM              = 0x8
};

using AccelerationStructureFlags = UInt;

enum AccelerationStructureFlagBits : AccelerationStructureFlags
{
    ACCELERATION_STRUCTURE_FLAGS_NONE               = 0x0,
    ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING   = 0x1,
    ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE   = 0x2,
    ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE    = 0x4
};

class AccelerationGeometry
{
    friend class AccelerationStructure;
    friend class TopLevelAccelerationStructure;
    friend class BottomLevelAccelerationStructure;

public:
    AccelerationGeometry(
        Array<PackedVertex> &&packed_vertices,
        Array<PackedIndex> &&packed_indices,
        UInt entity_index,
        UInt material_index
    );

    AccelerationGeometry(const AccelerationGeometry &other) = delete;
    AccelerationGeometry &operator=(const AccelerationGeometry &other) = delete;
    ~AccelerationGeometry();

    const Array<PackedVertex> &GetPackedVertices() const { return m_packed_vertices; }
    const Array<PackedIndex> &GetPackedIndices() const { return m_packed_indices; }

    PackedVertexStorageBuffer *GetPackedVertexStorageBuffer() const { return m_packed_vertex_buffer.get(); }
    PackedIndexStorageBuffer *GetPackedIndexStorageBuffer() const { return m_packed_index_buffer.get(); }

    UInt GetEntityIndex() const
        { return m_entity_index; }

    // must set proper flag on the parent BLAS
    // for it to take effect
    void SetEntityIndex(UInt entity_index)
        { m_entity_index = entity_index; }

    UInt GetMaterialIndex() const
        { return m_material_index; }

    // must set proper flag on the parent BLAS
    // for it to take effect
    void SetMaterialIndex(UInt material_index)
        { m_material_index = material_index; }

    Result Create(Device *device, Instance *instance);
    /* Remove from the parent acceleration structure */
    Result Destroy(Device *device);

private:
    Array<PackedVertex>                         m_packed_vertices;
    Array<PackedIndex>                          m_packed_indices;

    std::unique_ptr<PackedVertexStorageBuffer>  m_packed_vertex_buffer;
    std::unique_ptr<PackedIndexStorageBuffer>   m_packed_index_buffer;

    VkAccelerationStructureGeometryKHR          m_geometry;
    
    UInt                                        m_entity_index;
    UInt                                        m_material_index;
};

class AccelerationStructure
{
public:
    AccelerationStructure();
    AccelerationStructure(const AccelerationStructure &other) = delete;
    AccelerationStructure &operator=(const AccelerationStructure &other) = delete;
    ~AccelerationStructure();

    AccelerationStructureBuffer *GetBuffer() const { return m_buffer.get(); }
    AccelerationStructureInstancesBuffer *GetInstancesBuffer() const { return m_instances_buffer.get(); }

    VkAccelerationStructureKHR &GetAccelerationStructure() { return m_acceleration_structure; }
    const VkAccelerationStructureKHR &GetAccelerationStructure() const { return m_acceleration_structure; }

    UInt64 GetDeviceAddress() const { return m_device_address; }

    AccelerationStructureFlags GetFlags() const { return m_flags; }
    void SetFlag(AccelerationStructureFlagBits flag) { m_flags = AccelerationStructureFlags(m_flags | flag); }
    void ClearFlag(AccelerationStructureFlagBits flag) { m_flags = AccelerationStructureFlags(m_flags & ~flag); }

    std::vector<std::unique_ptr<AccelerationGeometry>> &GetGeometries()
        { return m_geometries; }

    const std::vector<std::unique_ptr<AccelerationGeometry>> &GetGeometries() const
        { return m_geometries; }

    void AddGeometry(std::unique_ptr<AccelerationGeometry> &&geometry)
        { m_geometries.push_back(std::move(geometry)); SetNeedsRebuildFlag(); }

    void RemoveGeometry(UInt index)
        { m_geometries.erase(m_geometries.begin() + index); SetNeedsRebuildFlag(); }

    /*! \brief Remove the geometry from the internal list of Nodes and set a flag that the
     * structure needs to be rebuilt. Will not automatically rebuild.
     */
    void RemoveGeometry(AccelerationGeometry *geometry);

    const Matrix4 &GetTransform() const
        { return m_transform; }

    void SetTransform(const Matrix4 &transform)
        { m_transform = transform; SetTransformUpdateFlag(); }

    Result Destroy(Device *device);

protected:
    static VkAccelerationStructureTypeKHR ToVkAccelerationStructureType(AccelerationStructureType);
    
    void SetTransformUpdateFlag()
        { SetFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE); }

    void SetNeedsRebuildFlag()
        { SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING); }

    Result CreateAccelerationStructure(
        Instance *instance,
        AccelerationStructureType type,
        std::vector<VkAccelerationStructureGeometryKHR> &&geometries,
        std::vector<UInt32> &&primitive_counts,
        bool update,
        RTUpdateStateFlags &out_update_state_flags
    );
    
    std::unique_ptr<AccelerationStructureBuffer> m_buffer;
    std::unique_ptr<AccelerationStructureInstancesBuffer> m_instances_buffer;
    std::unique_ptr<ScratchBuffer> m_scratch_buffer;
    std::vector<std::unique_ptr<AccelerationGeometry>> m_geometries;
    Matrix4 m_transform;
    VkAccelerationStructureKHR m_acceleration_structure;
    UInt64 m_device_address;
    AccelerationStructureFlags m_flags;
};

class BottomLevelAccelerationStructure : public AccelerationStructure
{
public:
    BottomLevelAccelerationStructure();
    BottomLevelAccelerationStructure(const BottomLevelAccelerationStructure &other) = delete;
    BottomLevelAccelerationStructure &operator=(const BottomLevelAccelerationStructure &other) = delete;
    ~BottomLevelAccelerationStructure();

    AccelerationStructureType GetType() const { return AccelerationStructureType::BOTTOM_LEVEL; }
    
    Result Create(Device *device, Instance *instance);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    Result UpdateStructure(Instance *instance, RTUpdateStateFlags &out_update_state_flags);

private:
    Result Rebuild(Instance *instance, RTUpdateStateFlags &out_update_state_flags);
};

class TopLevelAccelerationStructure : public AccelerationStructure
{
public:
    TopLevelAccelerationStructure();
    TopLevelAccelerationStructure(const TopLevelAccelerationStructure &other) = delete;
    TopLevelAccelerationStructure &operator=(const TopLevelAccelerationStructure &other) = delete;
    ~TopLevelAccelerationStructure();

    AccelerationStructureType GetType() const { return AccelerationStructureType::TOP_LEVEL; }
    StorageBuffer *GetMeshDescriptionsBuffer() const { return m_mesh_descriptions_buffer.get(); }

    void AddBLAS(BottomLevelAccelerationStructure *blas);
    void RemoveBLAS(BottomLevelAccelerationStructure *blas);
    
    Result Create(
        Device *device,
        Instance *instance,
        std::vector<BottomLevelAccelerationStructure *> &&blas
    );

    Result Destroy(Device *device);

    /*! \brief Rebuild IF the rebuild flag has been set. Otherwise this is a no-op. */
    Result UpdateStructure(Instance *instance, RTUpdateStateFlags &out_update_state_flags);

private:
    Result Rebuild(Instance *instance, RTUpdateStateFlags &out_update_state_flags);

    std::vector<VkAccelerationStructureGeometryKHR> GetGeometries(Instance *instance) const;
    std::vector<UInt32> GetPrimitiveCounts() const;

    Result CreateOrRebuildInstancesBuffer(Instance *instance);
    Result UpdateInstancesBuffer(Instance *instance, UInt first, UInt last);
    
    Result CreateMeshDescriptionsBuffer(Instance *instance);
    Result UpdateMeshDescriptionsBuffer(Instance *instance);
    Result UpdateMeshDescriptionsBuffer(Instance *instance, UInt first, UInt last);
    Result RebuildMeshDescriptionsBuffer(Instance *instance);

    std::vector<BottomLevelAccelerationStructure *> m_blas;
    std::unique_ptr<StorageBuffer> m_mesh_descriptions_buffer;
};

} // namespace renderer
} // namespace hyperion

#endif
