/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

HYP_ENUM()
enum class RenderBucket : uint8
{
    Opaque,         /* Opaque objects, default for all objects */
    Lightmapped,    /* Lightmapped objects - objects that should bypass typical shading calculations due to having precomputed lighting */
    Translucent,    /* Transparent - rendering on top of opaque objects with forward rendering */
    Sky,            /* Rendered without depth writing - rendered last to avoid overdraw */
    Debug           /* Debug rendering - rendered on top of everything else  (see DebugDrawer) */
};

static constexpr uint32 NumRenderBuckets = uint32(RenderBucket::Debug) + 1;

template <RenderBucket... Buckets>
static constexpr uint32 RenderBucketMask = ((1u << uint32(Buckets)) | ...);

static constexpr uint32 AllRenderBucketsMask = RenderBucketMask<
    RenderBucket::Opaque,
    RenderBucket::Lightmapped,
    RenderBucket::Sky,
    RenderBucket::Translucent,
    RenderBucket::Debug
>;

} // namespace Hyperion
