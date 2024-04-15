/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererShader.hpp>

#include <Types.hpp>

#include <system/vma/VmaUsage.hpp>

#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <ctime>

namespace hyperion {
namespace renderer {

using PlatformImage = VkImage;

namespace platform {

template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class CommandBuffer;

template <>
class GPUMemory<Platform::VULKAN>
{
public:
    struct Stats
    {
        SizeType    gpu_memory_used = 0;
        SizeType    last_gpu_memory_used = 0;
        int64       last_diff = 0;
        std::time_t last_timestamp = 0;
        const int64 time_diff = 10000;

        HYP_FORCE_INLINE void IncMemoryUsage(SizeType amount)
        {
            gpu_memory_used += amount;

            UpdateStats();
        }

        HYP_FORCE_INLINE void DecMemoryUsage(SizeType amount)
        {
            AssertThrow(gpu_memory_used >= amount);

            gpu_memory_used -= amount;

            UpdateStats();
        }

        HYP_FORCE_INLINE void UpdateStats()
        {
            const std::time_t now = std::time(nullptr);

            if (last_timestamp == 0 || now - last_timestamp >= time_diff) {
                if (gpu_memory_used > last_gpu_memory_used) {
                    last_diff = gpu_memory_used - last_gpu_memory_used;
                } else {
                    last_diff = last_gpu_memory_used - gpu_memory_used;
                }

                last_timestamp = now;
                last_gpu_memory_used = gpu_memory_used;
            }
        }
    };

    static uint FindMemoryType(Device<Platform::VULKAN> *device, uint vk_type_filter, VkMemoryPropertyFlags properties);
    static VkImageLayout GetImageLayout(ResourceState state);
    static VkAccessFlags GetAccessMask(ResourceState state);
    static VkPipelineStageFlags GetShaderStageMask(
        ResourceState state,
        bool src,
        ShaderModuleType shader_type = ShaderModuleType::UNSET
    );

    HYP_API GPUMemory();

    GPUMemory(const GPUMemory &other)               = delete;
    GPUMemory &operator=(const GPUMemory &other)    = delete;

    HYP_API GPUMemory(GPUMemory &&other) noexcept;
    GPUMemory &operator=(GPUMemory &&other)         = delete;

    HYP_API ~GPUMemory();

    ResourceState GetResourceState() const
        { return resource_state; }

    void SetResourceState(ResourceState new_state)
        { resource_state = new_state; }

    void SetID(uint id)
        { m_id = id; }

    uint GetID() const
        { return m_id; }

    bool IsCreated() const
        { return m_is_created; }

    void *GetMapping(Device<Platform::VULKAN> *device) const
    {
        if (!map) {
            Map(device, &map);
        }

        return map;
    }

    HYP_API void Memset(Device<Platform::VULKAN> *device, SizeType count, ubyte value);

    HYP_API void Copy(Device<Platform::VULKAN> *device, SizeType count, const void *ptr);
    HYP_API void Copy(Device<Platform::VULKAN> *device, SizeType offset, SizeType count, const void *ptr);

    HYP_API void Read(Device<Platform::VULKAN> *device, SizeType count, void *out_ptr) const;
    
    VmaAllocation allocation;
    VkDeviceSize size;
    
    static Stats stats;

protected:
    HYP_API void Map(Device<Platform::VULKAN> *device, void **ptr) const;
    HYP_API void Unmap(Device<Platform::VULKAN> *device) const;
    HYP_API void Create();
    HYP_API void Destroy();

    uint                    m_id;
    bool                    m_is_created;

    uint                    sharing_mode;
    mutable void            *map;
    mutable ResourceState   resource_state;
};

/* buffers */

template <>
class GPUBuffer<Platform::VULKAN> : public GPUMemory<Platform::VULKAN>
{
public:
    HYP_API GPUBuffer(GPUBufferType type);

    GPUBuffer(const GPUBuffer &other)                   = delete;
    GPUBuffer &operator=(const GPUBuffer &other)        = delete;

    GPUBuffer(GPUBuffer &&other) noexcept;
    GPUBuffer &operator=(GPUBuffer &&other) noexcept    = delete;

    HYP_API ~GPUBuffer();

    GPUBufferType GetBufferType() const
        { return type; }

    bool IsCPUAccessible() const
        { return vma_usage != VMA_MEMORY_USAGE_GPU_ONLY; }

    HYP_API void InsertBarrier(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        ResourceState new_state
    ) const;

    HYP_API void InsertBarrier(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        ResourceState new_state,
        ShaderModuleType shader_type
    ) const;

    HYP_API void CopyFrom(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const GPUBuffer *src_buffer,
        SizeType count
    );

    HYP_API Result CopyStaged(
        Instance<Platform::VULKAN> *instance,
        const void *ptr,
        SizeType count
    );

    HYP_API Result ReadStaged(
        Instance<Platform::VULKAN> *instance,
        SizeType count,
        void *out_ptr
    ) const;

    Result CheckCanAllocate(Device<Platform::VULKAN> *device, SizeType size) const;

    /* \brief Calls vkGetBufferDeviceAddressKHR. Only use this if the extension is enabled */
    uint64_t GetBufferDeviceAddress(Device<Platform::VULKAN> *device) const;

    HYP_API Result Create(
        Device<Platform::VULKAN> *device,
        SizeType buffer_size,
        SizeType buffer_alignment = 0
    );
    HYP_API Result Destroy(Device<Platform::VULKAN> *device);
    HYP_API Result EnsureCapacity(
        Device<Platform::VULKAN> *device,
        SizeType minimum_size,
        bool *out_size_changed = nullptr
    );
    HYP_API Result EnsureCapacity(
        Device<Platform::VULKAN> *device,
        SizeType minimum_size,
        SizeType alignment,
        bool *out_size_changed = nullptr
    );

    VkBuffer                    buffer;
    
private:
    Result CheckCanAllocate(
        Device<Platform::VULKAN> *device,
        const VkBufferCreateInfo &buffer_create_info,
        const VmaAllocationCreateInfo &allocation_create_info,
        SizeType size
    ) const;

    VmaAllocationCreateInfo GetAllocationCreateInfo(Device<Platform::VULKAN> *device) const;
    VkBufferCreateInfo GetBufferCreateInfo(Device<Platform::VULKAN> *device) const;

    GPUBufferType               type;

    VkBufferUsageFlags          usage_flags;
    VmaMemoryUsage              vma_usage;
    VmaAllocationCreateFlags    vma_allocation_create_flags;
};

template <>
class ShaderBindingTableBuffer<Platform::VULKAN> : public GPUBuffer<Platform::VULKAN>
{
public:
    ShaderBindingTableBuffer();

    VkStridedDeviceAddressRegionKHR region;
};

/* images */

template <>
class GPUImageMemory<Platform::VULKAN> : public GPUMemory<Platform::VULKAN>
{
public:
    enum class Aspect
    {
        COLOR,
        DEPTH
    };

    HYP_API GPUImageMemory();
    GPUImageMemory(const GPUImageMemory &other)             = delete;
    GPUImageMemory &operator=(const GPUImageMemory &other)  = delete;
    HYP_API GPUImageMemory(GPUImageMemory &&other) noexcept;
    GPUImageMemory &operator=(GPUImageMemory &&other)       = delete;
    HYP_API ~GPUImageMemory();

    ResourceState GetSubResourceState(const ImageSubResource &sub_resource) const;
    void SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state);

    void SetResourceState(ResourceState new_state);

    HYP_API void InsertBarrier(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        ResourceState new_state,
        ImageSubResourceFlagBits flags = ImageSubResourceFlags::IMAGE_SUB_RESOURCE_FLAGS_COLOR
    );

    HYP_API void InsertBarrier(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );

    HYP_API void InsertSubResourceBarrier(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );

    HYP_API Result Create(Device<Platform::VULKAN> *device, PlatformImage image);

    HYP_API Result Create(Device<Platform::VULKAN> *device, SizeType size, VkImageCreateInfo *image_info);
    HYP_API Result Destroy(Device<Platform::VULKAN> *device);

    PlatformImage image;

private:
    std::unordered_map<ImageSubResource, ResourceState> sub_resources;
    bool is_image_owned;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP
