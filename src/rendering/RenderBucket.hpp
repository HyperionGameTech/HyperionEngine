/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

namespace hyperion {

HYP_ENUM()
enum RenderBucket : uint32
{
    RB_NONE = 0,
    RB_OPAQUE,      /* Opaque objects, default for all objects */
    RB_LIGHTMAP,    /* Lightmapped objects - objects that should bypass deferred shading pass. */
    RB_TRANSLUCENT, /* Transparent - rendering on top of opaque objects */
    RB_SKYBOX,      /* Rendered without depth testing/writing, and rendered first */
    RB_DEBUG,       /* Objects that are rendered in the translucent pass, but should otherwise not be considered by shadows, env probes, etc. */
    RB_MAX
};

} // namespace hyperion
