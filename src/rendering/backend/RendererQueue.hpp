/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_QUEUE_H
#define HYPERION_V2_BACKEND_RENDERER_QUEUE_H

#include <rendering/backend/Platform.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion::renderer {

enum class DeviceQueueType : uint
{
    GRAPHICS,
    COMPUTE,
    TRANSFER,
    PRESENT,
    MAX
};

namespace platform {

template <PlatformType PLATFORM>
struct DeviceQueue
{
};

} // namespace platform
} // namespace hyperion::renderer
#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererQueue.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion::renderer {

using DeviceQueue = platform::DeviceQueue<Platform::CURRENT>;

} // namespace hyperion::renderer

#endif