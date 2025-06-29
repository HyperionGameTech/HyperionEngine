/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererGpuBuffer.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererInstance.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/math/MathUtil.hpp>

#include <Engine.hpp>

#include <cstring>

namespace hyperion {

extern IRenderBackend* g_render_backend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_render_backend);
}

#pragma region Helpers

static uint32 FindMemoryType(uint32 vk_type_filter, VkMemoryPropertyFlags vk_memory_property_flags)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(GetRenderBackend()->GetDevice()->GetPhysicalDevice(), &mem_properties);

    for (uint32 i = 0; i < mem_properties.memoryTypeCount; i++)
    {
        if ((vk_type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & vk_memory_property_flags) == vk_memory_property_flags)
        {
            HYP_LOG(RenderingBackend, Debug, "Found Memory type {}", i);
            return i;
        }
    }

    AssertThrowMsg(false, "Could not find suitable memory type!\n");
}

VkImageLayout GetVkImageLayout(ResourceState state)
{
    switch (state)
    {
    case RS_UNDEFINED:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case RS_PRE_INITIALIZED:
        return VK_IMAGE_LAYOUT_PREINITIALIZED;
    case RS_COMMON:
    case RS_UNORDERED_ACCESS:
        return VK_IMAGE_LAYOUT_GENERAL;
    case RS_RENDER_TARGET:
    case RS_RESOLVE_DST:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case RS_DEPTH_STENCIL:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case RS_SHADER_RESOURCE:
    case RS_RESOLVE_SRC:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case RS_COPY_DST:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case RS_COPY_SRC:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        break;
    case RS_PRESENT:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    default:
        AssertThrowMsg(false, "Unknown resource state");
        return VkImageLayout(-1);
    }
}

