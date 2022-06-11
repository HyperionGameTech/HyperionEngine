#include "renderer_buffer.h"
#include "renderer_command_buffer.h"
#include "renderer_device.h"
#include "renderer_helpers.h"
#include "renderer_features.h"
#include "renderer_instance.h"

#include <math/math_util.h>

#include <system/debug.h>

#include <vector>
#include <queue>
#include <cstring>

namespace hyperion {
namespace renderer {

StagingBuffer *StagingBufferPool::Context::Acquire(size_t required_size)
{
    if (required_size == 0) {
        DebugLog(LogType::Warn, "Attempt to acquire staging buffer of 0 size\n");

        return nullptr;
    }
    
    const size_t new_size = MathUtil::NextPowerOf2(required_size);

    StagingBuffer *staging_buffer = m_pool->FindStagingBuffer(required_size - 1);

    if (staging_buffer != nullptr && m_used.find(staging_buffer) == m_used.end()) {
        DebugLog(
            LogType::Debug,
            "Requested staging buffer of size %llu, reusing existing staging buffer of size %llu\n",
            required_size,
            staging_buffer->size
        );
    } else {
        DebugLog(
            LogType::Debug,
            "Staging buffer of size >= %llu not found, creating one of size %llu at time %lld\n",
            required_size,
            new_size,
            std::time(nullptr)
        );

        staging_buffer = CreateStagingBuffer(new_size);
    }

    m_used.insert(staging_buffer);

    return staging_buffer;
}

StagingBuffer *StagingBufferPool::Context::CreateStagingBuffer(size_t size)
{
    const std::time_t current_time = std::time(nullptr);

    DebugLog(
        LogType::Debug,
        "Creating staging buffer of size %llu at time %lld\n",
        size,
        current_time
    );

    auto staging_buffer = std::make_unique<StagingBuffer>();

    if (!staging_buffer->Create(m_device, size)) {
        return nullptr;
    }

    m_staging_buffers.push_back(StagingBufferRecord{
        .size = size,
        .buffer = std::move(staging_buffer),
        .last_used = current_time
    });

    return m_staging_buffers.back().buffer.get();
}

StagingBuffer *StagingBufferPool::FindStagingBuffer(size_t size)
{
    /* do a binary search to find one with the closest size (never less than required) */
    const auto bound = std::upper_bound(
        m_staging_buffers.begin(),
        m_staging_buffers.end(),
        size,
        [](const size_t &sz, const auto &it) {
            return sz < it.size;
        }
    );

    if (bound != m_staging_buffers.end()) {
        /* Update the time so that frequently used buffers will stay in the pool longer */
        bound->last_used = std::time(nullptr);

        return bound->buffer.get();
    }

    return nullptr;
}

Result StagingBufferPool::Use(Device *device, UseFunction &&fn)
{
    auto result = Result::OK;

    Context context(this, device);

    HYPERION_PASS_ERRORS(fn(context), result);

    for (auto &record : context.m_staging_buffers) {
        const auto bound = std::upper_bound(
            m_staging_buffers.begin(),
            m_staging_buffers.end(),
            record,
            [](const auto &record, const auto &it) {
                return record.size < it.size;
            }
        );

        m_staging_buffers.insert(bound, std::move(record));
    }
    
    if (++use_calls % gc_threshold == 0) {
        HYPERION_PASS_ERRORS(GC(device), result);
    }

    return result;
}

Result StagingBufferPool::GC(Device *device)
{
    const auto current_time = std::time(nullptr);

    DebugLog(LogType::Debug, "Clean up staging buffers from pool\n");

    std::queue<std::vector<StagingBufferRecord>::iterator> to_remove;

    for (auto it = m_staging_buffers.begin(); it != m_staging_buffers.end(); ++it) {
        if (current_time - it->last_used > hold_time) {
            to_remove.push(it);
        }
    }

    if (to_remove.empty()) {
        DebugLog(
            LogType::Debug,
            "No staging buffers queued for removal.\n"
        );
    }

    DebugLog(LogType::Debug, "Removing %llu staging buffers from pool\n", to_remove.size());

    auto result = Result::OK;

    while (!to_remove.empty()) {
        HYPERION_PASS_ERRORS(to_remove.front()->buffer->Destroy(device), result);

        m_staging_buffers.erase(to_remove.front());

        to_remove.pop();
    }

    return result;
}

Result StagingBufferPool::Destroy(Device *device)
{
    auto result = Result::OK;

    for (auto &record : m_staging_buffers) {
        HYPERION_PASS_ERRORS(record.buffer->Destroy(device), result);
    }

    m_staging_buffers.clear();

    use_calls = 0;

    return result;
}

uint32_t GPUMemory::FindMemoryType(Device *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(device->GetPhysicalDevice(), &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((vk_type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            DebugLog(LogType::Info, "Found Memory type [%d]!\n", i);
            return i;
        }
    }

    AssertThrowMsg(nullptr, "Could not find suitable memory type!\n");
}

VkImageLayout GPUMemory::GetImageLayout(ResourceState state)
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

VkAccessFlags GPUMemory::GetAccessMask(ResourceState state)
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

VkPipelineStageFlags GPUMemory::GetShaderStageMask(ResourceState state, bool src, ShaderModule::Type shader_type)
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
        case ShaderModule::Type::VERTEX:
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case ShaderModule::Type::FRAGMENT:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case ShaderModule::Type::COMPUTE:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case ShaderModule::Type::RAY_ANY_HIT:
        case ShaderModule::Type::RAY_CLOSEST_HIT:
        case ShaderModule::Type::RAY_GEN:
        case ShaderModule::Type::RAY_INTERSECT:
        case ShaderModule::Type::RAY_MISS:
            return VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        case ShaderModule::Type::GEOMETRY:
            return VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
        case ShaderModule::Type::TESS_CONTROL:
            return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
        case ShaderModule::Type::TESS_EVAL:
            return VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
        case ShaderModule::Type::MESH:
            return VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV;
        case ShaderModule::Type::TASK:
            return VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV;
        case ShaderModule::Type::UNSET:
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

GPUMemory::Stats GPUMemory::stats{};

GPUMemory::GPUMemory()
    : sharing_mode(VK_SHARING_MODE_EXCLUSIVE),
      size(0),
      index(0),
      map(nullptr),
      resource_state(ResourceState::UNDEFINED)
{
    static uint32_t allocations = 0;

    index = allocations++;
}

GPUMemory::~GPUMemory()
{
}

void GPUMemory::Map(Device *device, void **ptr) const
{
    vmaMapMemory(device->GetAllocator(), this->allocation, ptr);
}

void GPUMemory::Unmap(Device *device) const
{
    vmaUnmapMemory(device->GetAllocator(), this->allocation);
    map = nullptr;
}

void GPUMemory::Copy(Device *device, size_t count, const void *ptr)
{
    if (map == nullptr) {
        Map(device, &map);
    }

    std::memcpy(map, ptr, count);
}

void GPUMemory::Copy(Device *device, size_t offset, size_t count, const void *ptr)
{
    if (map == nullptr) {
        Map(device, &map);
    }

    std::memcpy(reinterpret_cast<void *>(uintptr_t(map) + offset), ptr, count);
}

void GPUMemory::Read(Device *device, size_t count, void *out_ptr) const
{
    if (map == nullptr) {
        Map(device, &map);
        DebugLog(LogType::Warn, "Attempt to Read() from buffer but data has not been mapped previously\n");
    }

    std::memcpy(out_ptr, map, count);
}

void GPUMemory::Create()
{
    DebugLog(LogType::Debug, "Allocate GPUMemory %u\n", index);

    stats.IncMemoryUsage(size);
}

void GPUMemory::Destroy()
{
    DebugLog(LogType::Debug, "Destroy GPUMemory %u\n", index);

    stats.DecMemoryUsage(size);
}


GPUBuffer::GPUBuffer(VkBufferUsageFlags usage_flags,
    VmaMemoryUsage vma_usage,
    VmaAllocationCreateFlags vma_allocation_create_flags
) : GPUMemory(),
    buffer(nullptr),
    usage_flags(usage_flags),
    vma_usage(vma_usage),
    vma_allocation_create_flags(vma_allocation_create_flags)
{
}

GPUBuffer::~GPUBuffer()
{
    AssertThrowMsg(buffer == nullptr, "buffer should have been destroyed!");
}

VkBufferCreateInfo GPUBuffer::GetBufferCreateInfo(Device *device) const
{
    const QueueFamilyIndices &qf_indices = device->GetQueueFamilyIndices();
    const uint32_t buffer_family_indices[] = { qf_indices.graphics_family.value(), qf_indices.compute_family.value() };

    VkBufferCreateInfo vk_buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    vk_buffer_info.size                  = size;
    vk_buffer_info.usage                 = usage_flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vk_buffer_info.pQueueFamilyIndices   = buffer_family_indices;
    vk_buffer_info.queueFamilyIndexCount = uint32_t(std::size(buffer_family_indices));

    return vk_buffer_info;
}

VmaAllocationCreateInfo GPUBuffer::GetAllocationCreateInfo(Device *device) const
{
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.flags = vma_allocation_create_flags;
    alloc_info.usage = vma_usage;
    alloc_info.pUserData = reinterpret_cast<void *>(index);

    return alloc_info;
}

Result GPUBuffer::CheckCanAllocate(Device *device, size_t size) const
{
    const auto create_info = GetBufferCreateInfo(device);
    const auto alloc_info = GetAllocationCreateInfo(device);

    return CheckCanAllocate(device, create_info, alloc_info, this->size);
}

uint64_t GPUBuffer::GetBufferDeviceAddress(Device *device) const
{
    AssertThrowMsg(
        device->GetFeatures().GetBufferDeviceAddressFeatures().bufferDeviceAddress,
        "Called GetBufferDeviceAddress() but the buffer device address extension feature is not supported or enabled!"
    );

    AssertThrow(buffer != VK_NULL_HANDLE);

	VkBufferDeviceAddressInfoKHR info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
	info.buffer = buffer;

	return device->GetFeatures().dyn_functions.vkGetBufferDeviceAddressKHR(
        device->GetDevice(),
        &info
    );
}

Result GPUBuffer::CheckCanAllocate(
    Device *device,
    const VkBufferCreateInfo &buffer_create_info,
    const VmaAllocationCreateInfo &allocation_create_info,
    size_t size
) const
{
    const Features &features = device->GetFeatures();

    auto result = Result::OK;

    uint32_t memory_type_index = UINT32_MAX;

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
    const auto &heap      = memory_properties.memoryHeaps[heap_index];

    if (heap.size < size) {
        return {Result::RENDERER_ERR, "Heap size is less than requested size. "
            "Maybe the wrong memory type has been requested, or the device is out of memory."};
    }

    return result;
}

void GPUBuffer::InsertBarrier(CommandBuffer *command_buffer, ResourceState new_state) const
{
    if (buffer == nullptr) {
        DebugLog(
            LogType::Warn,
            "Attempt to insert a resource barrier but buffer was not defined\n"
        );

        return;
    }

    VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcAccessMask       = GetAccessMask(resource_state);
    barrier.dstAccessMask       = GetAccessMask(new_state);
    barrier.buffer              = buffer;
    barrier.offset              = 0;
    barrier.size                = size;
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

void GPUBuffer::CopyFrom(CommandBuffer *command_buffer,
    const GPUBuffer *src_buffer,
    size_t count)
{
    InsertBarrier(command_buffer, ResourceState::COPY_DST);
    src_buffer->InsertBarrier(command_buffer, ResourceState::COPY_SRC);

    VkBufferCopy region{};
    region.size = count;

    vkCmdCopyBuffer(
        command_buffer->GetCommandBuffer(),
        src_buffer->buffer,
        buffer,
        1,
        &region
    );
}

Result GPUBuffer::CopyStaged(
    Instance *instance,
    const void *ptr,
    size_t count
)
{
    Device *device = instance->GetDevice();

    return instance->GetStagingBufferPool().Use(device, [&](StagingBufferPool::Context &holder) {
        auto commands = instance->GetSingleTimeCommands();

        auto *staging_buffer = holder.Acquire(count);
        staging_buffer->Copy(device, count, ptr);
        
        commands.Push([&](CommandBuffer *cmd) {
            CopyFrom(cmd, staging_buffer, count);

            HYPERION_RETURN_OK;
        });
    
        return commands.Execute(device);
    });
}

Result GPUBuffer::Create(Device *device, size_t size)
{
    if (buffer != nullptr) {
        DebugLog(
            LogType::Warn,
            "Create() called on a buffer (memory #%lu) that has not been destroyed!\n"
            "\tYou should explicitly call Destroy() on the object before reallocating it.\n"
            "\tTo prevent memory leaks, calling Destroy() before allocating the memory...\n",
            index
        );

#if HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif

        HYPERION_BUBBLE_ERRORS(Destroy(device));
    }

    this->size = size;

    GPUMemory::Create();
    
    if (size == 0) {
#if HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif
        return {Result::RENDERER_ERR, "Creating empty gpu buffer will result in errors! \n"};
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

    HYPERION_RETURN_OK;
}

Result GPUBuffer::Destroy(Device *device)
{
    GPUMemory::Destroy();

    if (map != nullptr) {
        Unmap(device);
    }

    vmaDestroyBuffer(device->GetAllocator(), buffer, allocation);
    buffer = nullptr;
    allocation = nullptr;

    HYPERION_RETURN_OK;
}

Result GPUBuffer::EnsureCapacity(Device *device,
    size_t minimum_size,
    bool *out_size_changed)
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

    HYPERION_PASS_ERRORS(Create(device, minimum_size), result);

    if (out_size_changed != nullptr) {
        *out_size_changed = bool(result);
    }

    return result;
}

#if HYP_DEBUG_MODE

void GPUBuffer::DebugLogBuffer(Device *device) const
{
    if (size % sizeof(uint32_t) == 0) {
        const auto data = DebugReadBytes<uint32_t>(device);

        for (size_t i = 0; i < data.size();) {
            const size_t dist = MathUtil::Min(data.size() - i, size_t(4));

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
        const auto data = DebugReadBytes(device);

        for (const auto byte : data) {
            DebugLog(LogType::Debug, "0x%.12\n", byte);
        }
    }
}

#endif


VertexBuffer::VertexBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY
      )
{
}

void VertexBuffer::Bind(CommandBuffer *cmd)
{
    const VkBuffer vertex_buffers[] = {buffer};
    const VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(cmd->GetCommandBuffer(), 0, 1, vertex_buffers, offsets);
}

IndexBuffer::IndexBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY
      )
{
}

void IndexBuffer::Bind(CommandBuffer *cmd)
{
    vkCmdBindIndexBuffer(
        cmd->GetCommandBuffer(),
        buffer,
        0,
        helpers::ToVkIndexType(GetDatumType())
    );
}

UniformBuffer::UniformBuffer()
    : GPUBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
{
}

StorageBuffer::StorageBuffer()
    : GPUBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
{
}

AtomicCounterBuffer::AtomicCounterBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY
      )
{
}

StagingBuffer::StagingBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          VMA_MEMORY_USAGE_CPU_ONLY
      )
{
}

IndirectBuffer::IndirectBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY
      )
{
}

void IndirectBuffer::DispatchIndirect(CommandBuffer *command_buffer, size_t offset) const
{
    vkCmdDispatchIndirect(
        command_buffer->GetCommandBuffer(),
        buffer,
        offset
    );
}

ShaderBindingTableBuffer::ShaderBindingTableBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
      ),
      region{}
{
}

AccelerationStructureBuffer::AccelerationStructureBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
      )
{
}

AccelerationStructureInstancesBuffer::AccelerationStructureInstancesBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
      )
{
}

PackedVertexStorageBuffer::PackedVertexStorageBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,                                  /* for rt */
          VMA_MEMORY_USAGE_GPU_ONLY
      )
{
}

PackedIndexStorageBuffer::PackedIndexStorageBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT                            /* for rt */
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR /* for rt */
            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,                                  /* for rt */
          VMA_MEMORY_USAGE_GPU_ONLY
      )
{
}

ScratchBuffer::ScratchBuffer()
    : GPUBuffer(
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
      )
{
}



GPUImageMemory::GPUImageMemory()
    : GPUMemory(),
      image(nullptr)
{
}

GPUImageMemory::~GPUImageMemory()
{
    std::cout << "Dealloc image\n";
    AssertThrowMsg(image == nullptr, "image should have been destroyed!");
}

void GPUImageMemory::SetResourceState(ResourceState new_state)
{
    resource_state = new_state;

    sub_resources.clear();
}

void GPUImageMemory::InsertBarrier(
    CommandBuffer *command_buffer,
    ResourceState new_state)
{
    InsertBarrier(
        command_buffer,
        ImageSubResource{
            .num_layers = VK_REMAINING_ARRAY_LAYERS,
            .num_levels = VK_REMAINING_MIP_LEVELS
        },
        new_state
    );
}

void GPUImageMemory::InsertBarrier(
    CommandBuffer *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state)
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

    VkImageSubresourceRange range{};
    range.aspectMask     = sub_resource.aspect_mask;
    range.baseArrayLayer = sub_resource.base_array_layer;
    range.layerCount     = sub_resource.num_layers;
    range.baseMipLevel   = sub_resource.base_mip_level;
    range.levelCount     = sub_resource.num_levels;

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout           = GetImageLayout(resource_state);
    barrier.newLayout           = GetImageLayout(new_state);
    barrier.srcAccessMask       = GetAccessMask(resource_state);
    barrier.dstAccessMask       = GetAccessMask(new_state);
    barrier.image               = image;
    barrier.subresourceRange    = range;
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

void GPUImageMemory::InsertSubResourceBarrier(
    CommandBuffer *command_buffer,
    const ImageSubResource &sub_resource,
    ResourceState new_state)
{
    if (image == nullptr) {
        DebugLog(
            LogType::Warn,
            "Attempt to insert a resource barrier but image was not defined\n"
        );

        return;
    }

    VkImageSubresourceRange range{};
    range.aspectMask     = sub_resource.aspect_mask;
    range.baseArrayLayer = sub_resource.base_array_layer;
    range.layerCount     = sub_resource.num_layers;
    range.baseMipLevel   = sub_resource.base_mip_level;
    range.levelCount     = sub_resource.num_levels;

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout           = GetImageLayout(resource_state);
    barrier.newLayout           = GetImageLayout(new_state);
    barrier.srcAccessMask       = GetAccessMask(resource_state);
    barrier.dstAccessMask       = GetAccessMask(new_state);
    barrier.image               = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange    = range;

    vkCmdPipelineBarrier(
        command_buffer->GetCommandBuffer(),
        GetShaderStageMask(resource_state, true),
        GetShaderStageMask(new_state, false),
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    sub_resources.emplace(sub_resource, new_state);
}

auto GPUImageMemory::GetSubResourceState(const ImageSubResource &sub_resource) const -> ResourceState
{
    const auto it = sub_resources.find(sub_resource);

    if (it == sub_resources.end()) {
        return resource_state;
    }

    return it->second;
}

void GPUImageMemory::SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state)
{
    sub_resources[sub_resource] = new_state;
}

Result GPUImageMemory::Create(Device *device, size_t size, VkImageCreateInfo *image_info)
{
    if (image != nullptr) {
        DebugLog(
            LogType::Warn,
            "Create() called on an image (memory #%lu) that has not been destroyed!\n"
            "\tYou should explicitly call Destroy() on the object before reallocating it.\n"
            "\tTo prevent memory leaks, calling Destroy() before allocating the memory...\n",
            index
        );

#if HYP_DEBUG_MODE
        HYP_BREAKPOINT;
#endif

        HYPERION_BUBBLE_ERRORS(Destroy(device));
    }

    this->size = size;

    GPUMemory::Create();

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage     = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.pUserData = reinterpret_cast<void *>(uintptr_t(index));

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

    HYPERION_RETURN_OK;
}

Result GPUImageMemory::Destroy(Device *device)
{
    GPUMemory::Destroy();

    if (map != nullptr) {
        Unmap(device);
    }

    vmaDestroyImage(device->GetAllocator(), image, allocation);

    image = nullptr;
    allocation = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion
