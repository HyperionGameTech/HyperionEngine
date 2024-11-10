/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_RENDER_BUCKET_HPP
#define HYPERION_RENDER_BUCKET_HPP

#include <core/Defines.hpp>
#include <Types.hpp>

namespace hyperion {

enum Bucket : uint32
{
    BUCKET_INVALID = uint32(-1),
    BUCKET_SWAPCHAIN = 0, /* Main swapchain */
    BUCKET_RESERVED0,     /* Reserved, unused */
    BUCKET_RESERVED1,
    /* === Scene objects === */
    BUCKET_OPAQUE,        /* Opaque items */
    BUCKET_TRANSLUCENT,   /* Transparent - rendering on top of opaque objects */
    BUCKET_SKYBOX,        /* Rendered without depth testing/writing, and rendered first */
    BUCKET_UI,
    BUCKET_MAX
};

HYP_FORCE_INLINE static bool BucketRayTestsEnabled(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT;
}

HYP_FORCE_INLINE static bool BucketRendersShadows(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT;
}

HYP_FORCE_INLINE static bool BucketHasGlobalIllumination(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT;
}

HYP_FORCE_INLINE static bool BucketFrustumCullingEnabled(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT;
}

HYP_FORCE_INLINE static bool BucketIsRenderable(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT
        || bucket == BUCKET_SKYBOX;
}

} // namespace hyperion

#endif