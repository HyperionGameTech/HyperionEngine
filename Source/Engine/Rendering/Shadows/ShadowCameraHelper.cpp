#include <RenderingPch.hpp>

#include <Rendering/Shadows/ShadowCameraHelper.hpp>

#include <Scene/Camera/Camera.hpp>

namespace Hyperion {
namespace ShadowCameraHelpers {

static constexpr float ZPullback = 1000.0f;
static constexpr float ZPadding = 50.0f;

Mat4f CalculateShadowViewMatrix(
    const Frustum& mainCameraFrustum,
    const Vec3f& lightDir)
{
    const FixedArray<Vec3f, 8>& corners = mainCameraFrustum.GetCorners();

    Vec3f frustumCenter = Vec3f::Zero();

    for (uint32 i = 0; i < 8; ++i)
    {
        frustumCenter += corners[i];
    }

    frustumCenter *= (1.0f / 8.0f);

    return Mat4f::LookAt(frustumCenter, frustumCenter - lightDir * ZPullback, Vec3f::UnitY());
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

    float worldUnitsPerTexel = (sphereRadius * 2.0f) / static_cast<float>(shadowMapResolution.Max());

    //centerLS.x = MathUtil::Floor(centerLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
    //centerLS.y = MathUtil::Floor(centerLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;

    BoundingBox finalBounds;
    finalBounds.min.x = centerLS.x - sphereRadius;
    finalBounds.max.x = centerLS.x + sphereRadius;
    finalBounds.min.y = centerLS.y - sphereRadius;
    finalBounds.max.y = centerLS.y + sphereRadius;

    Vec4f sceneCenterLS = shadowViewMatrix.TransformVector(Vec4f(sceneWorldBounds.GetCenter(), 1.0f));
    
    float sceneMinZ = sceneCenterLS.z - sceneWorldBounds.GetRadius();

    finalBounds.min.z = MathUtil::Min(centerLS.z - sphereRadius, sceneMinZ);
    finalBounds.max.z = centerLS.z + sphereRadius;

    return finalBounds;
}

} // namespace ShadowCameraHelpers
} // namespace Hyperion
