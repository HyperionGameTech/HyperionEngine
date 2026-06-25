/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {

struct Frustum;
struct BoundingSphere;

namespace ShadowCameraHelpers {

Mat4f CalculateShadowViewMatrix(
    const Frustum& mainCameraFrustum,
    const Vec3f& lightDir);

BoundingBox CalculateCascadeBounds(
    const Frustum& mainCameraFrustum,
    const BoundingSphere& sceneWorldBounds,
    const Mat4f& shadowViewMatrix,
    const Vec2u& shadowMapResolution,
    const float inNearRatio,
    const float inFarRatio,
    const Vec3f& lightDir);

} // namespace ShadowCameraHelpers

} // namespace Hyperion
