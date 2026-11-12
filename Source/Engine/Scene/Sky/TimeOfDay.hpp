/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

struct TimeOfDaySettings;

/// x = sunrise (hours), y = sunset (hours), z = how far the arc leans from overhead (radians), w = its heading about +y (radians)
ENGINE_API Vec4f GetSunArc(const TimeOfDaySettings& settings);

/// toward the sun at `hour`: up from +x at sunrise, highest halfway to sunset, down at -x, and round below the horizon at night
ENGINE_API Vec3f GetSunArcDirection(const Vec4f& sunArc, float hour);

/// the hour along the arc nearest `toSun`, and the sine of how far off the arc it is
ENGINE_API float GetSunArcHour(const Vec4f& sunArc, const Vec3f& toSun, float& outOffArc);

} // namespace Hyperion
