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

    void Map() const;
    void Unmap() const;

    RendererResult CheckCanAllocate(
        const VkBufferCreateInfo &buffer_create_info,
        const VmaAllocationCreateInfo &allocation_create_info,
        SizeType size
    ) const;

    VmaAllocationCreateInfo GetAllocationCreateInfo() const;
    VkBufferCreateInfo GetBufferCreateInfo() const;
};

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
