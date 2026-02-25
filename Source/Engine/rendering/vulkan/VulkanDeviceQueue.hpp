/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/FixedArray.hpp>
#include <Core/Types.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

HYP_ENUM()
enum class VulkanDeviceQueueType : uint8
{
    INVALID = 0,

    GRAPHICS,
    COMPUTE,
    TRANSFER,
    PRESENT,

    MAX
};

struct VulkanDeviceQueue
{
    VulkanDeviceQueueType type = VulkanDeviceQueueType::INVALID;
    VkQueue queue = VK_NULL_HANDLE;
    uint32 familyIndex = 0;
    FixedArray<VkCommandPool, 8> commandPools {};
};

} // namespace Hyperion
