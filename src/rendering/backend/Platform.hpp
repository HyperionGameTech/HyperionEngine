#ifndef HYPERION_V2_BACKEND_RENDERER_PLATFORM_HPP
#define HYPERION_V2_BACKEND_RENDERER_PLATFORM_HPP

#include <Types.hpp>
#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {

using PlatformType = int;

// Use class rather than enum or enum class
// to be able to implicitly convert values to PlatformType,
// but still prevent name collision issues
class Platform
{
public:
    static constexpr PlatformType UNKNOWN   = 0;
    static constexpr PlatformType VULKAN    = 1;
    static constexpr PlatformType WEBGPU    = 2;

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