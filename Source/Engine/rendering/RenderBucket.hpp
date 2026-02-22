/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

HYP_ENUM()
enum RenderBucket : uint8
{
    RB_NONE = 0,
    RB_OPAQUE,      /* Opaque objects, default for all objects */
    RB_LIGHTMAP,    /* Lightmapped objects - objects that should bypass typical shading calculations due to having precomputed lighting */
    RB_TRANSLUCENT, /* Transparent - rendering on top of opaque objects with forward rendering */
    RB_SKYBOX,      /* Rendered without depth writing - rendered last to avoid overdraw */
    RB_DEBUG,       /* Debug rendering - rendered on top of everything else  (see DebugDrawer) */
    RB_MAX
};

} // namespace Hyperion
