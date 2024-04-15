/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_PLATFORM_HPP
#define HYPERION_V2_BACKEND_RENDERER_PLATFORM_HPP

#include <Types.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

using PlatformType = int;

class Platform
{
public:
    static constexpr PlatformType UNKNOWN = 0;
    static constexpr PlatformType VULKAN = 1;
    static constexpr PlatformType WEBGPU = 2;

#if HYP_VULKAN
    static constexpr PlatformType CURRENT = VULKAN;
#elif HYP_WEBGPU
    static constexpr PlatformType CURRENT = WEBGPU;
#else
    static constexpr PlatformType CURRENT = UNKNOWN;
#endif
};
} // namespace renderer
} // namespace hyperion

#endif