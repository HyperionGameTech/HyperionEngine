/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FBO_HPP

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
struct FramebufferPlatformImpl<Platform::VULKAN>
{
    Framebuffer<Platform::VULKAN>                   *self = nullptr;
    FixedArray<VkFramebuffer, max_frames_in_flight> handles { VK_NULL_HANDLE };
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif // RENDERER_FBO_HPP
