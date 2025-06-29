/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/rt/RendererAccelerationStructure.hpp>
#include <rendering/backend/vulkan/RendererFence.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <rendering/RenderMaterial.hpp>

// @TODO: Refactor to not need this include
#include <scene/Material.hpp>

#include <core/utilities/Range.hpp>
#include <core/math/MathUtil.hpp>

namespace hyperion {

extern IRenderBackend* g_render_backend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_render_backend);
}

static VkTransformMatrixKHR ToVkTransform(const Matrix4& matrix)
{
    VkTransformMatrixKHR transform;
    std::memcpy(&transform, matrix.values, sizeof(VkTransformMatrixKHR));

    return transform;
}

static VkAccelerationStructureTypeKHR ToVkAccelerationStructureType(AccelerationStructureType type)
{
    switch (type)
    {
    case AccelerationStructureType::BOTTOM_LEVEL:
        return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    case AccelerationStructureType::TOP_LEVEL:
        return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    default:
        return VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;
    }
}

#pragma region VulkanAccelerationGeometry

VulkanAccelerationGeometry::VulkanAccelerationGeometry(
    const VulkanGpuBufferRef& packed_vertices_buffer,
    const VulkanGpuBufferRef& packed_indices_buffer,
    const Handle<Material>& material)
    : m_packed_vertices_buffer(packed_vertices_buffer),
      m_packed_indices_buffer(packed_indices_buffer),
      m_material(material),
      m_geometry {}
{
}

VulkanAccelerationGeometry::~VulkanAccelerationGeometry()
{
}

bool VulkanAccelerationGeometry::IsCreated() const
{
    return true;
}

RendererResult VulkanAccelerationGeometry::Create()
{
    if (m_material.IsValid())
    {
        m_material->GetRenderResource().IncRef();
    }

    if (!GetRenderBackend()->GetDevice()->GetFeatures().IsRaytracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support raytracing");
    }

    if (!m_packed_vertices_buffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not valid");
    }

    if (!m_packed_vertices_buffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed vertices buffer is not created");
    }

    if (!m_packed_indices_buffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not valid");
    }

    if (!m_packed_indices_buffer->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Packed indices buffer is not created");
    }

    VkDeviceOrHostAddressConstKHR vertices_address {
        .deviceAddress = m_packed_vertices_buffer->GetBufferDeviceAddress()
    };

    VkDeviceOrHostAddressConstKHR indices_address {
        .deviceAddress = m_packed_indices_buffer->GetBufferDeviceAddress()
    };

    m_geometry = VkAccelerationStructureGeometryKHR { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    m_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    m_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    m_geometry.geometry = {
        .triangles = VkAccelerationStructureGeometryTrianglesDataKHR {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = vertices_address,
            .vertexStride = sizeof(PackedVertex),
            .maxVertex = uint32(m_packed_vertices_buffer->Size() / sizeof(PackedVertex)),
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = indices_address,
            .transformData = { {} } }
    };

    HYPERION_RETURN_OK;
}

RendererResult VulkanAccelerationGeometry::Destroy()
{
    if (m_material.IsValid())
    {
        m_material->GetRenderResource().DecRef();
    }

    RendererResult result;

    SafeRelease(std::move(m_packed_vertices_buffer));
    SafeRelease(std::move(m_packed_indices_buffer));

    return result;
}

#pragma endregion VulkanAccelerationGeometry

#pragma region AccelerationStructure

VulkanAccelerationStructureBase::VulkanAccelerationStructureBase(const Matrix4& transform)
    : m_transform(transform),
      m_acceleration_structure(VK_NULL_HANDLE),
      m_device_address(0),
      m_flags(ACCELERATION_STRUCTURE_FLAGS_NONE)
{
}

VulkanAccelerationStructureBase::~VulkanAccelerationStructureBase()
{
    AssertThrowMsg(
        m_acceleration_structure == VK_NULL_HANDLE,
        "Expected acceleration structure to have been destroyed before destructor call");

    AssertThrowMsg(
        m_buffer == nullptr,
        "Acceleration structure buffer should have been destroyed before destructor call");

    AssertThrowMsg(
        m_scratch_buffer == nullptr,
        "Scratch buffer should have been destroyed before destructor call");
}

RendererResult VulkanAccelerationStructureBase::CreateAccelerationStructure(
    AccelerationStructureType type,
    const std::vector<VkAccelerationStructureGeometryKHR>& geometries,
    const std::vector<uint32>& primitive_counts,
    bool update,
    RTUpdateStateFlags& out_update_state_flags)
{
    if (update)
    {
        AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

        // TEMP: we should have two TLASes and two RT descriptor sets I suppose...
        // HYPERION_BUBBLE_ERRORS(instance->GetDevice()->Wait());
    }
    else
    {
        AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);
    }

    if (!GetRenderBackend()->GetDevice()->GetFeatures().IsRaytracingSupported())
    {
        return HYP_MAKE_ERROR(RendererError, "Device does not support raytracing");
    }

    if (geometries.empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Geometries empty");
    }

    RendererResult result;

    VkAccelerationStructureBuildGeometryInfoKHR geometry_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    geometry_info.type = ToVkAccelerationStructureType(type);
    geometry_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    geometry_info.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometry_info.geometryCount = uint32(geometries.size());
    geometry_info.pGeometries = geometries.data();

    AssertThrow(primitive_counts.size() == geometries.size());

    VkAccelerationStructureBuildSizesInfoKHR build_sizes_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    GetRenderBackend()->GetDevice()->GetFeatures().dyn_functions.vkGetAccelerationStructureBuildSizesKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &geometry_info,
        primitive_counts.data(),
        &build_sizes_info);

    const SizeType scratch_buffer_alignment = GetRenderBackend()->GetDevice()->GetFeatures().GetAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
    SizeType acceleration_structure_size = MathUtil::NextMultiple(build_sizes_info.accelerationStructureSize, 256ull);
    SizeType build_scratch_size = MathUtil::NextMultiple(build_sizes_info.buildScratchSize, scratch_buffer_alignment);
    SizeType update_scratch_size = MathUtil::NextMultiple(build_sizes_info.updateScratchSize, scratch_buffer_alignment);

    bool was_rebuilt = false;

    if (!m_buffer)
    {
        m_buffer = MakeRenderObject<VulkanGpuBuffer>(GpuBufferType::ACCELERATION_STRUCTURE_BUFFER, acceleration_structure_size);
        HYPERION_PASS_ERRORS(m_buffer->Create(), result);
    }

    HYPERION_BUBBLE_ERRORS(m_buffer->EnsureCapacity(
        acceleration_structure_size,
        &was_rebuilt));

    // force recreate (for debug)
    was_rebuilt = true;

    if (was_rebuilt)
    {
        out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE;

        if (update)
        {
            HYPERION_BUBBLE_ERRORS(GetRenderBackend()->GetDevice()->Wait()); // To prevent deletion while in use

            // delete the current acceleration structure
            GetRenderBackend()->GetDevice()->GetFeatures().dyn_functions.vkDestroyAccelerationStructureKHR(
                GetRenderBackend()->GetDevice()->GetDevice(),
                m_acceleration_structure,
                nullptr);

            m_acceleration_structure = VK_NULL_HANDLE;

            // fetch the corrected acceleration structure and scratch buffer sizes
            // update was true but we need to rebuild from scratch, have to unset the UPDATE flag.
            geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

            GetRenderBackend()->GetDevice()->GetFeatures().dyn_functions.vkGetAccelerationStructureBuildSizesKHR(
                GetRenderBackend()->GetDevice()->GetDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &geometry_info,
                primitive_counts.data(),
                &build_sizes_info);

            acceleration_structure_size = MathUtil::NextMultiple(build_sizes_info.accelerationStructureSize, 256ull);
            build_scratch_size = MathUtil::NextMultiple(build_sizes_info.buildScratchSize, scratch_buffer_alignment);
            update_scratch_size = MathUtil::NextMultiple(build_sizes_info.updateScratchSize, scratch_buffer_alignment);

            AssertThrow(m_buffer->Size() >= acceleration_structure_size);
        }

        // to be sure it's zeroed out
        m_buffer->Memset(acceleration_structure_size, 0);

        VkAccelerationStructureCreateInfoKHR create_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        create_info.buffer = m_buffer->GetVulkanHandle();
        create_info.size = acceleration_structure_size;
        create_info.type = ToVkAccelerationStructureType(type);

        HYPERION_VK_PASS_ERRORS(
            GetRenderBackend()->GetDevice()->GetFeatures().dyn_functions.vkCreateAccelerationStructureKHR(
                GetRenderBackend()->GetDevice()->GetDevice(),
                &create_info,
                VK_NULL_HANDLE,
                &m_acceleration_structure),
            result);

        if (!result)
        {
            HYPERION_IGNORE_ERRORS(Destroy());

            return result;
        }
    }

    AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

    VkAccelerationStructureDeviceAddressInfoKHR address_info { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    address_info.accelerationStructure = m_acceleration_structure;

    m_device_address = GetRenderBackend()->GetDevice()->GetFeatures().dyn_functions.vkGetAccelerationStructureDeviceAddressKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        &address_info);

    const SizeType scratch_size = (update && !was_rebuilt) ? update_scratch_size : build_scratch_size;

    if (m_scratch_buffer == nullptr)
    {
        m_scratch_buffer = MakeRenderObject<VulkanGpuBuffer>(GpuBufferType::SCRATCH_BUFFER, scratch_size, scratch_buffer_alignment);

        HYPERION_PASS_ERRORS(m_scratch_buffer->Create(), result);
    }
    else
    {
        HYPERION_PASS_ERRORS(
            m_scratch_buffer->EnsureCapacity(scratch_size, scratch_buffer_alignment),
            result);
    }

    // zero out scratch buffer
    m_scratch_buffer->Memset(m_scratch_buffer->Size(), 0);

    geometry_info.dstAccelerationStructure = m_acceleration_structure;
    geometry_info.srcAccelerationStructure = (update && !was_rebuilt) ? m_acceleration_structure : VK_NULL_HANDLE;
    geometry_info.scratchData = { .deviceAddress = m_scratch_buffer->GetBufferDeviceAddress() };

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> range_infos;
    range_infos.resize(geometries.size());

    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> range_info_ptrs;
    range_info_ptrs.resize(geometries.size());

    for (SizeType i = 0; i < geometries.size(); i++)
    {
        range_infos[i] = VkAccelerationStructureBuildRangeInfoKHR {
            .primitiveCount = primitive_counts[i],
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };

        range_info_ptrs[i] = &range_infos[i];
    }

    VulkanFenceRef fence = MakeRenderObject<VulkanFence>();

    VulkanCommandBufferRef command_buffer = MakeRenderObject<VulkanCommandBuffer>(CommandBufferType::COMMAND_BUFFER_PRIMARY);
    HYPERION_BUBBLE_ERRORS(command_buffer->Create(GetRenderBackend()->GetDevice()->GetGraphicsQueue().command_pools[0]));

    HYPERION_BUBBLE_ERRORS(command_buffer->Begin());

    GetRenderBackend()->GetDevice()->GetFeatures().dyn_functions.vkCmdBuildAccelerationStructuresKHR(
        command_buffer->GetVulkanHandle(),
        uint32(range_info_ptrs.size()),
        &geometry_info,
        range_info_ptrs.data());

    HYPERION_PASS_ERRORS(command_buffer->End(), result);

    HYPERION_PASS_ERRORS(fence->Create(), result);

    HYPERION_PASS_ERRORS(fence->Reset(), result);

    HYPERION_PASS_ERRORS(command_buffer->SubmitPrimary(&GetRenderBackend()->GetDevice()->GetGraphicsQueue(), fence, nullptr), result);

    HYPERION_PASS_ERRORS(fence->WaitForGPU(), result);
    HYPERION_PASS_ERRORS(fence->Destroy(), result);

    HYPERION_PASS_ERRORS(command_buffer->Destroy(), result);

    ClearFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);

    HYPERION_RETURN_OK;
}

RendererResult VulkanAccelerationStructureBase::Destroy()
{
    RendererResult result;

    SafeRelease(std::move(m_geometries));
    SafeRelease(std::move(m_buffer));
    SafeRelease(std::move(m_scratch_buffer));

    if (m_acceleration_structure != VK_NULL_HANDLE)
    {
        GetRenderBackend()->GetDevice()->GetFeatures().dyn_functions.vkDestroyAccelerationStructureKHR(
            GetRenderBackend()->GetDevice()->GetDevice(),
            m_acceleration_structure,
            VK_NULL_HANDLE);

        m_acceleration_structure = VK_NULL_HANDLE;
    }

    return result;
}

void VulkanAccelerationStructureBase::RemoveGeometry(uint32 index)
{
    const auto it = m_geometries.Begin() + index;

    if (it >= m_geometries.End())
    {
        return;
    }

    SafeRelease(std::move(*it));

    m_geometries.Erase(it);

    SetNeedsRebuildFlag();
}

void VulkanAccelerationStructureBase::RemoveGeometry(const VulkanAccelerationGeometryRef& geometry)
{
    if (geometry == nullptr)
    {
        return;
    }

    const auto it = m_geometries.Find(geometry);

    if (it == m_geometries.End())
    {
        return;
    }

    SafeRelease(std::move(*it));

    m_geometries.Erase(it);

    SetNeedsRebuildFlag();
}

#pragma endregion AccelerationStructure

#pragma region TLAS

VulkanTLAS::VulkanTLAS()
    : VulkanAccelerationStructureBase()
{
}

VulkanTLAS::~VulkanTLAS()
{
    AssertThrowMsg(
        m_instances_buffer == nullptr,
        "Instances buffer should have been destroyed before destructor call");

    AssertThrowMsg(m_acceleration_structure == VK_NULL_HANDLE, "Acceleration structure should have been destroyed before destructor call");
}

std::vector<VkAccelerationStructureGeometryKHR> VulkanTLAS::GetGeometries() const
{
    return {
        { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
            .geometry = {
                .instances = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                    .arrayOfPointers = VK_FALSE,
                    .data = { .deviceAddress = m_instances_buffer->GetBufferDeviceAddress() } } },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR }
    };
}

std::vector<uint32> VulkanTLAS::GetPrimitiveCounts() const
{
    return { uint32(m_blas.Size()) };
}

RendererResult VulkanTLAS::Create()
{
    AssertThrow(m_acceleration_structure == VK_NULL_HANDLE);

    if (m_blas.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Top level acceleration structure must have at least one BLAS");
    }

    HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer());

    RTUpdateStateFlags update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

    RendererResult result;

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            GetType(),
            GetGeometries(),
            GetPrimitiveCounts(),
            false,
            update_state_flags),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    AssertThrow(update_state_flags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    HYPERION_PASS_ERRORS(
        CreateMeshDescriptionsBuffer(),
        result);

    update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    return result;
}

RendererResult VulkanTLAS::Destroy()
{
    RendererResult result;

    SafeRelease(std::move(m_instances_buffer));
    SafeRelease(std::move(m_mesh_descriptions_buffer));
    SafeRelease(std::move(m_scratch_buffer));
    SafeRelease(std::move(m_blas));

    HYPERION_PASS_ERRORS(
        VulkanAccelerationStructureBase::Destroy(),
        result);

    return result;
}

void VulkanTLAS::AddBLAS(const BLASRef& blas)
{
    AssertThrow(blas != nullptr);

    VulkanBLASRef vulkan_blas { blas };

    AssertThrow(!vulkan_blas->GetGeometries().Empty());

    for (const auto& geometry : vulkan_blas->GetGeometries())
    {
        AssertThrow(geometry != nullptr);
        AssertThrow(geometry->GetPackedVerticesBuffer() != nullptr);
        AssertThrow(geometry->GetPackedIndicesBuffer() != nullptr);
    }

    m_blas.PushBack(std::move(vulkan_blas));

    SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
}

void VulkanTLAS::RemoveBLAS(const BLASRef& blas)
{
    auto it = std::find(m_blas.begin(), m_blas.end(), blas);

    if (it != m_blas.end())
    {
        m_blas.Erase(it);

        SetFlag(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING);
    }
}

RendererResult VulkanTLAS::CreateOrRebuildInstancesBuffer()
{
    Threads::AssertOnThread(g_render_thread);

    Array<VkAccelerationStructureInstanceKHR> instances;
    instances.Resize(m_blas.Size());

    for (SizeType i = 0; i < m_blas.Size(); i++)
    {
        const VulkanBLASRef& blas = m_blas[i];
        AssertThrow(blas != nullptr);

        const uint32 instance_index = uint32(i); /* Index of mesh in mesh descriptions buffer. */

        instances[i] = VkAccelerationStructureInstanceKHR {
            .transform = ToVkTransform(blas->GetTransform()),
            .instanceCustomIndex = instance_index,
            .mask = 0xff,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR,
            .accelerationStructureReference = blas->GetDeviceAddress()
        };
    }

    constexpr SizeType min_instances_buffer_size = sizeof(VkAccelerationStructureInstanceKHR);

    const SizeType instances_buffer_size = MathUtil::Max(
        min_instances_buffer_size,
        instances.Size() * sizeof(VkAccelerationStructureInstanceKHR));

    if (m_instances_buffer && m_instances_buffer->Size() != instances_buffer_size)
    {
        DebugLog(
            LogType::Debug,
            "Resizing TLAS instances buffer from %llu to %llu\n",
            m_instances_buffer->Size(),
            instances_buffer_size);

        SafeRelease(std::move(m_instances_buffer));
    }

    if (!m_instances_buffer)
    {
        m_instances_buffer = MakeRenderObject<VulkanGpuBuffer>(GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER, instances_buffer_size);
    }

    if (!m_instances_buffer->IsCreated())
    {
        HYPERION_BUBBLE_ERRORS(m_instances_buffer->Create());
    }

    if (instances.Empty())
    {
        // zero out the buffer
        m_instances_buffer->Memset(instances_buffer_size, 0);
    }
    else
    {
        m_instances_buffer->Copy(instances_buffer_size, instances.Data());
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanTLAS::UpdateInstancesBuffer(uint32 first, uint32 last)
{
    if (last == first)
    {
        HYPERION_RETURN_OK;
    }

    AssertThrow(last > first);
    AssertThrow(first < m_blas.Size());
    AssertThrow(last <= m_blas.Size());

    Array<VkAccelerationStructureInstanceKHR> instances;
    instances.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        const VulkanBLASRef& blas = m_blas[i];
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
        first * sizeof(VkAccelerationStructureInstanceKHR),
        instances_size,
        instances.Data());

    HYPERION_RETURN_OK;
}

RendererResult VulkanTLAS::CreateMeshDescriptionsBuffer()
{
    constexpr SizeType min_mesh_descriptions_buffer_size = sizeof(MeshDescription);

    const SizeType mesh_descriptions_buffer_size = MathUtil::Max(
        min_mesh_descriptions_buffer_size,
        sizeof(MeshDescription) * m_blas.Size());

    m_mesh_descriptions_buffer = MakeRenderObject<VulkanGpuBuffer>(GpuBufferType::SSBO, mesh_descriptions_buffer_size);
    HYPERION_BUBBLE_ERRORS(m_mesh_descriptions_buffer->Create());

    // zero out buffer
    m_mesh_descriptions_buffer->Memset(m_mesh_descriptions_buffer->Size(), 0x0);

    if (m_blas.Empty())
    {
        // no need to update the data inside
        HYPERION_RETURN_OK;
    }

    return UpdateMeshDescriptionsBuffer();
}

RendererResult VulkanTLAS::UpdateMeshDescriptionsBuffer()
{
    return UpdateMeshDescriptionsBuffer(0u, uint32(m_blas.Size()));
}

RendererResult VulkanTLAS::UpdateMeshDescriptionsBuffer(uint32 first, uint32 last)
{
    AssertThrow(m_mesh_descriptions_buffer != nullptr);
    AssertThrow(m_mesh_descriptions_buffer->Size() >= sizeof(MeshDescription) * m_blas.Size());
    AssertThrow(first < m_blas.Size());
    AssertThrow(last <= m_blas.Size());

    if (last <= first)
    {
        // nothing to update
        return RendererResult {};
    }

    Array<MeshDescription> mesh_descriptions;
    mesh_descriptions.Resize(last - first);

    for (uint32 i = first; i < last; i++)
    {
        const VulkanBLASRef& blas = m_blas[i];

        MeshDescription& mesh_description = mesh_descriptions[i - first];
        Memory::MemSet(&mesh_description, 0, sizeof(MeshDescription));

        if (blas->GetGeometries().Empty())
        {
            AssertThrowMsg(
                false,
                "No geometries added to BLAS node %u!\n",
                i);

            mesh_description = {};
        }
        else
        {
            const Handle<Material>& material = blas->GetGeometries()[0]->GetMaterial();

            if (material.IsValid())
            {
                // Must be initialized (AccelerationGeometry calls IncRef() and DecRef())
                AssertThrow(material->GetRenderResource().GetBufferIndex() != ~0u);
            }

            mesh_description.vertex_buffer_address = blas->GetGeometries()[0]->GetPackedVerticesBuffer()->GetBufferDeviceAddress();
            mesh_description.index_buffer_address = blas->GetGeometries()[0]->GetPackedIndicesBuffer()->GetBufferDeviceAddress();
            mesh_description.material_index = material.IsValid() ? material->GetRenderResource().GetBufferIndex() : ~0u;
            mesh_description.num_indices = uint32(blas->GetGeometries()[0]->GetPackedIndicesBuffer()->Size() / sizeof(uint32));
            mesh_description.num_vertices = uint32(blas->GetGeometries()[0]->GetPackedVerticesBuffer()->Size() / sizeof(PackedVertex));
        }
    }

    m_mesh_descriptions_buffer->Copy(
        first * sizeof(MeshDescription),
        mesh_descriptions.Size() * sizeof(MeshDescription),
        mesh_descriptions.Data());

    return RendererResult {};
}

RendererResult VulkanTLAS::RebuildMeshDescriptionsBuffer()
{
    SafeRelease(std::move(m_mesh_descriptions_buffer));

    return CreateMeshDescriptionsBuffer();
}

RendererResult VulkanTLAS::UpdateStructure(RTUpdateStateFlags& out_update_state_flags)
{
    out_update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(out_update_state_flags);
    }

    Range<uint32> dirty_range {};

    for (uint32 i = 0; i < uint32(m_blas.Size()); i++)
    {
        const VulkanBLASRef& blas = m_blas[i];
        AssertThrow(blas != nullptr);

        if (blas->GetFlags() & ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE)
        {
            dirty_range |= Range { i, i + 1 };

            blas->ClearFlag(ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE);
        }

        RTUpdateStateFlags blas_update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;
        HYPERION_BUBBLE_ERRORS(blas->UpdateStructure(blas_update_state_flags));

        if (blas_update_state_flags)
        {
            dirty_range |= Range { i, i + 1 };
        }
    }

    if (dirty_range)
    {
        out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS | RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES;

        // update data in instances buffer
        HYPERION_BUBBLE_ERRORS(UpdateInstancesBuffer(
            dirty_range.GetStart(),
            dirty_range.GetEnd()));

        // copy mesh descriptions
        HYPERION_BUBBLE_ERRORS(UpdateMeshDescriptionsBuffer(
            dirty_range.GetStart(),
            dirty_range.GetEnd()));

        HYPERION_BUBBLE_ERRORS(CreateAccelerationStructure(
            GetType(),
            GetGeometries(),
            GetPrimitiveCounts(),
            true,
            out_update_state_flags));
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanTLAS::Rebuild(RTUpdateStateFlags& out_update_state_flags)
{
    AssertThrow(m_acceleration_structure != VK_NULL_HANDLE);

    RendererResult result;

    // check each BLAS, assert that it is valid.
    for (const VulkanBLASRef& blas : m_blas)
    {
        AssertThrow(blas != nullptr);
        AssertThrow(!blas->GetGeometries().Empty());

        for (const VulkanAccelerationGeometryRef& geometry : blas->GetGeometries())
        {
            AssertThrow(geometry != nullptr);
            AssertThrow(geometry->GetPackedVerticesBuffer() != nullptr);
            AssertThrow(geometry->GetPackedIndicesBuffer() != nullptr);
        }
    }

    HYPERION_BUBBLE_ERRORS(CreateOrRebuildInstancesBuffer());

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            GetType(),
            GetGeometries(),
            GetPrimitiveCounts(),
            true,
            out_update_state_flags),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    HYPERION_PASS_ERRORS(
        RebuildMeshDescriptionsBuffer(),
        result);

    out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS;

    return result;
}

#pragma endregion TLAS

#pragma region BLAS

VulkanBLAS::VulkanBLAS(
    const VulkanGpuBufferRef& packed_vertices_buffer,
    const VulkanGpuBufferRef& packed_indices_buffer,
    const Handle<Material>& material,
    const Matrix4& transform)
    : VulkanAccelerationStructureBase(transform),
      m_packed_vertices_buffer(packed_vertices_buffer),
      m_packed_indices_buffer(packed_indices_buffer),
      m_material(material)
{
    m_geometries.PushBack(MakeRenderObject<VulkanAccelerationGeometry>(
        m_packed_vertices_buffer,
        m_packed_indices_buffer,
        m_material));
}

VulkanBLAS::~VulkanBLAS() = default;

RendererResult VulkanBLAS::Create()
{
    RendererResult result;

    std::vector<VkAccelerationStructureGeometryKHR> geometries(m_geometries.Size());
    std::vector<uint32> primitive_counts(m_geometries.Size());

    if (m_geometries.Empty())
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create BLAS with zero geometries");
    }

    for (SizeType i = 0; i < m_geometries.Size(); i++)
    {
        const VulkanAccelerationGeometryRef& geometry = m_geometries[i];

        if (!geometry->IsCreated())
        {
            HYPERION_PASS_ERRORS(geometry->Create(), result);
        }

        if (result)
        {
            geometries[i] = geometry->m_geometry;
            primitive_counts[i] = uint32(geometry->GetPackedIndicesBuffer()->Size() / sizeof(uint32) / 3);

            if (primitive_counts[i] == 0)
            {
                HYPERION_IGNORE_ERRORS(Destroy());

                return HYP_MAKE_ERROR(RendererError, "Cannot create BLAS -- geometry has zero indices");
            }
        }
        else
        {
            HYPERION_IGNORE_ERRORS(Destroy());

            return result;
        }
    }

    RTUpdateStateFlags update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            GetType(),
            geometries,
            primitive_counts,
            false,
            update_state_flags),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    AssertThrow(update_state_flags & RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE);

    return result;
}

