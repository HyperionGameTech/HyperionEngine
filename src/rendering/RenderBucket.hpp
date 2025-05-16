/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_RENDER_BUCKET_HPP
#define HYPERION_RENDER_BUCKET_HPP

#include <core/Defines.hpp>
#include <Types.hpp>

namespace hyperion {

enum Bucket : uint32
{
    BUCKET_NONE         = 0,
    BUCKET_OPAQUE,      /* Opaque objects, default for all objects */
    BUCKET_LIGHTMAP,    /* Lightmapped objects - objects that should bypass deferred shading pass. */
    BUCKET_TRANSLUCENT, /* Transparent - rendering on top of opaque objects */
    BUCKET_SKYBOX,      /* Rendered without depth testing/writing, and rendered first */
    BUCKET_DEBUG,       /* Objects that are rendered in the translucent pass, but should otherwise not be considered by shadows, env probes, etc. */
    BUCKET_MAX
};

} // namespace hyperion

#endif