/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Sky/TimeOfDay.hpp>
#include <Scene/EnvironmentSettings.hpp>

#include <Core/Math/MathUtil.hpp>

#include <cmath>

namespace Hyperion {

Vec4f GetSunArc(const TimeOfDaySettings& settings)
{
    const float sunrise = MathUtil::Clamp(settings.sunrise, 0.0f, 23.0f);
    const float sunset = MathUtil::Clamp(settings.sunset, sunrise + 1.0f, 24.0f);

    return Vec4f(
        sunrise,
        sunset,
        MathUtil::DegToRad(MathUtil::Clamp(settings.tiltDegrees, 0.0f, 89.0f)),
        MathUtil::DegToRad(settings.headingDegrees));
}

Vec3f GetSunArcDirection(const Vec4f& sunArc, float hour)
{
    const float day = sunArc.y - sunArc.x;
    const float since = MathUtil::Mod(MathUtil::Mod(hour - sunArc.x, 24.0f) + 24.0f, 24.0f);

    float angle;

    if (since <= day)
    {
        angle = (since / day - 0.5f) * MathUtil::pi<float>;
    }
    else
    {
        angle = ((since - day) / (24.0f - day) + 0.5f) * MathUtil::pi<float>;
    }

    const Vec3f onArc = Vec3f(-MathUtil::Sin(angle), MathUtil::Cos(angle) * MathUtil::Cos(sunArc.z), MathUtil::Cos(angle) * MathUtil::Sin(sunArc.z));

    const float headingCos = MathUtil::Cos(sunArc.w);
    const float headingSin = MathUtil::Sin(sunArc.w);

    return Vec3f(onArc.x * headingCos + onArc.z * headingSin, onArc.y, onArc.z * headingCos - onArc.x * headingSin);
}

float GetSunArcHour(const Vec4f& sunArc, const Vec3f& toSun, float& outOffArc)
{
    const float headingCos = MathUtil::Cos(sunArc.w);
    const float headingSin = MathUtil::Sin(sunArc.w);

    const Vec3f direction = toSun.Normalized();
    const Vec3f onArc = Vec3f(direction.x * headingCos - direction.z * headingSin, direction.y, direction.z * headingCos + direction.x * headingSin);

    const float tiltCos = MathUtil::Cos(sunArc.z);
    const float tiltSin = MathUtil::Sin(sunArc.z);

    outOffArc = MathUtil::Abs(onArc.z * tiltCos - onArc.y * tiltSin);

    const float angle = std::atan2(-onArc.x, onArc.y * tiltCos + onArc.z * tiltSin);
    const float day = sunArc.y - sunArc.x;

    if (MathUtil::Abs(angle) <= 0.5f * MathUtil::pi<float>)
    {
        return sunArc.x + (angle / MathUtil::pi<float> + 0.5f) * day;
    }

    const float night = angle > 0.0f ? angle : angle + 2.0f * MathUtil::pi<float>;

    return MathUtil::Mod(sunArc.y + (night / MathUtil::pi<float> - 0.5f) * (24.0f - day), 24.0f);
}

} // namespace Hyperion
