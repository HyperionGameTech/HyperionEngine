/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_DEVICE_HPP
#define HYPERION_BACKEND_RENDERER_DEVICE_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace platform {

template <PlatformType PLATFORM>
class Device : public RenderObject<Device<PLATFORM>>
{
public:
    static constexpr PlatformType platform = PLATFORM;
};

} // namespace platform

} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererDevice.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
using Device = platform::Device<Platform::current>;

} // namespace hyperion

#endif