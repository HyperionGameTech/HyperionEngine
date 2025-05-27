
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_CONFIG_HPP
#define HYPERION_RENDER_CONFIG_HPP

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

class IRenderConfig
{
public:
    virtual ~IRenderConfig() = default;

    // This means that each object with a different material will have a different draw call.
    // we can use this to have a smaller number of draw calls, but it means we need to access materials in shaders
    // via a large array. This is not supported on all platforms, and on non-bindless supported platforms, we need to use
    // a descriptor set per material (for textures).
    virtual bool ShouldCollectUniqueDrawCallPerMaterial() const = 0;

    // are bindless resources supported by the current platform?
    virtual bool IsBindlessSupported() const = 0;

    // is raytracing supported by the current platform?
    virtual bool IsRaytracingSupported() const = 0;

    // use indirect rendering for draw calls - allows occlusion culling to be done on the GPU
    virtual bool IsIndirectRenderingEnabled() const = 0;

    // use parallel rendering for draw calls
    virtual bool IsParallelRenderingEnabled() const = 0;

    virtual bool IsDynamicDescriptorIndexingSupported() const = 0;
};

} // namespace renderer
} // namespace hyperion

#endif