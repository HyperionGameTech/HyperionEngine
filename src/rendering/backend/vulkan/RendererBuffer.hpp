/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP

#include <system/vma/VmaUsage.hpp>

namespace hyperion {
namespace renderer {

using PlatformImage = VkImage;

namespace platform {

template <>
struct GPUBufferPlatformImpl<Platform::VULKAN>
{
    GPUBuffer<Platform::VULKAN> *self = nullptr;

    VkBuffer                    handle = VK_NULL_HANDLE;

    VkBufferUsageFlags          vk_buffer_usage_flags = 0;
    VmaMemoryUsage              vma_usage = VMA_MEMORY_USAGE_UNKNOWN;
    VmaAllocationCreateFlags    vma_allocation_create_flags = 0;
    VmaAllocation               vma_allocation = VK_NULL_HANDLE;
    VkDeviceSize                size = 0;

    mutable void                *mapping = nullptr;

    void Map(Device<Platform::VULKAN> *device) const;
    void Unmap(Device<Platform::VULKAN> *device) const;

    Result CheckCanAllocate(
        Device<Platform::VULKAN> *device,
        const VkBufferCreateInfo &buffer_create_info,
        const VmaAllocationCreateInfo &allocation_create_info,
        SizeType size
    ) const;

    VmaAllocationCreateInfo GetAllocationCreateInfo(Device<Platform::VULKAN> *device) const;
    VkBufferCreateInfo GetBufferCreateInfo(Device<Platform::VULKAN> *device) const;
};

// template <>
// class GPUMemory<Platform::VULKAN>
// {
// public:
//     static uint FindMemoryType(Device<Platform::VULKAN> *device, uint vk_type_filter, VkMemoryPropertyFlags properties);

//     HYP_API GPUMemory();

//     GPUMemory(const GPUMemory &other)               = delete;
//     GPUMemory &operator=(const GPUMemory &other)    = delete;

//     HYP_API GPUMemory(GPUMemory &&other) noexcept;
//     GPUMemory &operator=(GPUMemory &&other)         = delete;

//     HYP_API ~GPUMemory();

//     ResourceState GetResourceState() const
//         { return resource_state; }

//     void SetResourceState(ResourceState new_state)
//         { resource_state = new_state; }

//     void SetID(uint id)
//         { m_id = id; }

//     uint GetID() const
//         { return m_id; }

//     bool IsCreated() const
//         { return m_is_created; }

//     void *GetMapping(Device<Platform::VULKAN> *device) const
//     {
//         if (!map) {
//             Map(device, &map);
//         }

//         return map;
//     }

//     HYP_API void Memset(Device<Platform::VULKAN> *device, SizeType count, ubyte value);

//     HYP_API void Copy(Device<Platform::VULKAN> *device, SizeType count, const void *ptr);
//     HYP_API void Copy(Device<Platform::VULKAN> *device, SizeType offset, SizeType count, const void *ptr);

//     HYP_API void Read(Device<Platform::VULKAN> *device, SizeType count, void *out_ptr) const;
    
//     VmaAllocation allocation;
//     VkDeviceSize size;

// protected:
//     HYP_API void Map(Device<Platform::VULKAN> *device, void **ptr) const;
//     HYP_API void Unmap(Device<Platform::VULKAN> *device) const;
//     HYP_API void Create();
//     HYP_API void Destroy();

//     uint                    m_id;
//     bool                    m_is_created;

//     uint                    sharing_mode;
//     mutable void            *map;
//     mutable ResourceState   resource_state;
// };

/* buffers */

// template <>
// class GPUBuffer<Platform::VULKAN> : public GPUMemory<Platform::VULKAN>
// {
// public:
//     HYP_API GPUBuffer(GPUBufferType type);

//     GPUBuffer(const GPUBuffer &other)                   = delete;
//     GPUBuffer &operator=(const GPUBuffer &other)        = delete;

//     GPUBuffer(GPUBuffer &&other) noexcept;
//     GPUBuffer &operator=(GPUBuffer &&other) noexcept    = delete;

//     HYP_API ~GPUBuffer();

//     GPUBufferType GetBufferType() const
//         { return type; }

//     bool IsCPUAccessible() const
//         { return vma_usage != VMA_MEMORY_USAGE_GPU_ONLY; }

//     HYP_API void InsertBarrier(
//         CommandBuffer<Platform::VULKAN> *command_buffer,
//         ResourceState new_state
//     ) const;

//     HYP_API void InsertBarrier(
//         CommandBuffer<Platform::VULKAN> *command_buffer,
//         ResourceState new_state,
//         ShaderModuleType shader_type
//     ) const;

//     HYP_API void CopyFrom(
//         CommandBuffer<Platform::VULKAN> *command_buffer,
//         const GPUBuffer *src_buffer,
//         SizeType count
//     );

//     HYP_API Result CopyStaged(
//         Instance<Platform::VULKAN> *instance,
//         const void *ptr,
//         SizeType count
//     );

//     HYP_API Result ReadStaged(
//         Instance<Platform::VULKAN> *instance,
//         SizeType count,
//         void *out_ptr
//     ) const;

//     Result CheckCanAllocate(Device<Platform::VULKAN> *device, SizeType size) const;

//     /*! \brief Calls vkGetBufferDeviceAddressKHR. Only use this if the extension is enabled */
//     uint64_t GetBufferDeviceAddress(Device<Platform::VULKAN> *device) const;

//     HYP_API Result Create(
//         Device<Platform::VULKAN> *device,
//         SizeType buffer_size,
//         SizeType buffer_alignment = 0
//     );
//     HYP_API Result Destroy(Device<Platform::VULKAN> *device);
//     HYP_API Result EnsureCapacity(
//         Device<Platform::VULKAN> *device,
//         SizeType minimum_size,
//         bool *out_size_changed = nullptr
//     );
//     HYP_API Result EnsureCapacity(
//         Device<Platform::VULKAN> *device,
//         SizeType minimum_size,
//         SizeType alignment,
//         bool *out_size_changed = nullptr
//     );

//     VkBuffer                    buffer;
    
// private:
//     Result CheckCanAllocate(
//         Device<Platform::VULKAN> *device,
//         const VkBufferCreateInfo &buffer_create_info,
//         const VmaAllocationCreateInfo &allocation_create_info,
//         SizeType size
//     ) const;

//     VmaAllocationCreateInfo GetAllocationCreateInfo(Device<Platform::VULKAN> *device) const;
//     VkBufferCreateInfo GetBufferCreateInfo(Device<Platform::VULKAN> *device) const;

//     GPUBufferType               type;

//     VkBufferUsageFlags          usage_flags;
//     VmaMemoryUsage              vma_usage;
//     VmaAllocationCreateFlags    vma_allocation_create_flags;
// };

template <>
class ShaderBindingTableBuffer<Platform::VULKAN> : public GPUBuffer<Platform::VULKAN>
{
public:
    ShaderBindingTableBuffer();

    VkStridedDeviceAddressRegionKHR region;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_BUFFER_HPP
