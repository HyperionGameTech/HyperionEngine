#include <RenderingPch.hpp>

#include <Rendering/Shadows/ShadowCameraHelper.hpp>

#include <Scene/Camera/Camera.hpp>

namespace Hyperion {
namespace ShadowCameraHelpers {

static constexpr float ZPullback = 1000.0f;

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
    const BoundingSphere& sceneWorldBounds,
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

    Vec4f centerLS = shadowViewMatrix.TransformVector(Vec4f(frustumCenter, 1.0f));
    centerLS /= centerLS.w;

    // Snap the cascade center to whole shadow-map texels so the shadows don't shimmer as the
    // camera moves. Only stable because shadowViewMatrix is anchored to the scene, not the camera.
    const float worldUnitsPerTexel = (sphereRadius * 2.0f) / static_cast<float>(shadowMapResolution.Max());

    centerLS.x = MathUtil::Floor(centerLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    centerLS.y = MathUtil::Floor(centerLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;

    Vec4f sceneCenterLS = shadowViewMatrix.TransformVector(Vec4f(sceneWorldBounds.GetCenter(), 1.0f));
    sceneCenterLS /= sceneCenterLS.w;

    BoundingBox finalBounds;
    finalBounds.min.x = centerLS.x - sphereRadius;
    finalBounds.max.x = centerLS.x + sphereRadius;
    finalBounds.min.y = centerLS.y - sphereRadius;
    finalBounds.max.y = centerLS.y + sphereRadius;

    float sceneMinZ = sceneCenterLS.z - sceneWorldBounds.GetRadius();

    finalBounds.min.z = MathUtil::Min(centerLS.z - sphereRadius, sceneMinZ);
    finalBounds.max.z = centerLS.z + sphereRadius;

    return finalBounds;
}

} // namespace ShadowCameraHelpers
} // namespace Hyperion
