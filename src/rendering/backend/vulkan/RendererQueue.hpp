/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_QUEUE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_QUEUE_HPP

#include <core/lib/FixedArray.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <>
struct DeviceQueue<Platform::VULKAN>
{
    DeviceQueueType                 type;
    VkQueue                         queue;
    FixedArray<VkCommandPool, 8>    command_pools;
};

} // namespace platform

} // namespace renderer
} // namespace hyperion

#endif
