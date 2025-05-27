/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_INSTANCE_HPP
#define HYPERION_BACKEND_RENDERER_INSTANCE_HPP

#include <rendering/backend/Platform.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Instance
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
    #include <rendering/backend/vulkan/RendererInstance.hpp>
#else
    #error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Instance = platform::Instance<Platform::current>;

} // namespace renderer
} // namespace hyperion

#endif