/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_DEVICE_H
#define HYPERION_V2_BACKEND_RENDERER_DEVICE_H

#include <rendering/backend/Platform.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Device
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererDevice.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Device = platform::Device<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif