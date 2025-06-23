/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_PLATFORM_HPP
#define HYPERION_BACKEND_RENDERER_PLATFORM_HPP

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
using PlatformType = int;

class Platform
{
public:
    static constexpr PlatformType unknown = 0;
    static constexpr PlatformType vulkan = 1;
    static constexpr PlatformType webgpu = 2;

#if HYP_VULKAN
    static constexpr PlatformType current = vulkan;
#elif HYP_WEBGPU
    static constexpr PlatformType current = webgpu;
#else
    static constexpr PlatformType current = unknown;
#endif
};

} // namespace hyperion

#endif