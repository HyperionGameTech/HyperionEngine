#ifndef HYPERION_V2_RENDER_BUCKET_H
#define HYPERION_V2_RENDER_BUCKET_H

#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

enum Bucket : uint32
{
    BUCKET_INVALID = uint32(-1),
    BUCKET_SWAPCHAIN = 0, /* Main swapchain */
    BUCKET_RESERVED1,     /* Reserved, unused */
    BUCKET_SHADOW,
    /* === Scene objects === */
    BUCKET_OPAQUE,        /* Opaque items */
    BUCKET_TRANSLUCENT,   /* Transparent - rendering on top of opaque objects */
    BUCKET_SKYBOX,        /* Rendered without depth testing/writing, and rendered first */
    BUCKET_UI,
    BUCKET_MAX
};

HYP_FORCE_INLINE
static bool BucketRayTestsEnabled(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT;
}

HYP_FORCE_INLINE
static bool BucketRendersShadows(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT;
}

HYP_FORCE_INLINE
static bool BucketHasGlobalIllumination(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT;
}

HYP_FORCE_INLINE
static bool BucketFrustumCullingEnabled(Bucket bucket)
{
    return bucket == BUCKET_SHADOW
        || bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT;
}

HYP_FORCE_INLINE
static bool BucketIsRenderable(Bucket bucket)
{
    return bucket == BUCKET_OPAQUE
        || bucket == BUCKET_TRANSLUCENT
        || bucket == BUCKET_SKYBOX;
}

} // namespace hyperion::v2

#endif