/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererInstance.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <math/MathUtil.hpp>

#include <cstring>

namespace hyperion {
namespace renderer {
namespace platform {

#pragma region Helpers

static uint FindMemoryType(Device<Platform::VULKAN> *device, uint vk_type_filter, VkMemoryPropertyFlags vk_memory_property_flags)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(device->GetPhysicalDevice(), &mem_properties);

    for (uint i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((vk_type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & vk_memory_property_flags) == vk_memory_property_flags) {
            HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Found Memory type {}", i);
            return i;
        }
    }

    AssertThrowMsg(false, "Could not find suitable memory type!\n");
}

VkImageLayout GetVkImageLayout(ResourceState state)
{
    switch (state) {
    case ResourceState::UNDEFINED:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ResourceState::PRE_INITIALIZED:
        return VK_IMAGE_LAYOUT_PREINITIALIZED;
    case ResourceState::COMMON:
    case ResourceState::UNORDERED_ACCESS:
        return VK_IMAGE_LAYOUT_GENERAL;
    case ResourceState::RENDER_TARGET:
    case ResourceState::RESOLVE_DST:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;            
    case ResourceState::DEPTH_STENCIL:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ResourceState::SHADER_RESOURCE:
    case ResourceState::RESOLVE_SRC:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ResourceState::COPY_DST:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ResourceState::COPY_SRC:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        break;
    case ResourceState::PRESENT:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    default:
        AssertThrowMsg(false, "Unknown resource state");
        return VkImageLayout(-1);
    }
}

VkAccessFlags GetVkAccessMask(ResourceState state)
{
    switch (state) {
    case ResourceState::UNDEFINED:
    case ResourceState::PRESENT:
    case ResourceState::COMMON:
    case ResourceState::PRE_INITIALIZED:
        return VkAccessFlagBits(0);
    case ResourceState::VERTEX_BUFFER:
        return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    case ResourceState::CONSTANT_BUFFER:
        return VK_ACCESS_UNIFORM_READ_BIT;
    case ResourceState::INDEX_BUFFER:
        return VK_ACCESS_INDEX_READ_BIT;
    case ResourceState::RENDER_TARGET:
        return VkAccessFlagBits(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
    case ResourceState::UNORDERED_ACCESS:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case ResourceState::DEPTH_STENCIL:
        return VkAccessFlagBits(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    case ResourceState::SHADER_RESOURCE:
        return VK_ACCESS_SHADER_READ_BIT;
    case ResourceState::INDIRECT_ARG:
        return VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case ResourceState::COPY_DST:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case ResourceState::COPY_SRC:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case ResourceState::RESOLVE_DST:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case ResourceState::RESOLVE_SRC:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    default:
        AssertThrowMsg(false, "Unknown resource state");
        return VkAccessFlagBits(-1);
    }
}

VkPipelineStageFlags GetVkShaderStageMask(ResourceState state, bool src, ShaderModuleType shader_type = ShaderModuleType::UNSET)
{
    switch (state) {
    case ResourceState::UNDEFINED:
    case ResourceState::PRE_INITIALIZED:
    case ResourceState::COMMON:
        if (!src) {
            HYP_LOG(RenderingBackend, LogLevel::WARNING, "Attempt to get shader stage mask for resource state but `src` was set to false. Falling back to all commands.");

            return VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case ResourceState::VERTEX_BUFFER:
    case ResourceState::INDEX_BUFFER:
        return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    case ResourceState::UNORDERED_ACCESS:
    case ResourceState::CONSTANT_BUFFER:
    case ResourceState::SHADER_RESOURCE:
        switch (shader_type) {
        case ShaderModuleType::VERTEX:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case ShaderModuleType::FRAGMENT:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case ShaderModuleType::COMPUTE:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case ShaderModuleType::RAY_ANY_HIT:
        case ShaderModuleType::RAY_CLOSEST_HIT:
        case ShaderModuleType::RAY_GEN:
        case ShaderModuleType::RAY_INTERSECT:
        case ShaderModuleType::RAY_MISS:
            return VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        case ShaderModuleType::GEOMETRY:
            return VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        case ShaderModuleType::TESS_CONTROL:
            return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
        case ShaderModuleType::TESS_EVAL:
            return VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        case ShaderModuleType::MESH:
            return VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV;
        case ShaderModuleType::TASK:
            return VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV;
        case ShaderModuleType::UNSET:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                 | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                 | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        }
    case ResourceState::RENDER_TARGET:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case ResourceState::DEPTH_STENCIL:
        return src ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    case ResourceState::INDIRECT_ARG:
        return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case ResourceState::COPY_DST:
    case ResourceState::COPY_SRC:
    case ResourceState::RESOLVE_DST:
    case ResourceState::RESOLVE_SRC:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case ResourceState::PRESENT:
        return src ? (VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    default:
        AssertThrowMsg(false, "Unknown resource state");
        return VkPipelineStageFlags(-1);
    }
}

VkBufferUsageFlags GetVkUsageFlags(GPUBufferType type)
{
    switch (type) {
    case GPUBufferType::MESH_VERTEX_BUFFER:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    case GPUBufferType::MESH_INDEX_BUFFER:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case GPUBufferType::CONSTANT_BUFFER:
        return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    case GPUBufferType::STORAGE_BUFFER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GPUBufferType::ATOMIC_COUNTER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
          | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
          | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GPUBufferType::STAGING_BUFFER:
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    case GPUBufferType::INDIRECT_ARGS_BUFFER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
          | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
          | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    case GPUBufferType::SHADER_BINDING_TABLE:
        return VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GPUBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GPUBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    case GPUBufferType::RT_MESH_VERTEX_BUFFER:
        return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GPUBufferType::RT_MESH_INDEX_BUFFER:
        return VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    case GPUBufferType::SCRATCH_BUFFER:
        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    default:
        return 0;
    }
}

VmaMemoryUsage GetVkMemoryUsage(GPUBufferType type)
{
    switch (type) {
    case GPUBufferType::MESH_VERTEX_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GPUBufferType::MESH_INDEX_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GPUBufferType::CONSTANT_BUFFER:
        return VMA_MEMORY_USAGE_AUTO;
    case GPUBufferType::STORAGE_BUFFER:
        return VMA_MEMORY_USAGE_AUTO;
    case GPUBufferType::ATOMIC_COUNTER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GPUBufferType::STAGING_BUFFER:
        return VMA_MEMORY_USAGE_CPU_ONLY;
    case GPUBufferType::INDIRECT_ARGS_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GPUBufferType::SHADER_BINDING_TABLE:
        return VMA_MEMORY_USAGE_AUTO;
    case GPUBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VMA_MEMORY_USAGE_AUTO;
    case GPUBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VMA_MEMORY_USAGE_AUTO;
    case GPUBufferType::RT_MESH_VERTEX_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GPUBufferType::RT_MESH_INDEX_BUFFER:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case GPUBufferType::SCRATCH_BUFFER:
        return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    default:
        return VMA_MEMORY_USAGE_UNKNOWN;
    }
}

VmaAllocationCreateFlags GetVkAllocationCreateFlags(GPUBufferType type)
{
    switch (type) {
    case GPUBufferType::MESH_VERTEX_BUFFER:
        return 0;
    case GPUBufferType::MESH_INDEX_BUFFER:
        return 0;
    case GPUBufferType::CONSTANT_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GPUBufferType::STORAGE_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GPUBufferType::ATOMIC_COUNTER:
        return 0;
    case GPUBufferType::STAGING_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GPUBufferType::INDIRECT_ARGS_BUFFER:
        return 0;
    case GPUBufferType::SHADER_BINDING_TABLE:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GPUBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GPUBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GPUBufferType::RT_MESH_VERTEX_BUFFER:
        return 0;
    case GPUBufferType::RT_MESH_INDEX_BUFFER:
        return 0;
    case GPUBufferType::SCRATCH_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    default:
        HYP_THROW("Invalid gpu buffer type for allocation create flags");
    }
}

#pragma endregion Helpers

// GPUMemory<Platform::VULKAN>::GPUMemory()
//     : m_id(0),
//       sharing_mode(VK_SHARING_MODE_EXCLUSIVE),
//       size(0),
//       map(nullptr),
//       resource_state(ResourceState::UNDEFINED),
//       allocation(VK_NULL_HANDLE),
//       m_is_created(false)
// {
//     static uint allocations = 0;

//     m_id = allocations++;
// }

// GPUMemory<Platform::VULKAN>::GPUMemory(GPUMemory &&other) noexcept
//     : m_id(other.m_id),
//       sharing_mode(other.sharing_mode),
//       size(other.size),
//       map(other.map),
//       resource_state(other.resource_state),
//       allocation(other.allocation),
//       m_is_created(other.m_is_created)
// {
//     other.m_id = 0;
//     other.size = 0;
//     other.map = nullptr;
//     other.resource_state = ResourceState::UNDEFINED;
//     other.allocation = VK_NULL_HANDLE;
//     other.m_is_created = false;
// }

// GPUMemory<Platform::VULKAN>::~GPUMemory()
// {
// }

#pragma region GPUBufferPlatformImpl

void GPUBufferPlatformImpl<Platform::VULKAN>::Map(Device<Platform::VULKAN> *device) const
{
    vmaMapMemory(device->GetAllocator(), vma_allocation, &mapping);
}

void GPUBufferPlatformImpl<Platform::VULKAN>::Unmap(Device<Platform::VULKAN> *device) const
{
    vmaUnmapMemory(device->GetAllocator(), vma_allocation);
    mapping = nullptr;
}

VkBufferCreateInfo GPUBufferPlatformImpl<Platform::VULKAN>::GetBufferCreateInfo(Device<Platform::VULKAN> *device) const
{
    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const uint32 buffer_family_indices[] = { qf_indices.graphics_family.Get(), qf_indices.compute_family.Get() };

    VkBufferCreateInfo vk_buffer_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vk_buffer_info.size = size;
    vk_buffer_info.usage = vk_buffer_usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vk_buffer_info.pQueueFamilyIndices = buffer_family_indices;
    vk_buffer_info.queueFamilyIndexCount = ArraySize(buffer_family_indices);

    return vk_buffer_info;
}

VmaAllocationCreateInfo GPUBufferPlatformImpl<Platform::VULKAN>::GetAllocationCreateInfo(Device<Platform::VULKAN> *device) const
{
    /* @TODO: Property debug names */
    char debug_name_buffer[1024] = { 0 };
    snprintf(debug_name_buffer, sizeof(debug_name_buffer), "Unnamed buffer %p", this);

    VmaAllocationCreateInfo alloc_info { };
    alloc_info.flags = vma_allocation_create_flags;
    alloc_info.usage = vma_usage;
    alloc_info.pUserData = debug_name_buffer;

    return alloc_info;
}

Result GPUBufferPlatformImpl<Platform::VULKAN>::CheckCanAllocate(
    Device<Platform::VULKAN> *device,
    const VkBufferCreateInfo &buffer_create_info,
    const VmaAllocationCreateInfo &allocation_create_info,
    SizeType size
) const
{
    const Features &features = device->GetFeatures();

    Result result;

    uint32 memory_type_index = UINT32_MAX;

    HYPERION_VK_PASS_ERRORS(
        vmaFindMemoryTypeIndexForBufferInfo(
            device->GetAllocator(),
            &buffer_create_info,
            &allocation_create_info,
            &memory_type_index
        ),
        result
    );

    /* check that we have enough space in the memory type */
    const auto &memory_properties = features.GetPhysicalDeviceMemoryProperties();

    AssertThrow(memory_type_index < memory_properties.memoryTypeCount);

    const auto heap_index = memory_properties.memoryTypes[memory_type_index].heapIndex;
    const auto &heap = memory_properties.memoryHeaps[heap_index];

    if (heap.size < size) {
        return {Result::RENDERER_ERR, "Heap size is less than requested size. "
            "Maybe the wrong memory type has been requested, or the device is out of memory."};
    }

    return result;
}

#pragma endregion GPUBufferPlatformImpl

#pragma region GPUBuffer

template <>
void GPUBuffer<Platform::VULKAN>::Memset(Device<Platform::VULKAN> *device, SizeType count, ubyte value)
{    
    if (m_platform_impl.mapping == nullptr) {
        m_platform_impl.Map(device);
    }

    Memory::MemSet(m_platform_impl.mapping, value, count);
}

template <>
void GPUBuffer<Platform::VULKAN>::Copy(Device<Platform::VULKAN> *device, SizeType count, const void *ptr)
{
    if (m_platform_impl.mapping == nullptr) {
        m_platform_impl.Map(device);
    }

    Memory::MemCpy(m_platform_impl.mapping, ptr, count);
}

template <>
void GPUBuffer<Platform::VULKAN>::Copy(Device<Platform::VULKAN> *device, SizeType offset, SizeType count, const void *ptr)
{
    if (m_platform_impl.mapping == nullptr) {
        m_platform_impl.Map(device);
    }

    Memory::MemCpy(reinterpret_cast<void *>(uintptr_t(m_platform_impl.mapping) + offset), ptr, count);
}

template <>
void GPUBuffer<Platform::VULKAN>::Map(Device<Platform::VULKAN> *device)
{
    if (m_platform_impl.mapping != nullptr) {
        return;
    }

    m_platform_impl.Map(device);
}

template <>
void GPUBuffer<Platform::VULKAN>::Unmap(Device<Platform::VULKAN> *device)
{
    if (m_platform_impl.mapping == nullptr) {
        return;
    }

    m_platform_impl.Unmap(device);
}

template <>
void GPUBuffer<Platform::VULKAN>::Read(Device<Platform::VULKAN> *device, SizeType count, void *out_ptr) const
{
    if (m_platform_impl.mapping == nullptr) {
        m_platform_impl.Map(device);

        HYP_LOG(RenderingBackend, LogLevel::WARNING, "Attempt to Read() from buffer but data has not been mapped previously");
    }

    Memory::MemCpy(out_ptr, m_platform_impl.mapping, count);
}

template <>
GPUBuffer<Platform::VULKAN>::GPUBuffer(GPUBufferType type)
    : m_platform_impl { this },
      m_type(type),
      m_resource_state(ResourceState::UNDEFINED)
{
}

template <>
GPUBuffer<Platform::VULKAN>::GPUBuffer(GPUBuffer &&other) noexcept
    : m_platform_impl { this },
      m_type(other.m_type),
      m_resource_state(other.m_resource_state)
{
    m_platform_impl = std::move(other.m_platform_impl);
    m_platform_impl.self = this;

    other.m_platform_impl = { &other };
    other.m_type = GPUBufferType::NONE;
    other.m_resource_state = ResourceState::UNDEFINED;
}

template <>
GPUBuffer<Platform::VULKAN>::~GPUBuffer()
{
    AssertThrowMsg(m_platform_impl.handle == VK_NULL_HANDLE, "buffer should have been destroyed!");
}

template <>
bool GPUBuffer<Platform::VULKAN>::IsCreated() const
{
    return m_platform_impl.handle != VK_NULL_HANDLE;
}

template <>
uint32 GPUBuffer<Platform::VULKAN>::Size() const
{
    return m_platform_impl.size;
}

template <>
bool GPUBuffer<Platform::VULKAN>::IsCPUAccessible() const
{
    return m_platform_impl.vma_usage != VMA_MEMORY_USAGE_GPU_ONLY;
}

template <>
Result GPUBuffer<Platform::VULKAN>::CheckCanAllocate(Device<Platform::VULKAN> *device, SizeType size) const
{
    const VkBufferCreateInfo create_info = m_platform_impl.GetBufferCreateInfo(device);
    const VmaAllocationCreateInfo alloc_info = m_platform_impl.GetAllocationCreateInfo(device);

    return m_platform_impl.CheckCanAllocate(device, create_info, alloc_info, m_platform_impl.size);
}

template <>
uint64 GPUBuffer<Platform::VULKAN>::GetBufferDeviceAddress(Device<Platform::VULKAN> *device) const
{
    AssertThrowMsg(
        device->GetFeatures().GetBufferDeviceAddressFeatures().bufferDeviceAddress,
        "Called GetBufferDeviceAddress() but the buffer device address extension feature is not supported or enabled!"
    );

    AssertThrow(m_platform_impl.handle != VK_NULL_HANDLE);

    VkBufferDeviceAddressInfoKHR info { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = m_platform_impl.handle;

	return device->GetFeatures().dyn_functions.vkGetBufferDeviceAddressKHR(
        device->GetDevice(),
        &info
    );
}

template <>
void GPUBuffer<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    ResourceState new_state
) const
{
    if (!IsCreated()) {
        HYP_LOG(RenderingBackend, LogLevel::WARNING, "Attempt to insert a resource barrier but buffer was not created");

        return;
    }

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = GetVkAccessMask(m_resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.buffer = m_platform_impl.handle;
    barrier.offset = 0;
    barrier.size = m_platform_impl.size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetPlatformImpl().command_buffer,
        GetVkShaderStageMask(m_resource_state, true),
        GetVkShaderStageMask(new_state, false),
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );

    m_resource_state = new_state;
}

template <>
void GPUBuffer<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    ResourceState new_state,
    ShaderModuleType shader_type
) const
{
    if (!IsCreated()) {
        HYP_LOG(RenderingBackend, LogLevel::WARNING, "Attempt to insert a resource barrier but buffer was not created");

        return;
    }

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = GetVkAccessMask(m_resource_state);
    barrier.dstAccessMask = GetVkAccessMask(new_state);
    barrier.buffer = m_platform_impl.handle;
    barrier.offset = 0;
    barrier.size = m_platform_impl.size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetPlatformImpl().command_buffer,
        GetVkShaderStageMask(m_resource_state, true, shader_type),
        GetVkShaderStageMask(new_state, false, shader_type),
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );

    m_resource_state = new_state;
}

template <>
void GPUBuffer<Platform::VULKAN>::CopyFrom(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const GPUBuffer *src_buffer,
    SizeType count
)
{
    if (!IsCreated()) {
        HYP_LOG(RenderingBackend, LogLevel::WARNING, "Attempt to copy from buffer but dst buffer was not created");

        return;
    }

    if (!src_buffer->IsCreated()) {
        HYP_LOG(RenderingBackend, LogLevel::WARNING, "Attempt to copy from buffer but src buffer was not created");

        return;
    }

    InsertBarrier(command_buffer, ResourceState::COPY_DST);
    src_buffer->InsertBarrier(command_buffer, ResourceState::COPY_SRC);

    VkBufferCopy region { };
    region.size = count;

    vkCmdCopyBuffer(
        command_buffer->GetPlatformImpl().command_buffer,
        src_buffer->m_platform_impl.handle,
        m_platform_impl.handle,
        1,
        &region
    );
}

template <>
Result GPUBuffer<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (!IsCreated()) {
        HYPERION_RETURN_OK;
    }

    if (m_platform_impl.mapping != nullptr) {
        m_platform_impl.Unmap(device);
    }

    vmaDestroyBuffer(device->GetAllocator(), m_platform_impl.handle, m_platform_impl.vma_allocation);
    
    m_platform_impl = { this };
    m_resource_state = ResourceState::UNDEFINED;

    HYPERION_RETURN_OK;
}

template <>
Result GPUBuffer<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, SizeType size, SizeType alignment)
{
    if (IsCreated()) {
        HYP_LOG(RenderingBackend, LogLevel::WARNING, "Create() called on a buffer that has not been destroyed!\n"
            "\tYou should explicitly call Destroy() on the object before reallocating it.\n"
            "\tTo prevent memory leaks, calling Destroy() before allocating the memory...");

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(false, "Create() called on a buffer that has not been destroyed!");
#endif

        HYPERION_BUBBLE_ERRORS(Destroy(device));
    }

    m_platform_impl.vk_buffer_usage_flags = GetVkUsageFlags(m_type);
    m_platform_impl.vma_usage = GetVkMemoryUsage(m_type);
    m_platform_impl.vma_allocation_create_flags = GetVkAllocationCreateFlags(m_type)
        | VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
    m_platform_impl.size = size;
    
    if (size == 0) {
#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(false, "Creating empty gpu buffer will result in errors!");
#endif

        return { Result::RENDERER_ERR, "Creating empty gpu buffer will result in errors! \n" };
    }

    const auto create_info = m_platform_impl.GetBufferCreateInfo(device);
    const auto alloc_info = m_platform_impl.GetAllocationCreateInfo(device);

    HYPERION_BUBBLE_ERRORS(m_platform_impl.CheckCanAllocate(device, create_info, alloc_info, m_platform_impl.size));

    if (alignment != 0) {
        HYPERION_VK_CHECK_MSG(
            vmaCreateBufferWithAlignment(
                device->GetAllocator(),
                &create_info,
                &alloc_info,
                alignment,
                &m_platform_impl.handle,
                &m_platform_impl.vma_allocation,
                nullptr
            ),
            "Failed to create aligned gpu buffer!"
        );
    } else {
        HYPERION_VK_CHECK_MSG(
            vmaCreateBuffer(
                device->GetAllocator(),
                &create_info,
                &alloc_info,
                &m_platform_impl.handle,
                &m_platform_impl.vma_allocation,
                nullptr
            ),
            "Failed to create gpu buffer!"
        );
    }

    HYPERION_RETURN_OK;
}

template <>
Result GPUBuffer<Platform::VULKAN>::EnsureCapacity(
    Device<Platform::VULKAN> *device,
    SizeType minimum_size,
    SizeType alignment,
    bool *out_size_changed
)
{
    Result result;

    if (minimum_size <= m_platform_impl.size) {
        if (out_size_changed != nullptr) {
            *out_size_changed = false;
        }

        HYPERION_RETURN_OK;
    }

    if (m_platform_impl.handle != VK_NULL_HANDLE) {
        HYPERION_PASS_ERRORS(Destroy(device), result);

        if (!result) {
            if (out_size_changed != nullptr) {
                *out_size_changed = false;
            }

            return result;
        }
    }

    HYPERION_PASS_ERRORS(Create(device, minimum_size, alignment), result);

    if (out_size_changed != nullptr) {
        *out_size_changed = bool(result);
    }

    return result;
}

template <>
Result GPUBuffer<Platform::VULKAN>::EnsureCapacity(
    Device<Platform::VULKAN> *device,
    SizeType minimum_size,
    bool *out_size_changed
)
{
    return EnsureCapacity(device, minimum_size, 0, out_size_changed);
}

#pragma endregion GPUBuffer

#pragma region ShaderBindingTableBuffer

ShaderBindingTableBuffer<Platform::VULKAN>::ShaderBindingTableBuffer()
    : GPUBuffer<Platform::VULKAN>(GPUBufferType::SHADER_BINDING_TABLE),
      region { }
{
}

#pragma endregion ShaderBindingTableBuffer

} // namespace platform
} // namespace renderer
} // namespace hyperion
