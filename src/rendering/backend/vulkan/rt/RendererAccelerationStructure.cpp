/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/rt/RendererAccelerationStructure.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <rendering/RenderMaterial.hpp>

// @TODO: Refactor to not need this include
#include <scene/Material.hpp>

#include <core/utilities/Range.hpp>
#include <core/math/MathUtil.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

static VkTransformMatrixKHR ToVkTransform(const Matrix4 &matrix)
{
    VkTransformMatrixKHR transform;
    std::memcpy(&transform, matrix.values, sizeof(VkTransformMatrixKHR));

    return transform;
}

template <>
VkAccelerationStructureTypeKHR AccelerationStructure<Platform::VULKAN>::ToVkAccelerationStructureType(AccelerationStructureType type)
{
    switch (type) {
    case AccelerationStructureType::BOTTOM_LEVEL:
        return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    case AccelerationStructureType::TOP_LEVEL:
        return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    default:
        return VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
    }
}

// explicit specialization declarations

template <>
RendererResult AccelerationGeometry<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device);

template <>
RendererResult AccelerationStructure<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device);

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device);

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::CreateOrRebuildInstancesBuffer(Instance<Platform::VULKAN> *instance);

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::Rebuild(Instance<Platform::VULKAN> *instance, RTUpdateStateFlags &out_update_state_flags);

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::CreateMeshDescriptionsBuffer(Instance<Platform::VULKAN> *instance);

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::UpdateMeshDescriptionsBuffer(Instance<Platform::VULKAN> *instance);

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::UpdateMeshDescriptionsBuffer(Instance<Platform::VULKAN> *instance, uint32 first, uint32 last);

template <>
RendererResult BottomLevelAccelerationStructure<Platform::VULKAN>::Rebuild(Instance<Platform::VULKAN> *instance, RTUpdateStateFlags &out_update_state_flags);

template <>
RendererResult BottomLevelAccelerationStructure<Platform::VULKAN>::UpdateStructure(Instance<Platform::VULKAN> *instance, RTUpdateStateFlags &out_update_state_flags);

// explicit specialization definitions

#pragma region AccelerationGeometry

template <>
AccelerationGeometry<Platform::VULKAN>::AccelerationGeometry(
    const Array<PackedVertex> &packed_vertices,
    const Array<uint32> &packed_indices,
    const WeakHandle<Entity> &entity,
    const Handle<Material> &material
) : m_packed_vertices(packed_vertices),
    m_packed_indices(packed_indices),
    m_packed_vertex_buffer(MakeRenderObject<GPUBuffer<Platform::VULKAN>>(GPUBufferType::RT_MESH_VERTEX_BUFFER)),
    m_packed_index_buffer(MakeRenderObject<GPUBuffer<Platform::VULKAN>>(GPUBufferType::RT_MESH_INDEX_BUFFER)),
    m_entity(entity),
    m_material(material),
    m_geometry { }
{
    m_packed_vertex_buffer.SetName(NAME("RTPackedVertexBuffer"));
    m_packed_index_buffer.SetName(NAME("RTPackedIndexBuffer"));
}

template <>
AccelerationGeometry<Platform::VULKAN>::~AccelerationGeometry()
{
}

template <>
bool AccelerationGeometry<Platform::VULKAN>::IsCreated() const
{
    return m_packed_vertex_buffer.IsValid() && m_packed_vertex_buffer->IsCreated()
        && m_packed_index_buffer.IsValid() && m_packed_index_buffer->IsCreated();
}

template <>
RendererResult AccelerationGeometry<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, Instance<Platform::VULKAN> *instance)
{
    if (IsCreated()) {
        return HYP_MAKE_ERROR(RendererError, "Cannot initialize acceleration geometry that has already been initialized");
    }

    if (m_material.IsValid()) {
        m_material->GetRenderResource().Claim();
    }

    if (!device->GetFeatures().IsRaytracingSupported()) {
        return HYP_MAKE_ERROR(RendererError, "Device does not support raytracing");
    }

    if (m_packed_vertices.Empty() || m_packed_indices.Empty()) {
        return HYP_MAKE_ERROR(RendererError, "An acceleration geometry must have nonzero vertex count, index count, and vertex size.");
    }

    // some assertions to prevent gpu faults
    for (SizeType i = 0; i < m_packed_indices.Size(); i++) {
        AssertThrow(m_packed_indices[i] < m_packed_vertices.Size());
    }

    RendererResult result { };

    const SizeType packed_vertices_size = m_packed_vertices.Size() * sizeof(PackedVertex);
    const SizeType packed_indices_size = m_packed_indices.Size() * sizeof(uint32);

    HYPERION_BUBBLE_ERRORS(
        m_packed_vertex_buffer->Create(device, packed_vertices_size)
    );

    HYPERION_PASS_ERRORS(
        m_packed_index_buffer->Create(device, packed_indices_size),
        result
    );

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    GPUBufferRef<Platform::VULKAN> vertices_staging_buffer = MakeRenderObject<GPUBuffer<Platform::VULKAN>>(GPUBufferType::STAGING_BUFFER);
    HYPERION_BUBBLE_ERRORS(vertices_staging_buffer->Create(device, packed_vertices_size));
    vertices_staging_buffer->Memset(device, packed_vertices_size, 0x0); // zero out
    vertices_staging_buffer->Copy(device, m_packed_vertices.Size() * sizeof(PackedVertex), m_packed_vertices.Data());
    
    if (!result) {
        HYPERION_IGNORE_ERRORS(vertices_staging_buffer->Destroy(device));

        return result;
    }
    
    GPUBufferRef<Platform::VULKAN> indices_staging_buffer = MakeRenderObject<GPUBuffer<Platform::VULKAN>>(GPUBufferType::STAGING_BUFFER);
    HYPERION_BUBBLE_ERRORS(indices_staging_buffer->Create(device, packed_indices_size));
    indices_staging_buffer->Memset(device, packed_indices_size, 0x0); // zero out
    indices_staging_buffer->Copy(device, m_packed_indices.Size() * sizeof(uint32), m_packed_indices.Data());

    if (!result) {
        HYPERION_IGNORE_ERRORS(vertices_staging_buffer->Destroy(device));
        HYPERION_IGNORE_ERRORS(indices_staging_buffer->Destroy(device));

        return result;
    }
    
    SingleTimeCommands<Platform::CURRENT> commands { device };

    commands.Push([&](CommandBuffer<Platform::VULKAN> *cmd)
    {
        m_packed_vertex_buffer->CopyFrom(cmd, vertices_staging_buffer, packed_vertices_size);
        m_packed_index_buffer->CopyFrom(cmd, indices_staging_buffer, packed_indices_size);

        HYPERION_RETURN_OK;
    });

    HYPERION_PASS_ERRORS(commands.Execute(), result);

    SafeRelease(std::move(vertices_staging_buffer));
    SafeRelease(std::move(indices_staging_buffer));

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    VkDeviceOrHostAddressConstKHR vertices_address {
        .deviceAddress = m_packed_vertex_buffer->GetBufferDeviceAddress(device)
    };

    VkDeviceOrHostAddressConstKHR indices_address {
        .deviceAddress = m_packed_index_buffer->GetBufferDeviceAddress(device)
    };

    m_geometry = VkAccelerationStructureGeometryKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    m_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    m_geometry.geometry = {
        .triangles = VkAccelerationStructureGeometryTrianglesDataKHR {
            .sType 			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat 	= VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData 	= vertices_address,
            .vertexStride 	= sizeof(PackedVertex),
            .maxVertex 		= uint32(m_packed_vertices.Size()),
            .indexType 		= VK_INDEX_TYPE_UINT32,
            .indexData 		= indices_address,
            .transformData 	= { { } }
        }
    };

    HYPERION_RETURN_OK;
}

template <>
RendererResult AccelerationGeometry<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (!IsCreated()) {
        return HYP_MAKE_ERROR(RendererError, "Cannot destroy acceleration geometry that has not been initialized");
    }

    if (m_material.IsValid()) {
        m_material->GetRenderResource().Unclaim();
    }

    RendererResult result;

    SafeRelease(std::move(m_packed_vertex_buffer));
    SafeRelease(std::move(m_packed_index_buffer));

    return result;
}

#pragma endregion AccelerationGeometry

#pragma region AccelerationStructure

template <>
AccelerationStructure<Platform::VULKAN>::AccelerationStructure(const Matrix4 &transform)
    : m_transform(transform),
      m_acceleration_structure(VK_NULL_HANDLE),
      m_instances_buffer(MakeRenderObject<GPUBuffer<Platform::VULKAN>>(GPUBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER)),
      m_device_address(0),
      m_flags(ACCELERATION_STRUCTURE_FLAGS_NONE)
{
}

template <>
AccelerationStructure<Platform::VULKAN>::~AccelerationStructure()
{
    AssertThrowMsg(
        m_acceleration_structure == VK_NULL_HANDLE,
        "Expected acceleration structure to have been destroyed before destructor call"
    );

    AssertThrowMsg(
        m_buffer == nullptr,
        "Acceleration structure buffer should have been destroyed before destructor call"
    );

    AssertThrowMsg(
        m_instances_buffer == nullptr,
        "Instances buffer should have been destroyed before destructor call"
    );

    AssertThrowMsg(
        m_scratch_buffer == nullptr,
        "Scratch buffer should have been destroyed before destructor call"
    );
}

template <>
RendererResult AccelerationStructure<Platform::VULKAN>::CreateAccelerationStructure(
    Instance<Platform::VULKAN> *instance,
    AccelerationStructureType type,
    const std::vector<VkAccelerationStructureGeometryKHR> &geometries,
    const std::vector<uint32> &primitive_counts,
    bool update,
    RTUpdateStateFlags &out_update_state_flags
)
{
    if (update) {
        AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

        // TEMP: we should have two TLASes and two RT descriptor sets I suppose...
        //HYPERION_BUBBLE_ERRORS(instance->GetDevice()->Wait());
    } else {
        AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);
    }

    if (!instance->GetDevice()->GetFeatures().IsRaytracingSupported()) {
        return HYP_MAKE_ERROR(RendererError, "Device does not support raytracing");
    }

    if (geometries.empty()) {
        return HYP_MAKE_ERROR(RendererError, "Geometries empty");
    }

    RendererResult result;
    Device<Platform::VULKAN> *device = instance->GetDevice();
    
    if (m_buffer == nullptr) {
        m_buffer = MakeRenderObject<GPUBuffer<Platform::VULKAN>>(GPUBufferType::ACCELERATION_STRUCTURE_BUFFER);
    }
    
    VkAccelerationStructureBuildGeometryInfoKHR geometry_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    geometry_info.type = ToVkAccelerationStructureType(type);
    geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    geometry_info.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometry_info.geometryCount = uint32(geometries.size());
    geometry_info.pGeometries = geometries.data();

    AssertThrow(primitive_counts.size() == geometries.size());
    
    VkAccelerationStructureBuildSizesInfoKHR build_sizes_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    device->GetFeatures().dyn_functions.vkGetAccelerationStructureBuildSizesKHR(
        device->GetDevice(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &geometry_info,
        primitive_counts.data(),
        &build_sizes_info
    );

    const SizeType scratch_buffer_alignment = device->GetFeatures().GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
    SizeType acceleration_structure_size = MathUtil::NextMultiple(build_sizes_info.accelerationStructureSize, 256ull);
    SizeType build_scratch_size = MathUtil::NextMultiple(build_sizes_info.buildScratchSize, scratch_buffer_alignment);
    SizeType update_scratch_size = MathUtil::NextMultiple(build_sizes_info.updateScratchSize, scratch_buffer_alignment);

    bool was_rebuilt = false;

    HYPERION_BUBBLE_ERRORS(m_buffer->EnsureCapacity(
        device,
        acceleration_structure_size,
        &was_rebuilt
    ));

    // force recreate (for debug)
    was_rebuilt = true;

    if (was_rebuilt) {
        out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

        if (update) {
            HYPERION_BUBBLE_ERRORS(device->Wait()); // To prevent deletion while in use 

            // delete the current acceleration structure
            device->GetFeatures().dyn_functions.vkDestroyAccelerationStructureKHR(
                device->GetDevice(),
                m_acceleration_structure,
                nullptr
            );

            m_acceleration_structure = VK_NULL_HANDLE;

            // fetch the corrected acceleration structure and scratch buffer sizes
            // update was true but we need to rebuild from scratch, have to unset the UPDATE flag. 
            geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

            device->GetFeatures().dyn_functions.vkGetAccelerationStructureBuildSizesKHR(
                device->GetDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &geometry_info,
                primitive_counts.data(),
                &build_sizes_info
            );

            acceleration_structure_size = MathUtil::NextMultiple(build_sizes_info.accelerationStructureSize, 256ull);
            build_scratch_size = MathUtil::NextMultiple(build_sizes_info.buildScratchSize, scratch_buffer_alignment);
            update_scratch_size = MathUtil::NextMultiple(build_sizes_info.updateScratchSize, scratch_buffer_alignment);

            AssertThrow(m_buffer->Size() >= acceleration_structure_size);
        }

        // to be sure it's zeroed out
        m_buffer->Memset(device, acceleration_structure_size, 0x0);

        VkAccelerationStructureCreateInfoKHR create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        create_info.buffer = m_buffer->GetPlatformImpl().handle;
        create_info.size = acceleration_structure_size;
        create_info.type = ToVkAccelerationStructureType(type);

        HYPERION_VK_PASS_ERRORS(
            device->GetFeatures().dyn_functions.vkCreateAccelerationStructureKHR(
                device->GetDevice(),
                &create_info,
                VK_NULL_HANDLE,
                &m_acceleration_structure
            ),
            result
        );

        if (!result) {
            HYPERION_IGNORE_ERRORS(Destroy(device));

            return result;
        }
    }

    AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

    VkAccelerationStructureDeviceAddressInfoKHR address_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    address_info.accelerationStructure = m_acceleration_structure;

    m_device_address = device->GetFeatures().dyn_functions.vkGetAccelerationStructureDeviceAddressKHR(
        device->GetDevice(),
        &address_info
    );

    const SizeType scratch_size = (update && !was_rebuilt) ? update_scratch_size : build_scratch_size;

    if (m_scratch_buffer == nullptr) {
        m_scratch_buffer = MakeRenderObject<GPUBuffer<Platform::VULKAN>>(GPUBufferType::SCRATCH_BUFFER);
        
        HYPERION_PASS_ERRORS(
            m_scratch_buffer->Create(device, scratch_size, scratch_buffer_alignment),
            result
        );
    } else {
        HYPERION_PASS_ERRORS(
            m_scratch_buffer->EnsureCapacity(device, scratch_size, scratch_buffer_alignment),
            result
        );
    }

    // zero out scratch buffer
    m_scratch_buffer->Memset(device, m_scratch_buffer->Size(), 0);

    geometry_info.dstAccelerationStructure = m_acceleration_structure;
    geometry_info.srcAccelerationStructure = (update && !was_rebuilt) ? m_acceleration_structure : VK_NULL_HANDLE;
    geometry_info.scratchData = { .deviceAddress = m_scratch_buffer->GetBufferDeviceAddress(device) };
    
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> range_infos;
    range_infos.resize(geometries.size());

    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> range_info_ptrs;
    range_info_ptrs.resize(geometries.size());

    for (SizeType i = 0; i < geometries.size(); i++) {
        range_infos[i] = VkAccelerationStructureBuildRangeInfoKHR {
            .primitiveCount = primitive_counts[i],
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };

        range_info_ptrs[i] = &range_infos[i];
    }

    SingleTimeCommands<Platform::VULKAN> commands { device };

    commands.Push([&](const CommandBufferRef<Platform::VULKAN> &command_buffer)
    {
        device->GetFeatures().dyn_functions.vkCmdBuildAccelerationStructuresKHR(
            command_buffer->GetPlatformImpl().command_buffer,
            uint32(range_info_ptrs.size()),
            &geometry_info,
            range_info_ptrs.data()
        );

        HYPERION_RETURN_OK; 
    });

    HYPERION_PASS_ERRORS(commands.Execute(), result);

    ClearFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);

    HYPERION_RETURN_OK;
}

template <>
RendererResult AccelerationStructure<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    RendererResult result;

    SafeRelease(std::move(m_geometries));
    SafeRelease(std::move(m_buffer));
    SafeRelease(std::move(m_instances_buffer));
    SafeRelease(std::move(m_scratch_buffer));

    if (m_acceleration_structure != VK_NULL_HANDLE) {
        device->GetFeatures().dyn_functions.vkDestroyAccelerationStructureKHR(
            device->GetDevice(),
            m_acceleration_structure,
            VK_NULL_HANDLE
        );

        m_acceleration_structure = VK_NULL_HANDLE;
    }

    return result;
}

template <>
void AccelerationStructure<Platform::VULKAN>::RemoveGeometry(uint32 index)
{
    const auto it = m_geometries.Begin() + index;

    if (it >= m_geometries.End()) {
        return;
    }

    SafeRelease(std::move(*it));

    m_geometries.Erase(it);

    SetNeedsRebuildFlag();
}

template <>
void AccelerationStructure<Platform::VULKAN>::RemoveGeometry(const AccelerationGeometryRef<Platform::VULKAN> &geometry)
{
    if (geometry == nullptr) {
        return;
    }

    const auto it = m_geometries.Find(geometry);

    if (it == m_geometries.End()) {
        return;
    }

    SafeRelease(std::move(*it));

    m_geometries.Erase(it);

    SetNeedsRebuildFlag();
}

#pragma endregion AccelerationStructure

#pragma region TopLevelAccelerationStructure

template <>
TopLevelAccelerationStructure<Platform::VULKAN>::TopLevelAccelerationStructure()
    : AccelerationStructure(),
      m_mesh_descriptions_buffer(MakeRenderObject<GPUBuffer<Platform::VULKAN>>(GPUBufferType::STORAGE_BUFFER))
{
    m_instances_buffer.SetName(NAME("TLASInstancesBuffer"));
    m_mesh_descriptions_buffer.SetName(NAME("TLASMeshDescriptionsBuffer"));
}

template <>
TopLevelAccelerationStructure<Platform::VULKAN>::~TopLevelAccelerationStructure()
{
    AssertThrowMsg(m_acceleration_structure == VK_NULL_HANDLE, "Acceleration structure should have been destroyed before destructor call");
}

template <>
std::vector<VkAccelerationStructureGeometryKHR> TopLevelAccelerationStructure<Platform::VULKAN>::GetGeometries(Instance<Platform::VULKAN> *instance) const
{
    return {
        {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {
                .instances = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                    .arrayOfPointers = VK_FALSE,
                    .data = { .deviceAddress = m_instances_buffer->GetBufferDeviceAddress(instance->GetDevice()) }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        }
    };
}

template <>
std::vector<uint32> TopLevelAccelerationStructure<Platform::VULKAN>::GetPrimitiveCounts() const
{
    return { uint32(m_blas.Size()) };
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::Create(
    Device<Platform::VULKAN> *device,
    Instance<Platform::VULKAN> *instance
)
{
    AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);

    if (m_blas.Empty()) {
        return HYP_MAKE_ERROR(RendererError, "Top level acceleration structure must have at least one BLAS");
    }

    HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer(instance));

    RTUpdateStateFlags update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

    RendererResult result;
    
    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            instance,
            GetType(),
            GetGeometries(instance),
            GetPrimitiveCounts(),
            false,
            update_state_flags
        ),
        result
    );

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    AssertThrow(update_state_flags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    HYPERION_PASS_ERRORS(
        CreateMeshDescriptionsBuffer(
            instance
        ),
        result
    );

    update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    return result;
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    RendererResult result;

    SafeRelease(std::move(m_mesh_descriptions_buffer));
    SafeRelease(std::move(m_scratch_buffer));
    SafeRelease(std::move(m_blas));

    HYPERION_PASS_ERRORS(
        AccelerationStructure::Destroy(device),
        result
    );

    return result;
}

template <>
void TopLevelAccelerationStructure<Platform::VULKAN>::AddBLAS(const BLASRef<Platform::VULKAN> &blas)
{
    AssertThrow(blas != nullptr);
    AssertThrow(!blas->GetGeometries().Empty());

    for (const auto &geometry : blas->GetGeometries()) {
        AssertThrow(geometry != nullptr);
        AssertThrow(geometry->GetPackedVertexStorageBuffer() != nullptr);
        AssertThrow(geometry->GetPackedIndexStorageBuffer() != nullptr);
    }

    m_blas.PushBack(blas);

    SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
}

template <>
void TopLevelAccelerationStructure<Platform::VULKAN>::RemoveBLAS(const BLASRef<Platform::VULKAN> &blas)
{
    auto it = std::find(m_blas.begin(), m_blas.end(), blas);

    if (it != m_blas.end()) {
        m_blas.Erase(it);

        SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    }
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::CreateOrRebuildInstancesBuffer(Instance<Platform::VULKAN> *instance)
{
    Threads::AssertOnThread(g_render_thread);

    Device<Platform::VULKAN> *device = instance->GetDevice();

    Array<VkAccelerationStructureInstanceKHR> instances;
    instances.Resize(m_blas.Size());

    for (SizeType i = 0; i < m_blas.Size(); i++) {
        const BLASRef<Platform::VULKAN> &blas = m_blas[i];
        AssertThrow(blas != nullptr);

        const uint32 instance_index = uint32(i); /* Index of mesh in mesh descriptions buffer. */

        instances[i] = VkAccelerationStructureInstanceKHR {
            .transform                              = ToVkTransform(blas->GetTransform()),
            .instanceCustomIndex                    = instance_index,
            .mask                                   = 0xff,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR,
            .accelerationStructureReference         = blas->GetDeviceAddress()
        };
    }

    constexpr SizeType min_instances_buffer_size = sizeof(VkAccelerationStructureInstanceKHR);

    const SizeType instances_buffer_size = MathUtil::Max(
        min_instances_buffer_size,
        instances.Size() * sizeof(VkAccelerationStructureInstanceKHR)
    );

    AssertThrow(m_instances_buffer.IsValid());

    if (m_instances_buffer->IsCreated() && m_instances_buffer->Size() != instances_buffer_size) {
        DebugLog(
            LogType::Debug,
            "Resizing TLAS instances buffer from %llu to %llu\n",
            m_instances_buffer->Size(),
            instances_buffer_size
        );

        // prevent crash while device is using buffer
        HYPERION_BUBBLE_ERRORS(device->Wait());

        HYPERION_BUBBLE_ERRORS(m_instances_buffer->Destroy(device));
    }

    if (!m_instances_buffer->IsCreated()) {
        HYPERION_BUBBLE_ERRORS(m_instances_buffer->Create(device, instances_buffer_size));
    }

    if (instances.Empty()) {
        // zero out the buffer
        m_instances_buffer->Memset(device, instances_buffer_size, 0x00);
    } else {
        m_instances_buffer->Copy(device, instances_buffer_size, instances.Data());
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::UpdateInstancesBuffer(Instance<Platform::VULKAN> *instance, uint32 first, uint32 last)
{
    if (last == first) {
        HYPERION_RETURN_OK; 
    }

    AssertThrow(last > first);
    AssertThrow(first < m_blas.Size());
    AssertThrow(last <= m_blas.Size());

    Device<Platform::VULKAN> *device = instance->GetDevice();

    Array<VkAccelerationStructureInstanceKHR> instances;
    instances.Resize(last - first);

    for (uint32 i = first; i < last; i++) {
        const BLASRef<Platform::VULKAN> &blas = m_blas[i];
        AssertThrow(blas != nullptr);

        const uint32 instance_index = i; /* Index of mesh in mesh descriptions buffer. */

        instances[i - first] = VkAccelerationStructureInstanceKHR {
            .transform = ToVkTransform(blas->GetTransform()),
            .instanceCustomIndex = instance_index,
            .mask = 0xff,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blas->GetDeviceAddress()
        };
    }

    const SizeType instances_size = instances.Size() * sizeof(VkAccelerationStructureInstanceKHR);

    AssertThrow(m_instances_buffer != nullptr);
    AssertThrow(m_instances_buffer->Size() >= instances_size);

    m_instances_buffer->Copy(
        device,
        first * sizeof(VkAccelerationStructureInstanceKHR),
        instances_size,
        instances.Data()
    );

    HYPERION_RETURN_OK;
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::CreateMeshDescriptionsBuffer(Instance<Platform::VULKAN> *instance)
{
    Device<Platform::VULKAN> *device = instance->GetDevice();

    constexpr SizeType min_mesh_descriptions_buffer_size = sizeof(MeshDescription);

    const SizeType mesh_descriptions_buffer_size = MathUtil::Max(
        min_mesh_descriptions_buffer_size,
        sizeof(MeshDescription) * m_blas.Size()
    );

    HYPERION_BUBBLE_ERRORS(m_mesh_descriptions_buffer->Create(
        device,
        mesh_descriptions_buffer_size
    ));

    // zero out buffer
    m_mesh_descriptions_buffer->Memset(device, m_mesh_descriptions_buffer->Size(), 0x0);

    if (m_blas.Empty()) {
        // no need to update the data inside
        HYPERION_RETURN_OK;
    }

    return UpdateMeshDescriptionsBuffer(instance);
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::UpdateMeshDescriptionsBuffer(Instance<Platform::VULKAN> *instance)
{
    return UpdateMeshDescriptionsBuffer(instance, 0u, uint32(m_blas.Size()));
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::UpdateMeshDescriptionsBuffer(Instance<Platform::VULKAN> *instance, uint32 first, uint32 last)
{
    AssertThrow(m_mesh_descriptions_buffer != nullptr);
    AssertThrow(m_mesh_descriptions_buffer->Size() >= sizeof(MeshDescription) * m_blas.Size());
    AssertThrow(first < m_blas.Size());
    AssertThrow(last <= m_blas.Size());

    if (last <= first) {
        // nothing to update
        return RendererResult { };
    }

    Device<Platform::VULKAN> *device = instance->GetDevice();

    Array<MeshDescription> mesh_descriptions;
    mesh_descriptions.Resize(last - first);

    for (uint32 i = first; i < last; i++) {
        const BLASRef<Platform::VULKAN> &blas = m_blas[i];

        MeshDescription &mesh_description = mesh_descriptions[i - first];
        Memory::MemSet(&mesh_description, 0, sizeof(MeshDescription));

        if (blas->GetGeometries().Empty()) {
            AssertThrowMsg(
                false,
                "No geometries added to BLAS node %u!\n",
                i
            );

            mesh_description = { };
        } else {
            const WeakHandle<Entity> &entity_weak = blas->GetGeometries()[0]->GetEntity();
            const Handle<Material> &material = blas->GetGeometries()[0]->GetMaterial();

            if (material.IsValid()) {
                // Must be initialized (AccelerationGeometry calls Claim() and Unclaim())
                AssertThrow(material->GetRenderResource().GetBufferIndex() != ~0u);
            }

            mesh_description.vertex_buffer_address = blas->GetGeometries()[0]->GetPackedVertexStorageBuffer()->GetBufferDeviceAddress(device);
            mesh_description.index_buffer_address = blas->GetGeometries()[0]->GetPackedIndexStorageBuffer()->GetBufferDeviceAddress(device);
            mesh_description.material_index = material.IsValid() ? material->GetRenderResource().GetBufferIndex() : ~0u;
            mesh_description.num_indices = uint32(blas->GetGeometries()[0]->GetPackedIndices().Size());
            mesh_description.num_vertices = uint32(blas->GetGeometries()[0]->GetPackedVertices().Size());
        }
    }

    m_mesh_descriptions_buffer->Copy(
        device,
        first * sizeof(MeshDescription),
        mesh_descriptions.Size() * sizeof(MeshDescription),
        mesh_descriptions.Data()
    );

    return RendererResult { };
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::RebuildMeshDescriptionsBuffer(Instance<Platform::VULKAN> *instance)
{
    //SafeRelease(std::move(m_mesh_descriptions_buffer));
    
    HYPERION_BUBBLE_ERRORS(m_mesh_descriptions_buffer->Destroy(instance->GetDevice()));

    return CreateMeshDescriptionsBuffer(instance);
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::UpdateStructure(Instance<Platform::VULKAN> *instance, RTUpdateStateFlags &out_update_state_flags)
{
    out_update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING) {
        return Rebuild(instance, out_update_state_flags);
    }

    Range<uint32> dirty_range { };

    for (uint32 i = 0; i < uint32(m_blas.Size()); i++) {
        const BLASRef<Platform::VULKAN> &blas = m_blas[i];
        AssertThrow(blas != nullptr);

        if (blas->GetFlags() & ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE) {
            dirty_range |= Range { i, i + 1 };

            blas->ClearFlag(ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
        }

        RTUpdateStateFlags blas_update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;
        HYPERION_BUBBLE_ERRORS(blas->UpdateStructure(instance, blas_update_state_flags));

        if (blas_update_state_flags) {
            dirty_range |= Range { i, i + 1 };
        }
    }

    if (dirty_range) {
        out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS | RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;

        // update data in instances buffer
        HYPERION_BUBBLE_ERRORS(UpdateInstancesBuffer(
            instance,
            dirty_range.GetStart(),
            dirty_range.GetEnd()
        ));

        // copy mesh descriptions
        HYPERION_BUBBLE_ERRORS(UpdateMeshDescriptionsBuffer(
            instance,
            dirty_range.GetStart(),
            dirty_range.GetEnd()
        ));
        
        HYPERION_BUBBLE_ERRORS(CreateAccelerationStructure(
            instance,
            GetType(),
            GetGeometries(instance),
            GetPrimitiveCounts(),
            true,
            out_update_state_flags
        ));
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult TopLevelAccelerationStructure<Platform::VULKAN>::Rebuild(Instance<Platform::VULKAN> *instance, RTUpdateStateFlags &out_update_state_flags)
{
    AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

    RendererResult result;

    Device<Platform::VULKAN> *device = instance->GetDevice();

    // check each BLAS, assert that it is valid.
    for (const BLASRef<Platform::VULKAN> &blas : m_blas) {
        AssertThrow(blas != nullptr);
        AssertThrow(!blas->GetGeometries().Empty());

        for (const AccelerationGeometryRef<Platform::VULKAN> &geometry : blas->GetGeometries()) {
            AssertThrow(geometry != nullptr);
            AssertThrow(geometry->GetPackedVertexStorageBuffer() != nullptr);
            AssertThrow(geometry->GetPackedIndexStorageBuffer() != nullptr);
        }
    }

    HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer(instance));

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            instance,
            GetType(),
            GetGeometries(instance),
            GetPrimitiveCounts(),
            true,
            out_update_state_flags
        ),
        result
    );

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    HYPERION_PASS_ERRORS(
        RebuildMeshDescriptionsBuffer(
            instance
        ),
        result
    );

    out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    return result;
}

#pragma endregion TopLevelAccelerationStructure

#pragma region BottomLevelAccelerationStructure

template <>
BottomLevelAccelerationStructure<Platform::VULKAN>::BottomLevelAccelerationStructure(const Matrix4 &transform)
    : AccelerationStructure(transform)
{
    m_instances_buffer.SetName(NAME("BLASInstancesBuffer"));
}

template <>
BottomLevelAccelerationStructure<Platform::VULKAN>::~BottomLevelAccelerationStructure() = default;

template <>
RendererResult BottomLevelAccelerationStructure<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, Instance<Platform::VULKAN> *instance)
{
    RendererResult result;

    std::vector<VkAccelerationStructureGeometryKHR> geometries(m_geometries.Size());
    std::vector<uint32> primitive_counts(m_geometries.Size());

    if (m_geometries.Empty()) {
        return HYP_MAKE_ERROR(RendererError, "Cannot create BLAS with zero geometries");
    }

    for (SizeType i = 0; i < m_geometries.Size(); i++) {
        const AccelerationGeometryRef<Platform::VULKAN> &geometry = m_geometries[i];

        if (geometry->GetPackedIndices().Size() % 3 != 0) {
            return HYP_MAKE_ERROR(RendererError, "Invalid triangle mesh!");
        }

        if (!geometry->IsCreated()) {
            HYPERION_PASS_ERRORS(geometry->Create(device, instance), result);
        }

        if (result) {
            geometries[i] = geometry->m_geometry;
            primitive_counts[i] = uint32(geometry->GetPackedIndices().Size() / 3);

            if (primitive_counts[i] == 0) {
                HYPERION_IGNORE_ERRORS(Destroy(device));

                return HYP_MAKE_ERROR(RendererError, "Cannot create BLAS -- geometry has zero indices");
            }
        } else {
            HYPERION_IGNORE_ERRORS(Destroy(device));

            return result;
        }
    }

    RTUpdateStateFlags update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            instance,
            GetType(),
            geometries,
            primitive_counts,
            false,
            update_state_flags
        ),
        result
    );

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    AssertThrow(update_state_flags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    return result;
}

template <>
RendererResult BottomLevelAccelerationStructure<Platform::VULKAN>::UpdateStructure(Instance<Platform::VULKAN> *instance, RTUpdateStateFlags &out_update_state_flags)
{
    out_update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;
    
    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE) {
        out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM;

        ClearFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);
    }

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING) {
        return Rebuild(instance, out_update_state_flags);
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult BottomLevelAccelerationStructure<Platform::VULKAN>::Rebuild(Instance<Platform::VULKAN> *instance, RTUpdateStateFlags &out_update_state_flags)
{
    RendererResult result;
    Device<Platform::VULKAN> *device = instance->GetDevice();

    std::vector<VkAccelerationStructureGeometryKHR> geometries(m_geometries.Size());
    std::vector<uint32> primitive_counts(m_geometries.Size());

    for (SizeType i = 0; i < m_geometries.Size(); i++) {
        const AccelerationGeometryRef<Platform::VULKAN> &geometry = m_geometries[i];
        AssertThrow(geometry != nullptr);

        if (!geometry->GetPackedIndices().Empty()) {
            return HYP_MAKE_ERROR(RendererError, "No mesh data set on geometry!");
        }

        geometries[i] = geometry->m_geometry;
        primitive_counts[i] = uint32(geometry->GetPackedIndices().Size() / 3);
    }

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            instance,
            GetType(),
            geometries,
            primitive_counts,
            true,
            out_update_state_flags
        ),
        result
    );

    if (!result) {
        HYPERION_IGNORE_ERRORS(Destroy(device));

        return result;
    }

    m_flags &= ~(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING | ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);

    return result;
}

#pragma endregion BottomLevelAccelerationStructure

} // namespace platform
} // namespace renderer
} // namespace hyperion
