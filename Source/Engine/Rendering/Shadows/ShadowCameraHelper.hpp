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
    const BoundingSphere& sceneWorldBounds,
    const Vec3f& lightDir);

BoundingBox CalculateCascadeBounds(
    const Frustum& mainCameraFrustum,
    const Mat4f& shadowViewMatrix,
    const Vec2u& shadowMapResolution,
    const float inNearRatio,
    const float inFarRatio,
    const Vec3f& lightDir);

///\p previousBounds while the new fit still lies inside them, so small frame-to-frame changes in the camera
///(dimensions, clip planes) don't rescale and resample the whole cascade. Both must be in the same light-space basis
BoundingBox StabilizeCascadeBounds(
    const BoundingBox& newBounds,
    const BoundingBox& previousBounds);

BoundingBox CalculateCascadeCullingBounds(
    const BoundingBox& cascadeBounds,
    const BoundingSphere& sceneWorldBounds,
    const Mat4f& shadowViewMatrix);

} // namespace ShadowCameraHelpers

} // namespace Hyperion