RendererResult VulkanBLAS::Destroy()
{
    SafeRelease(std::move(m_packed_vertices_buffer));
    SafeRelease(std::move(m_packed_indices_buffer));

    return VulkanAccelerationStructureBase::Destroy();
}

RendererResult VulkanBLAS::UpdateStructure(RTUpdateStateFlags& out_update_state_flags)
{
    out_update_state_flags = RT_UPDATE_STATE_FLAGS_NONE;

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE)
    {
        out_update_state_flags |= RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM;

        ClearFlag(ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);
    }

    if (m_flags & ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING)
    {
        return Rebuild(out_update_state_flags);
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanBLAS::Rebuild(RTUpdateStateFlags& out_update_state_flags)
{
    RendererResult result;

    std::vector<VkAccelerationStructureGeometryKHR> geometries(m_geometries.Size());
    std::vector<uint32> primitive_counts(m_geometries.Size());

    for (SizeType i = 0; i < m_geometries.Size(); i++)
    {
        const VulkanAccelerationGeometryRef& geometry = m_geometries[i];
        AssertThrow(geometry != nullptr);

        geometries[i] = geometry->m_geometry;
        primitive_counts[i] = uint32(geometry->GetPackedIndicesBuffer()->Size() / sizeof(uint32) / 3);
    }

    HYPERION_PASS_ERRORS(
        CreateAccelerationStructure(
            GetType(),
            geometries,
            primitive_counts,
            true,
            out_update_state_flags),
        result);

    if (!result)
    {
        HYPERION_IGNORE_ERRORS(Destroy());

        return result;
    }

    m_flags &= ~(ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING | ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE);

    return result;
}

#pragma endregion BLAS

} // namespace hyperion
