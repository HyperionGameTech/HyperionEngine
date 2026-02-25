/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

namespace Hyperion {

class IRenderConfig
{
protected:
    IRenderConfig() = default;

public:
    // Whether or not the backend supports bindless textures.
    bool bindlessTextures : 1 = false;

    // RayTracing support / enabled.
    bool rayTracing : 1 = false;

    // Indirect rendering allows us to issue many draw calls with a single API call, as well as perform occlusion culling on the GPU.
    bool indirectRendering : 1 = false;

    // Whether or not parallel rendering is enabled. This allows multiple threads to record rendering commands simultaneously.
    bool parallelRendering : 1 = false;

    // Whether or not dynamic descriptor indexing is enabled / supported.
    bool dynamicDescriptorIndexing : 1 = false;
};

} // namespace Hyperion
