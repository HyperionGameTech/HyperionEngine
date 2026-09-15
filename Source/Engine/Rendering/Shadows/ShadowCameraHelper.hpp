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

float CalculateCascadeSplitRatio(
    uint32 splitIndex,
    uint32 numCascades,
    float nearDistance,
    float farDistance,
    float lambda);

BoundingBox CalculateCascadeBounds(
    const Frustum& mainCameraFrustum,
    const Mat4f& shadowViewMatrix,
    const Vec2u& shadowMapResolution,
    const float inNearRatio,
    const float inFarRatio,
    const Vec3f& lightDir);

BoundingBox StabilizeCascadeBounds(
    const BoundingBox& newBounds,
    const BoundingBox& previousBounds);

BoundingBox CalculateCascadeCullingBounds(
    const BoundingBox& cascadeBounds,
    const BoundingSphere& sceneWorldBounds,
    const Mat4f& shadowViewMatrix);

} // namespace ShadowCameraHelpers

} // namespace Hyperion