VkAccessFlags GetVkAccessMask(ResourceState state)
{
    switch (state)
    {
    case RS_UNDEFINED:
    case RS_PRESENT:
    case RS_COMMON:
    case RS_PRE_INITIALIZED:
        return VkAccessFlagBits(0);
    case RS_VERTEX_BUFFER:
        return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    case RS_CONSTANT_BUFFER:
        return VK_ACCESS_UNIFORM_READ_BIT;
    case RS_INDEX_BUFFER:
        return VK_ACCESS_INDEX_READ_BIT;
    case RS_RENDER_TARGET:
        return VkAccessFlagBits(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    case RS_UNORDERED_ACCESS:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case RS_DEPTH_STENCIL:
        return VkAccessFlagBits(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    case RS_SHADER_RESOURCE:
        return VK_ACCESS_SHADER_READ_BIT;
    case RS_INDIRECT_ARG:
        return VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case RS_COPY_DST:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case RS_COPY_SRC:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case RS_RESOLVE_DST:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case RS_RESOLVE_SRC:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    default:
        AssertThrowMsg(false, "Unknown resource state");
        return VkAccessFlagBits(-1);
    }
}

VkPipelineStageFlags GetVkShaderStageMask(ResourceState state, bool src, ShaderModuleType shader_type = SMT_UNSET)
{
    switch (state)
    {
    case RS_UNDEFINED:
    case RS_PRE_INITIALIZED:
    case RS_COMMON:
        if (!src)
        {
            HYP_LOG(RenderingBackend, Warning, "Attempt to get shader stage mask for resource state but `src` was set to false. Falling back to all commands.");

            return VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case RS_VERTEX_BUFFER:
    case RS_INDEX_BUFFER:
        return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    case RS_UNORDERED_ACCESS:
    case RS_CONSTANT_BUFFER:
    case RS_SHADER_RESOURCE:
        switch (shader_type)
        {
        case SMT_VERTEX:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case SMT_FRAGMENT:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case SMT_COMPUTE:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case SMT_RAY_ANY_HIT:
        case SMT_RAY_CLOSEST_HIT:
        case SMT_RAY_GEN:
        case SMT_RAY_INTERSECT:
        case SMT_RAY_MISS:
            return VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        case SMT_GEOMETRY:
            return VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        case SMT_TESS_CONTROL:
            return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
        case SMT_TESS_EVAL:
            return VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        case SMT_MESH:
            return VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV;
        case SMT_TASK:
            return VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV;
        case SMT_UNSET:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        }
    case RS_RENDER_TARGET:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case RS_DEPTH_STENCIL:
        return src ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    case RS_INDIRECT_ARG:
        return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case RS_COPY_DST:
    case RS_COPY_SRC:
    case RS_RESOLVE_DST:
    case RS_RESOLVE_SRC:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case RS_PRESENT:
        return src ? (VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    default:
        AssertThrowMsg(false, "Unknown resource state");
        return VkPipelineStageFlags(-1);
    }
}

VkBufferUsageFlags GetVkUsageFlags(GpuBufferType type)
{
    switch (type)
    {
    case GpuBufferType::MESH_VERTEX_BUFFER:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    case GpuBufferType::MESH_INDEX_BUFFER:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case GpuBufferType::CBUFF:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case GpuBufferType::SSBO:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GpuBufferType::ATOMIC_COUNTER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::STAGING_BUFFER:
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    case GpuBufferType::INDIRECT_ARGS_BUFFER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GpuBufferType::SHADER_BINDING_TABLE:
        return VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GpuBufferType::RT_MESH_VERTEX_BUFFER:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GpuBufferType::RT_MESH_INDEX_BUFFER:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GpuBufferType::SCRATCH_BUFFER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    default:
        return 0;
    }
}

VmaMemoryUsage GetVkMemoryUsage(GpuBufferType type)
{
    switch (type)
    {
    case GpuBufferType::MESH_VERTEX_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::MESH_INDEX_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::CBUFF:
        return VMA_MEMORY_USAGE_AUTO;
    case GpuBufferType::SSBO:
        return VMA_MEMORY_USAGE_AUTO;
    case GpuBufferType::ATOMIC_COUNTER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::STAGING_BUFFER:
        return VMA_MEMORY_USAGE_CPU_ONLY;
    case GpuBufferType::INDIRECT_ARGS_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::SHADER_BINDING_TABLE:
        return VMA_MEMORY_USAGE_AUTO;
    case GpuBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VMA_MEMORY_USAGE_AUTO;
    case GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VMA_MEMORY_USAGE_AUTO;
    case GpuBufferType::RT_MESH_VERTEX_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::RT_MESH_INDEX_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GpuBufferType::SCRATCH_BUFFER:
        return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    default:
        return VMA_MEMORY_USAGE_UNKNOWN;
    }
}

VmaAllocationCreateFlags GetVkAllocationCreateFlags(GpuBufferType type)
{
    switch (type)
    {
    case GpuBufferType::MESH_VERTEX_BUFFER:
        return 0;
    case GpuBufferType::MESH_INDEX_BUFFER:
        return 0;
    case GpuBufferType::CBUFF:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::SSBO:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::ATOMIC_COUNTER:
        return 0;
    case GpuBufferType::STAGING_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::INDIRECT_ARGS_BUFFER:
        return 0;
    case GpuBufferType::SHADER_BINDING_TABLE:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::RT_MESH_VERTEX_BUFFER:
        return 0;
    case GpuBufferType::RT_MESH_INDEX_BUFFER:
        return 0;
    case GpuBufferType::SCRATCH_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    default:
        HYP_THROW("Invalid gpu buffer type for allocation create flags");
    }
}

#pragma endregion Helpers

#pragma region VulkanGpuBuffer

VulkanGpuBuffer::VulkanGpuBuffer(GpuBufferType type, SizeType size, SizeType alignment)
    : GpuBufferBase(type, size, alignment)
{
}

VulkanGpuBuffer::~VulkanGpuBuffer()
{
    AssertThrowMsg(m_handle == VK_NULL_HANDLE,
        "Buffer should have been destroyed! Debug name: %s",
        GetDebugName().LookupString());
}

void VulkanGpuBuffer::Memset(SizeType count, ubyte value)
{
    if (m_mapping == nullptr)
    {
        Map();
    }

    Memory::MemSet(m_mapping, value, count);
}

void VulkanGpuBuffer::Copy(SizeType count, const void* ptr)
{
    if (m_mapping == nullptr)
    {
        Map();
    }

    Memory::MemCpy(m_mapping, ptr, count);
}

void VulkanGpuBuffer::Copy(SizeType offset, SizeType count, const void* ptr)
{
    if (m_mapping == nullptr)
    {
        Map();
    }

    Memory::MemCpy(reinterpret_cast<void*>(uintptr_t(m_mapping) + offset), ptr, count);
}

void VulkanGpuBuffer::Map() const
{
    if (m_mapping != nullptr)
    {
        return;
    }

    AssertThrowMsg(IsCPUAccessible(), "Attempt to map a buffer that is not CPU accessible!");

    vmaMapMemory(GetRenderBackend()->GetDevice()->GetAllocator(), m_vma_allocation, &m_mapping);
}

void VulkanGpuBuffer::Unmap() const
{
    if (m_mapping == nullptr)
    {
        return;
    }

    vmaUnmapMemory(GetRenderBackend()->GetDevice()->GetAllocator(), m_vma_allocation);
    m_mapping = nullptr;
}

void VulkanGpuBuffer::Read(SizeType count, void* out_ptr) const
{
    if (m_mapping == nullptr)
    {
        Map();

        HYP_LOG(RenderingBackend, Warning, "Attempt to Read() from buffer but data has not been mapped previously");
    }

    Memory::MemCpy(out_ptr, m_mapping, count);
}

void VulkanGpuBuffer::Read(SizeType offset, SizeType count, void* out_ptr) const
{
    if (m_mapping == nullptr)
    {
        Map();

        HYP_LOG(RenderingBackend, Warning, "Attempt to Read() from buffer but data has not been mapped previously");
    }

    Memory::MemCpy(out_ptr, reinterpret_cast<void*>(uintptr_t(m_mapping) + uintptr_t(offset)), count);
}

bool VulkanGpuBuffer::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

bool VulkanGpuBuffer::IsCPUAccessible() const
{
    return m_vma_usage != VMA_MEMORY_USAGE_GPU_ONLY;
}

RendererResult VulkanGpuBuffer::CheckCanAllocate(SizeType size) const
{
    const VkBufferCreateInfo create_info = GetBufferCreateInfo();
    const VmaAllocationCreateInfo alloc_info = GetAllocationCreateInfo();

    return CheckCanAllocate(create_info, alloc_info, m_size);
}

uint64 VulkanGpuBuffer::GetBufferDeviceAddress() const
{
    AssertThrowMsg(
        GetRenderBackend()->GetDevice()->GetFeatures().GetBufferDeviceAddressFeatures().bufferDeviceAddress,
        "Called GetBufferDeviceAddress() but the buffer device address extension feature is not supported or enabled!");

    AssertThrow(m_handle != VK_NULL_HANDLE);

    VkBufferDeviceAddressInfoKHR info { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = m_handle;

    return GetRenderBackend()->GetDevice()->GetFeatures().dyn_functions.vkGetBufferDeviceAddressKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        &info);
}

void VulkanGpuBuffer::InsertBarrier(CommandBufferBase* command_buffer, ResourceState new_state) const
{
    InsertBarrier(
        static_cast<VulkanCommandBuffer*>(command_buffer),
        new_state);
}

void VulkanGpuBuffer::InsertBarrier(CommandBufferBase* command_buffer, ResourceState new_state, ShaderModuleType shader_type) const
{
    InsertBarrier(
        static_cast<VulkanCommandBuffer*>(command_buffer),
        new_state,
        shader_type);
}

void VulkanGpuBuffer::InsertBarrier(
    VulkanCommandBuffer* command_buffer,
    ResourceState new_state) const
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to insert a resource barrier but buffer was not created");

        return;
    }

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = GetVkAccessMask(m_resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.buffer = m_handle;
    barrier.offset = 0;
    barrier.size = m_size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetVulkanHandle(),
        GetVkShaderStageMask(m_resource_state, true),
        GetVkShaderStageMask(new_state, false),
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr);

    m_resource_state = new_state;
}

void VulkanGpuBuffer::InsertBarrier(
    VulkanCommandBuffer* command_buffer,
    ResourceState new_state,
    ShaderModuleType shader_type) const
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to insert a resource barrier but buffer was not created");

        return;
    }

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = GetVkAccessMask(m_resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.buffer = m_handle;
    barrier.offset = 0;
    barrier.size = m_size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetVulkanHandle(),
        GetVkShaderStageMask(m_resource_state, true, shader_type),
        GetVkShaderStageMask(new_state, false, shader_type),
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr);

    m_resource_state = new_state;
}

void VulkanGpuBuffer::CopyFrom(
    CommandBufferBase* command_buffer,
    const GpuBufferBase* src_buffer,
    SizeType count)
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but dst buffer was not created");

        return;
    }

    if (!src_buffer->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but src buffer was not created");

        return;
    }

    InsertBarrier(command_buffer, RS_COPY_DST);
    src_buffer->InsertBarrier(command_buffer, RS_COPY_SRC);

    VkBufferCopy region {};
    region.size = count;

    vkCmdCopyBuffer(
        static_cast<VulkanCommandBuffer*>(command_buffer)->GetVulkanHandle(),
        static_cast<const VulkanGpuBuffer*>(src_buffer)->m_handle,
        m_handle,
        1,
        &region);
}

RendererResult VulkanGpuBuffer::Destroy()
{
    if (!IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    if (m_mapping != nullptr)
    {
        Unmap();
    }

    vmaDestroyBuffer(GetRenderBackend()->GetDevice()->GetAllocator(), m_handle, m_vma_allocation);

    m_handle = VK_NULL_HANDLE;
    m_vma_allocation = VK_NULL_HANDLE;
    m_resource_state = RS_UNDEFINED;

    HYPERION_RETURN_OK;
}

RendererResult VulkanGpuBuffer::Create()
{
    if (IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Create() called on a buffer that has not been destroyed!\n"
                                           "\tYou should explicitly call Destroy() on the object before reallocating it.\n"
                                           "\tTo prevent memory leaks, calling Destroy() before allocating the memory...");

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(false, "Create() called on a buffer that has not been destroyed!");
#endif

        HYPERION_BUBBLE_ERRORS(Destroy());
    }

    m_vk_buffer_usage_flags = GetVkUsageFlags(m_type);
    m_vma_usage = GetVkMemoryUsage(m_type);
    m_vma_allocation_create_flags = GetVkAllocationCreateFlags(m_type)
        | VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;

    if (m_size == 0)
    {
#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(false, "Creating empty gpu buffer will result in errors!");
#endif

        return HYP_MAKE_ERROR(RendererError, "Creating empty gpu buffer will result in errors!");
    }

    const auto create_info = GetBufferCreateInfo();
    const auto alloc_info = GetAllocationCreateInfo();

    HYPERION_BUBBLE_ERRORS(CheckCanAllocate(create_info, alloc_info, m_size));

    if (m_alignment != 0)
    {
        HYPERION_VK_CHECK_MSG(
            vmaCreateBufferWithAlignment(
                GetRenderBackend()->GetDevice()->GetAllocator(),
                &create_info,
                &alloc_info,
                m_alignment,
                &m_handle,
                &m_vma_allocation,
                nullptr),
            "Failed to create aligned gpu buffer!");
    }
    else
    {
        HYPERION_VK_CHECK_MSG(
            vmaCreateBuffer(
                GetRenderBackend()->GetDevice()->GetAllocator(),
                &create_info,
                &alloc_info,
                &m_handle,
                &m_vma_allocation,
                nullptr),
            "Failed to create gpu buffer!");
    }

    if (IsCPUAccessible())
    {
        // Memset all to zero
        Memset(m_size, 0);
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanGpuBuffer::EnsureCapacity(
    SizeType minimum_size,
    SizeType alignment,
    bool* out_size_changed)
{
    if (minimum_size == 0)
    {
        HYPERION_RETURN_OK;
    }

    RendererResult result;

    if (minimum_size <= m_size)
    {
        if (out_size_changed != nullptr)
        {
            *out_size_changed = false;
        }

        HYPERION_RETURN_OK;
    }

    if (m_handle != VK_NULL_HANDLE)
    {
        HYPERION_PASS_ERRORS(Destroy(), result);

        if (!result)
        {
            if (out_size_changed != nullptr)
            {
                *out_size_changed = false;
            }

            return result;
        }
    }

    m_size = minimum_size;
    m_alignment = alignment;

    HYPERION_PASS_ERRORS(Create(), result);

    if (result)
    {
        if (out_size_changed != nullptr)
        {
            *out_size_changed = true;
        }
    }
    else
    {
        m_size = 0;
        m_alignment = 0;
    }

    return result;
}

RendererResult VulkanGpuBuffer::EnsureCapacity(
    SizeType minimum_size,
    bool* out_size_changed)
{
    return EnsureCapacity(minimum_size, 0, out_size_changed);
}

VkBufferCreateInfo VulkanGpuBuffer::GetBufferCreateInfo() const
{
    const QueueFamilyIndices& qf_indices = GetRenderBackend()->GetDevice()->GetQueueFamilyIndices();
    const uint32 buffer_family_indices[] = { qf_indices.graphics_family.Get(), qf_indices.compute_family.Get() };

    VkBufferCreateInfo vk_buffer_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vk_buffer_info.size = m_size;
    vk_buffer_info.usage = m_vk_buffer_usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vk_buffer_info.pQueueFamilyIndices = buffer_family_indices;
    vk_buffer_info.queueFamilyIndexCount = ArraySize(buffer_family_indices);

    return vk_buffer_info;
}

VmaAllocationCreateInfo VulkanGpuBuffer::GetAllocationCreateInfo() const
{
    VmaAllocationCreateInfo alloc_info {};
    alloc_info.flags = m_vma_allocation_create_flags;
    alloc_info.usage = m_vma_usage;

    return alloc_info;
}

RendererResult VulkanGpuBuffer::CheckCanAllocate(
    const VkBufferCreateInfo& buffer_create_info,
    const VmaAllocationCreateInfo& allocation_create_info,
    SizeType size) const
{
    const Features& features = GetRenderBackend()->GetDevice()->GetFeatures();

    RendererResult result;

    uint32 memory_type_index = UINT32_MAX;

    HYPERION_VK_PASS_ERRORS(
        vmaFindMemoryTypeIndexForBufferInfo(
            GetRenderBackend()->GetDevice()->GetAllocator(),
            &buffer_create_info,
            &allocation_create_info,
            &memory_type_index),
        result);

    /* check that we have enough space in the memory type */
    const auto& memory_properties = features.GetPhysicalDeviceMemoryProperties();

    AssertThrow(memory_type_index < memory_properties.memoryTypeCount);

    const auto heap_index = memory_properties.memoryTypes[memory_type_index].heapIndex;
    const auto& heap = memory_properties.memoryHeaps[heap_index];

    if (heap.size < size)
    {
        return HYP_MAKE_ERROR(RendererError, "Heap size is less than requested size. "
                                             "Maybe the wrong memory type has been requested, or the device is out of memory.");
    }

    return result;
}

#pragma endregion VulkanGpuBuffer

} // namespace hyperion
