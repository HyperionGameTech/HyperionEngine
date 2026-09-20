/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Transform.hpp>
#include <Core/Containers/SortedArray.hpp>

namespace Hyperion {

class Camera;

namespace CameraUtils {

static inline float ClampPitchDelta(const Camera& camera, float deltaRadians, float maxPitchSine)
{
    const float maxPitch = MathUtil::Arcsin(maxPitchSine);
    const float currentPitch = MathUtil::Arcsin(MathUtil::Clamp(camera.GetDirection().y, -1.0f, 1.0f));
    // Rotating by +deltaRadians about the side vector *decreases* pitch, so the target
    // pitch moves the opposite way of deltaRadians.
    const float clampedPitch = MathUtil::Clamp(currentPitch - deltaRadians, -maxPitch, maxPitch);

    return currentPitch - clampedPitch;
}

} // namespace CameraUtils

} // namespace Hyperion