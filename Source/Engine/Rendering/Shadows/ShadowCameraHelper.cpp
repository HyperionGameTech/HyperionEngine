#include <RenderingPch.hpp>

#include <Rendering/Shadows/ShadowCameraHelper.hpp>

#include <Scene/Camera/Camera.hpp>

namespace Hyperion {
namespace ShadowCameraHelpers {

static constexpr float ZPullback = 1000.0f;

// cascades are fit slightly larger than the camera slice, so small frame-to-frame changes in the camera still fit inside
// the previous frame's bounds (see StabilizeCascadeBounds)
static constexpr float CascadeRadiusPadding = 1.02f;

// previous bounds are only kept while they're at most this much larger than a fresh fit
static constexpr float MaxReusedCascadeOversize = 1.05f;

Mat4f CalculateShadowViewMatrix(
    const BoundingSphere& sceneWorldBounds,
    const Vec3f& lightDir)
{
    // Anchor the light-space frame to the scene center, not the camera, so it stays fixed in
    // world space as the camera moves. This is what lets texel snapping actually stop the swim.
    const Vec3f center = sceneWorldBounds.GetCenter();

    return Mat4f::LookAt(center, center - lightDir * ZPullback, Vec3f::UnitY());
}

BoundingBox CalculateCascadeBounds(
    const Frustum& mainCameraFrustum,
    const Mat4f& shadowViewMatrix,
    const Vec2u& shadowMapResolution,
    const float inNearRatio,
    const float inFarRatio,
    const Vec3f& lightDir)
{
    Frustum cascadeFrustum = mainCameraFrustum.SubFrustum(inNearRatio, inFarRatio);

    const FixedArray<Vec3f, 8>& frustumCorners = cascadeFrustum.GetCorners();

    Vec3f frustumCenter(0.0f);

    for (uint8 i = 0; i < 8; ++i)
    {
        frustumCenter += frustumCorners[i];
    }

    frustumCenter /= 8.0f;

    float sphereRadius = 0.0f;

    for (uint8 i = 0; i < 8; ++i)
    {
        float dist = (frustumCorners[i] - frustumCenter).Length();

        sphereRadius = MathUtil::Max(sphereRadius, dist);
    }

    sphereRadius *= CascadeRadiusPadding;

    Vec4f centerLS = shadowViewMatrix.TransformVector(Vec4f(frustumCenter, 1.0f));
    centerLS /= centerLS.w;

    // Snap the cascade center to whole shadow-map texels so the shadows don't shimmer as the
    // camera moves. Only stable because shadowViewMatrix is anchored to the scene, not the camera.
    const float worldUnitsPerTexel = (sphereRadius * 2.0f) / static_cast<float>(shadowMapResolution.Max());

    centerLS.x = MathUtil::Floor(centerLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    centerLS.y = MathUtil::Floor(centerLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;
    centerLS.z = MathUtil::Floor(centerLS.z / worldUnitsPerTexel) * worldUnitsPerTexel;

    BoundingBox finalBounds;
    finalBounds.min.x = centerLS.x - sphereRadius;
    finalBounds.max.x = centerLS.x + sphereRadius;
    finalBounds.min.y = centerLS.y - sphereRadius;
    finalBounds.max.y = centerLS.y + sphereRadius;
    finalBounds.min.z = centerLS.z - sphereRadius;
    finalBounds.max.z = centerLS.z + sphereRadius;

    return finalBounds;
}

BoundingBox StabilizeCascadeBounds(
    const BoundingBox& newBounds,
    const BoundingBox& previousBounds)
{
    if (!previousBounds.IsValid() || !previousBounds.IsFinite())
    {
        return newBounds;
    }

    const Vec3f newCenter = newBounds.GetCenter();
    const Vec3f previousCenter = previousBounds.GetCenter();

    const float newHalfExtent = newBounds.GetExtent().x * 0.5f;
    const float previousHalfExtent = previousBounds.GetExtent().x * 0.5f;

    if (previousHalfExtent > newHalfExtent * MaxReusedCascadeOversize)
    {
        return newBounds;
    }

    // the unpadded fit has to lie inside the previous bounds, otherwise the padding would be what's being kept
    const float requiredHalfExtent = newHalfExtent / CascadeRadiusPadding;

    const Vec3f centerOffset = newCenter - previousCenter;

    const bool fitsInPrevious = MathUtil::Abs(centerOffset.x) + requiredHalfExtent <= previousHalfExtent
        && MathUtil::Abs(centerOffset.y) + requiredHalfExtent <= previousHalfExtent
        && MathUtil::Abs(centerOffset.z) + requiredHalfExtent <= previousHalfExtent;

    return fitsInPrevious ? previousBounds : newBounds;
}

BoundingBox CalculateCascadeCullingBounds(
    const BoundingBox& cascadeBounds,
    const BoundingSphere& sceneWorldBounds,
    const Mat4f& shadowViewMatrix)
{
    Vec4f sceneCenterLS = shadowViewMatrix.TransformVector(Vec4f(sceneWorldBounds.GetCenter(), 1.0f));
    sceneCenterLS /= sceneCenterLS.w;

    // light-space -Z points toward the light
    const float sceneMinZ = sceneCenterLS.z - sceneWorldBounds.GetRadius();

    BoundingBox cullingBounds = cascadeBounds;
    cullingBounds.min.z = MathUtil::Min(cascadeBounds.min.z, sceneMinZ);

    return cullingBounds;
}

} // namespace ShadowCameraHelpers
} // namespace Hyperion
