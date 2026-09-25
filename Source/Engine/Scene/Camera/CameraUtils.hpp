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
    
    const float yClamp = MathUtil::Clamp(camera.GetDirection().y, -1.0f, 1.0f);

    const float currentPitch = MathUtil::Arcsin(yClamp);
    const float clampedPitch = MathUtil::Clamp(currentPitch - deltaRadians, -maxPitch, maxPitch);

    return currentPitch - clampedPitch;
}

} // namespace CameraUtils

} // namespace Hyperion