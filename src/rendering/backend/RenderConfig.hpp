
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_CONFIG_HPP
#define HYPERION_RENDER_CONFIG_HPP

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

class HYP_API RenderConfig
{
public:
    // This means that each object with a different material will have a different draw call.
    // we can use this to have a smaller number of draw calls, but it means we need to access materials in shaders
    // via a large array. This is not supported on all platforms, and on non-bindless supported platforms, we need to use
    // a descriptor set per material (for textures).
    static bool ShouldCollectUniqueDrawCallPerMaterial();

    // are bindless resources supported by the current platform?
    static bool IsBindlessSupported();
    
    // is raytracing supported by the current platform?
    static bool IsRaytracingSupported();
};

} // namespace renderer
} // namespace hyperion

#endif