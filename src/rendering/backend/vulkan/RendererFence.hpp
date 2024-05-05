/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_FENCE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_FENCE_HPP

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
struct FencePlatformImpl<Platform::VULKAN>
{
    Fence<Platform::VULKAN> *self = nullptr;
    VkFence                 handle = VK_NULL_HANDLE;
    VkResult                last_frame_result = VK_SUCCESS;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif
