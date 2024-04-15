/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_FENCE_H
#define HYPERION_V2_BACKEND_RENDERER_FENCE_H

#include <rendering/backend/Platform.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Fence
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion


#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFence.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Fence = platform::Fence<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif