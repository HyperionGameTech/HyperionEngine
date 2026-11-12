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

ENGINE_API Vec4f GetSunArc(const TimeOfDaySettings& settings);
ENGINE_API Vec3f GetSunArcDirection(const Vec4f& sunArc, float hour);

ENGINE_API float GetSunArcHour(const Vec4f& sunArc, const Vec3f& toSun, float& outOffArc);

} // namespace Hyperion
