/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Math/Vector3.hpp>

namespace Hyperion {

namespace LightHelpers {

/// sunlight left after crossing the atmosphere toward \p directionToSun, relative to the sun at zenith. Black below the horizon
Vec3f ComputeSunAtmosphereTint(const Vec3f& directionToSun);

} // namespace LightHelpers

} // namespace Hyperion
