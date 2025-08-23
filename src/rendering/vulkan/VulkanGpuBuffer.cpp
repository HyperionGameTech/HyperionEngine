/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/math/MathUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <cstring>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

#pragma region Helpers

static uint32 FindMemoryType(uint32 vkTypeFilter, VkMemoryPropertyFlags vkMemoryPropertyFlags)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(GetRenderBackend()->GetDevice()->GetPhysicalDevice(), &memProperties);

    for (uint32 i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((vkTypeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & vkMemoryPropertyFlags) == vkMemoryPropertyFlags)
        {
            HYP_LOG(RenderingBackend, Debug, "Found Memory type {}", i);
            return i;
        }
    }

    HYP_FAIL("Could not find suitable memory type!");
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
        HYP_UNREACHABLE();
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
        HYP_UNREACHABLE();
    }
}

VkPipelineStageFlags GetVkShaderStageMask(ResourceState state, bool src, ShaderModuleType shaderType = SMT_UNSET)
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
        switch (shaderType)
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
        HYP_UNREACHABLE();
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
        return VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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

VmaAllocationCreateFlags GetVkAllocationCreateFlags(GpuBufferType type, bool requireCpuAccessible = false)
{
    switch (type)
    {
    case GpuBufferType::MESH_VERTEX_BUFFER:
        return (requireCpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::MESH_INDEX_BUFFER:
        return (requireCpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::CBUFF:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::SSBO:
        return (requireCpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::ATOMIC_COUNTER:
        return (requireCpuAccessible ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0);
    case GpuBufferType::STAGING_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::INDIRECT_ARGS_BUFFER:
        HYP_GFX_ASSERT(!requireCpuAccessible, "Indirect args buffer cannot be CPU accessible!");
        return 0;
    case GpuBufferType::SHADER_BINDING_TABLE:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER:
        return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    case GpuBufferType::RT_MESH_VERTEX_BUFFER:
        HYP_GFX_ASSERT(!requireCpuAccessible, "RT mesh vertex buffer cannot be CPU accessible!");
        return 0;
    case GpuBufferType::RT_MESH_INDEX_BUFFER:
        HYP_GFX_ASSERT(!requireCpuAccessible, "RT mesh index buffer cannot be CPU accessible!");
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
    if (!IsCreated())
    {
        return;
    }

    if (m_mapping != nullptr)
    {
        Unmap();
    }

    vmaDestroyBuffer(GetRenderBackend()->GetDevice()->GetAllocator(), m_handle, m_vmaAllocation);

    m_handle = VK_NULL_HANDLE;
    m_vmaAllocation = VK_NULL_HANDLE;
    m_resourceState = RS_UNDEFINED;
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

    HYP_GFX_ASSERT(IsCpuAccessible(), "Attempt to map a buffer that is not CPU accessible!");

    vmaMapMemory(GetRenderBackend()->GetDevice()->GetAllocator(), m_vmaAllocation, &m_mapping);
}

void VulkanGpuBuffer::Unmap() const
{
    if (m_mapping == nullptr)
    {
        return;
    }

    vmaUnmapMemory(GetRenderBackend()->GetDevice()->GetAllocator(), m_vmaAllocation);
    m_mapping = nullptr;
}

void VulkanGpuBuffer::Read(SizeType count, void* outPtr) const
{
    if (m_mapping == nullptr)
    {
        Map();

        HYP_LOG(RenderingBackend, Warning, "Attempt to Read() from buffer but data has not been mapped previously");
    }

    Memory::MemCpy(outPtr, m_mapping, count);
}

void VulkanGpuBuffer::Read(SizeType offset, SizeType count, void* outPtr) const
{
    if (m_mapping == nullptr)
    {
        Map();

        HYP_LOG(RenderingBackend, Warning, "Attempt to Read() from buffer but data has not been mapped previously");
    }

    Memory::MemCpy(outPtr, reinterpret_cast<void*>(uintptr_t(m_mapping) + uintptr_t(offset)), count);
}

bool VulkanGpuBuffer::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

bool VulkanGpuBuffer::IsCpuAccessible() const
{
    VmaAllocationInfo info {};
    vmaGetAllocationInfo(GetRenderBackend()->GetDevice()->GetAllocator(), m_vmaAllocation, &info);

    VkMemoryPropertyFlags flags = 0;
    vmaGetMemoryTypeProperties(GetRenderBackend()->GetDevice()->GetAllocator(), info.memoryType, &flags);

    return (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
}

RendererResult VulkanGpuBuffer::CheckCanAllocate(SizeType size) const
{
    const VkBufferCreateInfo createInfo = GetBufferCreateInfo();
    const VmaAllocationCreateInfo allocInfo = GetAllocationCreateInfo();

    return CheckCanAllocate(createInfo, allocInfo, m_size);
}

uint64 VulkanGpuBuffer::GetBufferDeviceAddress() const
{
    HYP_GFX_ASSERT(
        GetRenderBackend()->GetDevice()->GetFeatures().GetBufferDeviceAddressFeatures().bufferDeviceAddress,
        "Called GetBufferDeviceAddress() but the buffer device address extension feature is not supported or enabled!");

    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VkBufferDeviceAddressInfoKHR info { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = m_handle;

    return g_vulkanDynamicFunctions->vkGetBufferDeviceAddressKHR(
        GetRenderBackend()->GetDevice()->GetDevice(),
        &info);
}

void VulkanGpuBuffer::InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState) const
{
    InsertBarrier(VULKAN_CAST(commandBuffer), newState);
}

void VulkanGpuBuffer::InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const
{
    InsertBarrier(VULKAN_CAST(commandBuffer), newState, shaderType);
}

void VulkanGpuBuffer::InsertBarrier(
    VulkanCommandBuffer* commandBuffer,
    ResourceState newState) const
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to insert a resource barrier but buffer was not created");

        return;
    }

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = GetVkAccessMask(m_resourceState);
    barrier.dstAccessMask = GetVkAccessMask(newState);
    barrier.buffer = m_handle;
    barrier.offset = 0;
    barrier.size = m_size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        commandBuffer->GetVulkanHandle(),
        GetVkShaderStageMask(m_resourceState, true),
        GetVkShaderStageMask(newState, false),
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr);

    m_resourceState = newState;
}

void VulkanGpuBuffer::InsertBarrier(
    VulkanCommandBuffer* commandBuffer,
    ResourceState newState,
    ShaderModuleType shaderType) const
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to insert a resource barrier but buffer was not created");

        return;
    }

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = GetVkAccessMask(m_resourceState);
    barrier.dstAccessMask = GetVkAccessMask(newState);
    barrier.buffer = m_handle;
    barrier.offset = 0;
    barrier.size = m_size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        commandBuffer->GetVulkanHandle(),
        GetVkShaderStageMask(m_resourceState, true, shaderType),
        GetVkShaderStageMask(newState, false, shaderType),
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr);

    m_resourceState = newState;
}

void VulkanGpuBuffer::CopyFrom(
    CommandBufferBase* commandBuffer,
    const GpuBufferBase* srcBuffer,
    uint32 count)
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but dst buffer was not created");

        return;
    }

    if (!srcBuffer->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but src buffer was not created");

        return;
    }

    VkBufferCopy region {};
    region.size = count;

    vkCmdCopyBuffer(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VULKAN_CAST(srcBuffer)->m_handle,
        m_handle,
        1,
        &region);
}

void VulkanGpuBuffer::CopyFrom(
    CommandBufferBase* commandBuffer,
    const GpuBufferBase* srcBuffer,
    uint32 srcOffset, uint32 dstOffset,
    uint32 count)
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but dst buffer was not created");

        return;
    }

    if (!srcBuffer->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but src buffer was not created");

        return;
    }

    HYP_GFX_ASSERT((srcOffset + count <= srcBuffer->Size()) && (dstOffset + count <= Size()), "Copy out of bounds!");

    VkBufferCopy region {};
    region.size = count;
    region.srcOffset = srcOffset;
    region.dstOffset = dstOffset;
    
    vkCmdCopyBuffer(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VULKAN_CAST(srcBuffer)->m_handle,
        m_handle,
        1,
        &region);
}

RendererResult VulkanGpuBuffer::Create()
{
    if (IsCreated())
    {
        // already created
        return {};
    }

    m_vkBufferUsageFlags = GetVkUsageFlags(m_type);
    m_vmaUsage = GetVkMemoryUsage(m_type);
    m_vmaAllocationCreateFlags = GetVkAllocationCreateFlags(m_type, m_requireCpuAccessible);

    if (m_size == 0)
    {
        HYP_GFX_ASSERT("Creating empty gpu buffer will result in errors!");

        return HYP_MAKE_ERROR(RendererError, "Creating empty gpu buffer will result in errors!");
    }

    const auto createInfo = GetBufferCreateInfo();
    const auto allocInfo = GetAllocationCreateInfo();

    HYP_GFX_CHECK(CheckCanAllocate(createInfo, allocInfo, m_size));

    if (m_alignment != 0)
    {
        VULKAN_CHECK_MSG(
            vmaCreateBufferWithAlignment(
                GetRenderBackend()->GetDevice()->GetAllocator(),
                &createInfo,
                &allocInfo,
                m_alignment,
                &m_handle,
                &m_vmaAllocation,
                nullptr),
            "Failed to create aligned gpu buffer!");
    }
    else
    {
        VULKAN_CHECK_MSG(
            vmaCreateBuffer(
                GetRenderBackend()->GetDevice()->GetAllocator(),
                &createInfo,
                &allocInfo,
                &m_handle,
                &m_vmaAllocation,
                nullptr),
            "Failed to create gpu buffer!");
    }

    if (IsCpuAccessible())
    {
        Map();
        
        // Memset all to zero
        Memory::MemSet(m_mapping, 0, m_size);
    }

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    return {};
}

RendererResult VulkanGpuBuffer::EnsureCapacity(
    SizeType minimumSize,
    SizeType alignment,
    bool* outSizeChanged)
{
    if (minimumSize == 0)
    {
        return {};
    }

    if (minimumSize <= m_size)
    {
        if (outSizeChanged != nullptr)
        {
            *outSizeChanged = false;
        }

        return {};
    }

    bool shouldCreate = IsCreated();

    if (shouldCreate)
    {
        if (m_mapping != nullptr)
        {
            Unmap();
        }

        struct VulkanBufferDeleter
        {
            VkBuffer buffer;
            VmaAllocation vmaAllocation;
        };
        
        // safely destroy the buffer after the GPU is done with it:
        VulkanBufferDeleter* deleter = GetSafeDeleterInstance()->AllocCustom<VulkanBufferDeleter>([](void* ptr)
            {
                VulkanBufferDeleter* bufferDeleter = reinterpret_cast<VulkanBufferDeleter*>(ptr);

                vmaDestroyBuffer(GetRenderBackend()->GetDevice()->GetAllocator(), bufferDeleter->buffer, bufferDeleter->vmaAllocation);
            });

        new (deleter) VulkanBufferDeleter {
            .buffer = m_handle,
            .vmaAllocation = m_vmaAllocation
        };

        m_handle = VK_NULL_HANDLE;
        m_vmaAllocation = VK_NULL_HANDLE;
        m_resourceState = RS_UNDEFINED;
    }

    m_size = minimumSize;
    m_alignment = alignment;

    if (outSizeChanged != nullptr)
    {
        *outSizeChanged = true;
    }

    if (shouldCreate)
    {
        HYP_GFX_CHECK(Create());
    }

    return {};
}

RendererResult VulkanGpuBuffer::EnsureCapacity(
    SizeType minimumSize,
    bool* outSizeChanged)
{
    return EnsureCapacity(minimumSize, 0, outSizeChanged);
}

VkBufferCreateInfo VulkanGpuBuffer::GetBufferCreateInfo() const
{
    const QueueFamilyIndices& qfIndices = GetRenderBackend()->GetDevice()->GetQueueFamilyIndices();
    const uint32 bufferFamilyIndices[] = { qfIndices.graphicsFamily.Get(), qfIndices.computeFamily.Get() };

    VkBufferCreateInfo vkBufferInfo { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vkBufferInfo.size = m_size;
    vkBufferInfo.usage = m_vkBufferUsageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vkBufferInfo.pQueueFamilyIndices = bufferFamilyIndices;
    vkBufferInfo.queueFamilyIndexCount = ArraySize(bufferFamilyIndices);

    return vkBufferInfo;
}

VmaAllocationCreateInfo VulkanGpuBuffer::GetAllocationCreateInfo() const
{
    VmaAllocationCreateInfo allocInfo {};
    allocInfo.flags = m_vmaAllocationCreateFlags;
    allocInfo.usage = m_vmaUsage;

    return allocInfo;
}

RendererResult VulkanGpuBuffer::CheckCanAllocate(
    const VkBufferCreateInfo& bufferCreateInfo,
    const VmaAllocationCreateInfo& allocationCreateInfo,
    SizeType size) const
{
    const VulkanFeatures& features = GetRenderBackend()->GetDevice()->GetFeatures();

    RendererResult result;

    uint32 memoryTypeIndex = UINT32_MAX;

    VULKAN_PASS_ERRORS(
        vmaFindMemoryTypeIndexForBufferInfo(
            GetRenderBackend()->GetDevice()->GetAllocator(),
            &bufferCreateInfo,
            &allocationCreateInfo,
            &memoryTypeIndex),
        result);

    /* check that we have enough space in the memory type */
    const auto& memoryProperties = features.GetPhysicalDeviceMemoryProperties();

    HYP_GFX_ASSERT(memoryTypeIndex < memoryProperties.memoryTypeCount);

    const auto heapIndex = memoryProperties.memoryTypes[memoryTypeIndex].heapIndex;
    const auto& heap = memoryProperties.memoryHeaps[heapIndex];

    if (heap.size < size)
    {
        return HYP_MAKE_ERROR(RendererError, "Heap size is less than requested size. "
                                             "Maybe the wrong memory type has been requested, or the device is out of memory.");
    }

    return result;
}

#ifdef HYP_DEBUG_MODE

void VulkanGpuBuffer::SetDebugName(Name name)
{
    GpuBufferBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    if (m_vmaAllocation != VK_NULL_HANDLE)
    {
        vmaSetAllocationName(GetRenderBackend()->GetDevice()->GetAllocator(), m_vmaAllocation, strName);
    }

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(GetRenderBackend()->GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanGpuBuffer

} // namespace hyperion
