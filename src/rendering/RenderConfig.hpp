/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

namespace hyperion {

class IRenderConfig
{
protected:
    IRenderConfig() = default;

public:
    // This means that each object with a different material will have a different draw call.
    // we can use this to have a smaller number of draw calls, but it means we need to access materials in shaders
    // via a large array. This is not supported on all platforms, and on non-bindless supported platforms, we need to use
    // a descriptor set per material (for textures).
    bool uniqueDrawCallPerMaterial : 1 = false;

    // Whether or not the backend supports bindless textures.
    bool bindlessTextures : 1 = false;

    // Raytracing support / enabled.
    bool raytracing : 1 = false;

    // Indirect rendering allows us to issue many draw calls with a single API call, as well as perform occlusion culling on the GPU.
    bool indirectRendering : 1 = false;

    // Whether or not parallel rendering is enabled. This allows multiple threads to record rendering commands simultaneously.
    bool parallelRendering : 1 = false;

    // Whether or not dynamic descriptor indexing is enabled / supported.
    bool dynamicDescriptorIndexing : 1 = false;
};

} // namespace hyperion
