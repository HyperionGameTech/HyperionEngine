#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererInstance.hpp>

#include <math/MathUtil.hpp>

#include <system/Debug.hpp>

#include <vector>
#include <queue>
#include <cstring>

namespace hyperion {
namespace renderer {
namespace platform {

uint GPUMemory<Platform::VULKAN>::FindMemoryType(Device<Platform::VULKAN> *device, uint vk_type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(device->GetPhysicalDevice(), &mem_properties);

    for (uint i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((vk_type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            DebugLog(LogType::Info, "Found Memory type [%d]!\n", i);
            return i;
        }
    }

    AssertThrowMsg(false, "Could not find suitable memory type!\n");
}

VkImageLayout GPUMemory<Platform::VULKAN>::GetImageLayout(ResourceState state)
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

VkAccessFlags GPUMemory<Platform::VULKAN>::GetAccessMask(ResourceState state)
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

VkPipelineStageFlags GPUMemory<Platform::VULKAN>::GetShaderStageMask(ResourceState state, bool src, ShaderModuleType shader_type)
{
    switch (state) {
    case ResourceState::UNDEFINED:
    case ResourceState::PRE_INITIALIZED:
    case ResourceState::COMMON:
        if (!src) {
            DebugLog(LogType::Warn, "Attempt to get shader stage mask for resource state %d but `src` was set to false. Falling back to all commands.\n");

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

GPUMemory<Platform::VULKAN>::Stats GPUMemory<Platform::VULKAN>::stats{};

GPUMemory<Platform::VULKAN>::GPUMemory()
    : m_id(0),
      sharing_mode(VK_SHARING_MODE_EXCLUSIVE),
      size(0),
      map(nullptr),
      resource_state(ResourceState::UNDEFINED),
      allocation(VK_NULL_HANDLE),
      m_is_created(false)
{
    static uint allocations = 0;

    m_id = allocations++;
}

GPUMemory<Platform::VULKAN>::GPUMemory(GPUMemory &&other) noexcept
    : m_id(other.m_id),
      sharing_mode(other.sharing_mode),
      size(other.size),
      map(other.map),
      resource_state(other.resource_state),
      allocation(other.allocation),
      m_is_created(other.m_is_created)
{
    other.m_id = 0;
    other.size = 0;
    other.map = nullptr;
    other.resource_state = ResourceState::UNDEFINED;
    other.allocation = VK_NULL_HANDLE;
    other.m_is_created = false;
}

GPUMemory<Platform::VULKAN>::~GPUMemory()
{
}

void GPUMemory<Platform::VULKAN>::Map(Device<Platform::VULKAN> *device, void **ptr) const
{
    vmaMapMemory(device->GetAllocator(), allocation, ptr);
}

void GPUMemory<Platform::VULKAN>::Unmap(Device<Platform::VULKAN> *device) const
{
    vmaUnmapMemory(device->GetAllocator(), allocation);
    map = nullptr;
}

void GPUMemory<Platform::VULKAN>::Memset(Device<Platform::VULKAN> *device, SizeType count, ubyte value)
{    
    if (map == nullptr) {
        Map(device, &map);
    }

    std::memset(map, value, count);
}

void GPUMemory<Platform::VULKAN>::Copy(Device<Platform::VULKAN> *device, SizeType count, const void *ptr)
{
    if (map == nullptr) {
        Map(device, &map);
    }

    Memory::MemCpy(map, ptr, count);
}

void GPUMemory<Platform::VULKAN>::Copy(Device<Platform::VULKAN> *device, SizeType offset, SizeType count, const void *ptr)
{
    if (map == nullptr) {
        Map(device, &map);
    }

    Memory::MemCpy(reinterpret_cast<void *>(uintptr_t(map) + offset), ptr, count);
}

void GPUMemory<Platform::VULKAN>::Read(Device<Platform::VULKAN> *device, SizeType count, void *out_ptr) const
{
    if (map == nullptr) {
        Map(device, &map);
        DebugLog(LogType::Warn, "Attempt to Read() from buffer but data has not been mapped previously\n");
    }

    Memory::MemCpy(out_ptr, map, count);
}

void GPUMemory<Platform::VULKAN>::Create()
{
    stats.IncMemoryUsage(size);

    m_is_created = true;
}

void GPUMemory<Platform::VULKAN>::Destroy()
{
    stats.DecMemoryUsage(size);

    m_is_created = false;
}

static VkBufferUsageFlags GetVkUsageFlags(GPUBufferType type)
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

static VmaMemoryUsage GetVkMemoryUsage(GPUBufferType type)
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

GPUBuffer<Platform::VULKAN>::GPUBuffer(GPUBufferType type)
    : GPUMemory<Platform::VULKAN>(),
      type(type),
      buffer(nullptr),
      usage_flags(GetVkUsageFlags(type)),
      vma_usage(GetVkMemoryUsage(type)),
      vma_allocation_create_flags(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
{
}

GPUBuffer<Platform::VULKAN>::GPUBuffer(GPUBuffer &&other) noexcept
    : GPUMemory<Platform::VULKAN>(static_cast<GPUMemory<Platform::VULKAN> &&>(std::move(other))),
      type(other.type),
      buffer(other.buffer),
      usage_flags(other.usage_flags),
      vma_usage(other.vma_usage),
      vma_allocation_create_flags(other.vma_allocation_create_flags)
{
    other.buffer = VK_NULL_HANDLE;
    other.usage_flags = 0;
    other.vma_usage = VMA_MEMORY_USAGE_UNKNOWN;
    other.vma_allocation_create_flags = 0;
}

GPUBuffer<Platform::VULKAN>::~GPUBuffer()
{
    AssertThrowMsg(buffer == VK_NULL_HANDLE, "buffer should have been destroyed!");
}

VkBufferCreateInfo GPUBuffer<Platform::VULKAN>::GetBufferCreateInfo(Device<Platform::VULKAN> *device) const
{
    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const uint32 buffer_family_indices[] = { qf_indices.graphics_family.Get(), qf_indices.compute_family.Get() };

    VkBufferCreateInfo vk_buffer_info { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    vk_buffer_info.size                     = size;
    vk_buffer_info.usage                    = usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vk_buffer_info.pQueueFamilyIndices      = buffer_family_indices;
    vk_buffer_info.queueFamilyIndexCount    = uint32(std::size(buffer_family_indices));

    return vk_buffer_info;
}

VmaAllocationCreateInfo GPUBuffer<Platform::VULKAN>::GetAllocationCreateInfo(Device<Platform::VULKAN> *device) const
{
    VmaAllocationCreateInfo alloc_info { };
    alloc_info.flags = vma_allocation_create_flags;
    alloc_info.usage = vma_usage;
    alloc_info.pUserData = reinterpret_cast<void *>(uintptr_t(uint64(ID_MASK_BUFFER) | uint64(m_id)));

    return alloc_info;
}

Result GPUBuffer<Platform::VULKAN>::CheckCanAllocate(Device<Platform::VULKAN> *device, SizeType size) const
{
    const auto create_info = GetBufferCreateInfo(device);
    const auto alloc_info = GetAllocationCreateInfo(device);

    return CheckCanAllocate(device, create_info, alloc_info, this->size);
}

uint64 GPUBuffer<Platform::VULKAN>::GetBufferDeviceAddress(Device<Platform::VULKAN> *device) const
{
    AssertThrowMsg(
        device->GetFeatures().GetBufferDeviceAddressFeatures().bufferDeviceAddress,
        "Called GetBufferDeviceAddress() but the buffer device address extension feature is not supported or enabled!"
    );

    AssertThrow(buffer != VK_NULL_HANDLE);

    VkBufferDeviceAddressInfoKHR info { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = buffer;

	return device->GetFeatures().dyn_functions.vkGetBufferDeviceAddressKHR(
        device->GetDevice(),
        &info
    );
}

Result GPUBuffer<Platform::VULKAN>::CheckCanAllocate(
    Device<Platform::VULKAN> *device,
    const VkBufferCreateInfo &buffer_create_info,
    const VmaAllocationCreateInfo &allocation_create_info,
    SizeType size
) const
{
    const Features &features = device->GetFeatures();

    auto result = Result::OK;

    uint memory_type_index = UINT32_MAX;

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

void GPUBuffer<Platform::VULKAN>::InsertBarrier(CommandBuffer<Platform::VULKAN> *command_buffer, ResourceState new_state) const
{
    if (buffer == nullptr) {
        DebugLog(
            LogType::Warn,
            "Attempt to insert a resource barrier but buffer was not defined\n"
        );

        return;
    }

    VkBufferMemoryBarrier barrier { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = GetAccessMask(resource_state);
    barrier.dstAccessMask = GetAccessMask(new_state);
    barrier.buffer = buffer;
    barrier.offset = 0;
    barrier.size = size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetCommandBuffer(),
        GetShaderStageMask(resource_state, true),
        GetShaderStageMask(new_state, false),
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );

    resource_state = new_state;
}

void GPUBuffer<Platform::VULKAN>::CopyFrom(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const GPUBuffer *src_buffer,
    SizeType count
)
{
    InsertBarrier(command_buffer, ResourceState::COPY_DST);
    src_buffer->InsertBarrier(command_buffer, ResourceState::COPY_SRC);

    VkBufferCopy region { };
    region.size = count;

    vkCmdCopyBuffer(
        command_buffer->GetCommandBuffer(),
        src_buffer->buffer,
        buffer,
        1,
        &region
    );
}

Result GPUBuffer<Platform::VULKAN>::CopyStaged(
    Instance<Platform::VULKAN> *instance,
    const void *ptr,
    SizeType count
)
{
    Device<Platform::VULKAN> *device = instance->GetDevice();

    return instance->GetStagingBufferPool().Use(device, [&](StagingBufferPool::Context &holder) {
        auto commands = instance->GetSingleTimeCommands();

        auto *staging_buffer = holder.Acquire(count);
        staging_buffer->Copy(device, count, ptr);
        
        commands.Push([&](CommandBuffer<Platform::VULKAN> *cmd) {
            CopyFrom(cmd, staging_buffer, count);

            HYPERION_RETURN_OK;
        });
    
        return commands.Execute(device);
    });
}

Result GPUBuffer<Platform::VULKAN>::ReadStaged(
    Instance<Platform::VULKAN> *instance,
    SizeType count,
    void *out_ptr
) const
{
    Device<Platform::VULKAN> *device = instance->GetDevice();

    return instance->GetStagingBufferPool().Use(device, [&](StagingBufferPool::Context &holder) {
        auto commands = instance->GetSingleTimeCommands();

        auto *staging_buffer = holder.Acquire(count);
        
        commands.Push([&](CommandBuffer<Platform::VULKAN> *cmd) {
            staging_buffer->CopyFrom(
                cmd,
                this,
                count
            );

            HYPERION_RETURN_OK;
        });
    
        auto result = commands.Execute(device);

        if (result) {
            staging_buffer->Read(
                device,
                count,
                out_ptr
            );
        }

        return result;
    });
}

Result GPUBuffer<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, SizeType size, SizeType alignment)
{
    if (buffer != nullptr) {
        DebugLog(
            LogType::Warn,
            "Create() called on a buffer (memory #%u) that has not been destroyed!\n"
            "\tYou should explicitly call Destroy() on the object before reallocating it.\n"
            "\tTo prevent memory leaks, calling Destroy() before allocating the memory...\n",
            m_id
        );

#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif

        HYPERION_BUBBLE_ERRORS(Destroy(device));
    }

    this->size = size;

    GPUMemory<Platform::VULKAN>::Create();
    
    if (size == 0) {
#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif
        return { Result::RENDERER_ERR, "Creating empty gpu buffer will result in errors! \n" };
    }

    DebugLog(
        LogType::Debug,
        "Allocating GPU buffer with flags:\t(buffer usage: %lu, alloc create: %lu, alloc usage: %lu)\n",
        usage_flags,
        vma_allocation_create_flags,
        vma_usage
    );

    const auto create_info = GetBufferCreateInfo(device);
    const auto alloc_info = GetAllocationCreateInfo(device);

    HYPERION_BUBBLE_ERRORS(CheckCanAllocate(device, create_info, alloc_info, this->size));

    if (alignment != 0) {
        HYPERION_VK_CHECK_MSG(
            vmaCreateBufferWithAlignment(
                device->GetAllocator(),
                &create_info,
                &alloc_info,
                alignment,
                &buffer,
                &allocation,
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
                &buffer,
                &allocation,
                nullptr
            ),
            "Failed to create gpu buffer!"
        );
    }

    HYPERION_RETURN_OK;
}

Result GPUBuffer<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    GPUMemory<Platform::VULKAN>::Destroy();

    if (map != nullptr) {
        Unmap(device);
    }

    vmaDestroyBuffer(device->GetAllocator(), buffer, allocation);
    buffer = nullptr;
    allocation = nullptr;

    HYPERION_RETURN_OK;
}

Result GPUBuffer<Platform::VULKAN>::EnsureCapacity(
    Device<Platform::VULKAN> *device,
    SizeType minimum_size,
    bool *out_size_changed
)
{
    return EnsureCapacity(device, minimum_size, 0, out_size_changed);
}

Result GPUBuffer<Platform::VULKAN>::EnsureCapacity(
    Device<Platform::VULKAN> *device,
    SizeType minimum_size,
    SizeType alignment,
    bool *out_size_changed
)
{
    auto result = Result::OK;

    if (minimum_size <= size) {
        if (out_size_changed != nullptr) {
            *out_size_changed = false;
        }

        HYPERION_RETURN_OK;
    }

    if (buffer != VK_NULL_HANDLE) {
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

#ifdef HYP_DEBUG_MODE

void GPUBuffer<Platform::VULKAN>::DebugLogBuffer(Instance<Platform::VULKAN> *instance) const
{
    Device<Platform::VULKAN> *device = instance->GetDevice();

    if (size % sizeof(uint32) == 0) {
        const auto data = DebugReadBytes<uint32>(instance, device);

        for (SizeType i = 0; i < data.size();) {
            const auto dist = MathUtil::Min(data.size() - i, SizeType(4));

            if (dist == 4) {
                DebugLog(LogType::Debug, "%lu\t%lu\t%lu\t%lu\n", data[i], data[i + 1], data[i + 2], data[i + 3]);
            } else if (dist == 3) {
                DebugLog(LogType::Debug, "%lu\t%lu\t%lu\n", data[i], data[i + 1], data[i + 2]);
            } else if (dist == 2) {
                DebugLog(LogType::Debug, "%lu\t%lu\n", data[i], data[i + 1]);
            } else {
                DebugLog(LogType::Debug, "%lu\n", data[i]);
            }

            i += dist;
        }
    } else {
        const auto data = DebugReadBytes(instance, device);

        for (const auto byte : data) {
            DebugLog(LogType::Debug, "0x%.12\n", byte);
        }
    }
}

#endif

template <>
void VertexBuffer<Platform::VULKAN>::Bind(CommandBuffer<Platform::VULKAN> *cmd)
{
    const VkBuffer vertex_buffers[] = { buffer };
    const VkDeviceSize offsets[]    = { 0 };

    vkCmdBindVertexBuffers(cmd->GetCommandBuffer(), 0, 1, vertex_buffers, offsets);
}

template <>
void IndexBuffer<Platform::VULKAN>::Bind(CommandBuffer<Platform::VULKAN> *cmd)
{
    vkCmdBindIndexBuffer(
        cmd->GetCommandBuffer(),
        buffer,
        0,
        helpers::ToVkIndexType(GetDatumType())
    );
}

template <>
void IndirectBuffer<Platform::VULKAN>::DispatchIndirect(CommandBuffer<Platform::VULKAN> *command_buffer, SizeType offset) const
{
    vkCmdDispatchIndirect(
        command_buffer->GetCommandBuffer(),
        buffer,
        offset
    );
}

ShaderBindingTableBuffer<Platform::VULKAN>::ShaderBindingTableBuffer()
    : GPUBuffer<Platform::VULKAN>(GPUBufferType::SHADER_BINDING_TABLE),
      region { }
{
}

GPUImageMemory<Platform::VULKAN>::GPUImageMemory()
    : GPUMemory<Platform::VULKAN>(),
      image(VK_NULL_HANDLE),
      is_image_owned(false)
{
}

GPUImageMemory<Platform::VULKAN>::~GPUImageMemory()
{
    AssertThrowMsg(image == VK_NULL_HANDLE, "image should have been destroyed!");
}

void GPUImageMemory<Platform::VULKAN>::SetResourceState(ResourceState new_state)
{
    resource_state = new_state;

    sub_resources.clear();
}

void GPUImageMemory<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    ResourceState new_state,
    ImageSubResourceFlagBits flags
)
{
    InsertBarrier(
        command_buffer,
        ImageSubResource {
            .flags = flags,
            .num_layers = VK_REMAINING_ARRAY_LAYERS,
            .num_levels = VK_REMAINING_MIP_LEVELS
        },
        new_state
    );
}

void GPUImageMemory<Platform::VULKAN>::InsertBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    /* Clear any sub-resources that are in a separate state */
    if (!sub_resources.empty()) {
        sub_resources.clear();
    }

    if (image == nullptr) {
        DebugLog(
            LogType::Warn,
            "Attempt to insert a resource barrier but image was not defined\n"
        );

        return;
    }

    const VkImageAspectFlags aspect_flag_bits = 
        (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    VkImageSubresourceRange range { };
    range.aspectMask = aspect_flag_bits;
    range.baseArrayLayer = sub_resource.base_array_layer;
    range.layerCount = sub_resource.num_layers;
    range.baseMipLevel = sub_resource.base_mip_level;
    range.levelCount = sub_resource.num_levels;

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = GetImageLayout(resource_state);
    barrier.newLayout = GetImageLayout(new_state);
    barrier.srcAccessMask = GetAccessMask(resource_state);
    barrier.dstAccessMask = GetAccessMask(new_state);
    barrier.image = image;
    barrier.subresourceRange = range;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        command_buffer->GetCommandBuffer(),
        GetShaderStageMask(resource_state, true),
        GetShaderStageMask(new_state, false),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    resource_state = new_state;
}

void GPUImageMemory<Platform::VULKAN>::InsertSubResourceBarrier(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state
)
{
    if (image == nullptr) {
        DebugLog(
            LogType::Warn,
            "Attempt to insert a resource barrier but image was not defined\n"
        );

        return;
    }

    ResourceState prev_resource_state = resource_state;

    auto it = sub_resources.find(sub_resource);

    if (it != sub_resources.end()) {
        prev_resource_state = it->second;
    }

    const VkImageAspectFlags aspect_flag_bits = 
        (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_DEPTH ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)
        | (sub_resource.flags & IMAGE_SUB_RESOURCE_FLAGS_STENCIL ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);

    VkImageSubresourceRange range{};
    range.aspectMask = aspect_flag_bits;
    range.baseArrayLayer = sub_resource.base_array_layer;
    range.layerCount = sub_resource.num_layers;
    range.baseMipLevel = sub_resource.base_mip_level;
    range.levelCount = sub_resource.num_levels;

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = GetImageLayout(prev_resource_state);
    barrier.newLayout = GetImageLayout(new_state);
    barrier.srcAccessMask = GetAccessMask(prev_resource_state);
    barrier.dstAccessMask = GetAccessMask(new_state);
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = range;

    vkCmdPipelineBarrier(
        command_buffer->GetCommandBuffer(),
        GetShaderStageMask(prev_resource_state, true),
        GetShaderStageMask(new_state, false),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    sub_resources.emplace(sub_resource, new_state);
}

auto GPUImageMemory<Platform::VULKAN>::GetSubResourceState(const ImageSubResource &sub_resource) const -> ResourceState
{
    const auto it = sub_resources.find(sub_resource);

    if (it == sub_resources.end()) {
        return resource_state;
    }

    return it->second;
}

void GPUImageMemory<Platform::VULKAN>::SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state)
{
    sub_resources[sub_resource] = new_state;
}

Result GPUImageMemory<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, PlatformImage new_image)
{
    if (image != VK_NULL_HANDLE) {
        DebugLog(
            LogType::Warn,
            "Create() called on an image (memory #%u) that has not been destroyed!\n"
            "\tYou should explicitly call Destroy() on the object before reallocating it.\n"
            "\tTo prevent memory leaks, calling Destroy() before allocating the memory...\n",
            m_id
        );

#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif

        HYPERION_BUBBLE_ERRORS(Destroy(device));
    }

    image = new_image;
    is_image_owned = false;

    allocation = VK_NULL_HANDLE;

    HYPERION_RETURN_OK;
}

Result GPUImageMemory<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, SizeType size, VkImageCreateInfo *image_info)
{
    if (image != VK_NULL_HANDLE) {
        DebugLog(
            LogType::Warn,
            "Create() called on an image (memory #%u) that has not been destroyed!\n"
            "\tYou should explicitly call Destroy() on the object before reallocating it.\n"
            "\tTo prevent memory leaks, calling Destroy() before allocating the memory...\n",
            m_id
        );

#ifdef HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif

        HYPERION_BUBBLE_ERRORS(Destroy(device));
    }

    this->size = size;

    GPUMemory<Platform::VULKAN>::Create();

    VmaAllocationCreateInfo alloc_info { };
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.pUserData = reinterpret_cast<void *>(uintptr_t(uint64(ID_MASK_IMAGE) | uint64(m_id)));

    HYPERION_VK_CHECK_MSG(
        vmaCreateImage(
            device->GetAllocator(),
            image_info,
            &alloc_info,
            &image,
            &allocation,
            nullptr
        ),
        "Failed to create gpu image!"
    );

    is_image_owned = true;

    HYPERION_RETURN_OK;
}

Result GPUImageMemory<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (is_image_owned) {
        GPUMemory<Platform::VULKAN>::Destroy();
    }

    if (map != nullptr) {
        Unmap(device);
    }

    if (is_image_owned) {
        vmaDestroyImage(device->GetAllocator(), image, allocation);
    }

    image = VK_NULL_HANDLE;
    allocation = VK_NULL_HANDLE;

    is_image_owned = false;

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
