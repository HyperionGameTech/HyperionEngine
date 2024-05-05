/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_IMAGE_VIEW_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_IMAGE_VIEW_HPP

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device;

template <>
struct ImageViewPlatformImpl<Platform::VULKAN>
{
    ImageView<Platform::VULKAN> *self = nullptr;
    VkImageView                 handle = VK_NULL_HANDLE;

    Result Create(
        Device<Platform::VULKAN> *device,
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspect_flags,
        VkImageViewType view_type,
        uint mipmap_layer,
        uint num_mipmaps,
        uint face_layer,
        uint num_faces
    );

    Result Destroy(Device<Platform::VULKAN> *device);
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif