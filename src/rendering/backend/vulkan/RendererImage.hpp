/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef RENDERER_IMAGE_HPP
#define RENDERER_IMAGE_HPP

#include <rendering/backend/RendererBuffer.hpp>
#include <core/math/MathUtil.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class CommandBuffer;

template <>
struct ImagePlatformImpl<Platform::VULKAN>
{
    Image<Platform::VULKAN>                     *self = nullptr;
    VkImage                                     handle = VK_NULL_HANDLE;
    VmaAllocation                               allocation = VK_NULL_HANDLE;

    VkImageTiling                               tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags                           usage_flags = 0;

    mutable ResourceState                       resource_state = ResourceState::UNDEFINED;
    HashMap<ImageSubResource, ResourceState>    sub_resources;

    // true if we created the VkImage, false otherwise (e.g retrieved from swapchain)
    bool                                        is_handle_owned = true;

    RendererResult ConvertTo32BPP(
        Device<Platform::VULKAN> *device,
        const TextureData *in_texture_data,
        VkImageType image_type,
        VkImageCreateFlags image_create_flags,
        VkImageFormatProperties *out_image_format_properties,
        VkFormat *out_format,
        UniquePtr<TextureData> &out_texture_data
    );

    RendererResult Create(
        Device<Platform::VULKAN> *device,
        const TextureData *in_texture_data,
        VkImageLayout initial_layout,
        VkImageCreateInfo *out_image_info,
        UniquePtr<TextureData> &out_texture_data
    );

    RendererResult Destroy(
        Device<Platform::VULKAN> *device
    );

    ResourceState GetResourceState() const
        { return resource_state; }

    void SetResourceState(ResourceState new_state);

    ResourceState GetSubResourceState(const ImageSubResource &sub_resource) const;
    void SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state);

    void InsertBarrier(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        ResourceState new_state,
        ImageSubResourceFlagBits flags = ImageSubResourceFlags::IMAGE_SUB_RESOURCE_FLAGS_COLOR
    );

    void InsertBarrier(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );

    void InsertSubResourceBarrier(
        CommandBuffer<Platform::VULKAN> *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif
