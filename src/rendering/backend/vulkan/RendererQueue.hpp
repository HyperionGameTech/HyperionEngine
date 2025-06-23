/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_QUEUE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_QUEUE_HPP

#include <core/containers/FixedArray.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
enum class VulkanDeviceQueueType : uint8
{
    GRAPHICS,
    COMPUTE,
    TRANSFER,
    PRESENT
};

struct VulkanDeviceQueue
{
    VulkanDeviceQueueType type;
    VkQueue queue;
    FixedArray<VkCommandPool, 8> command_pools;
};

} // namespace hyperion

#endif
