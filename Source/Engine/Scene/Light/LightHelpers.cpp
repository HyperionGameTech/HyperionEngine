/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Light/LightHelpers.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

namespace LightHelpers {

// matches Shaders/Include/Atmosphere.hlsli
static constexpr double PlanetRadius = 6371e3;
static constexpr double AtmosphereRadius = 6471e3;
static constexpr double RayleighScatterCoefficients[3] = { 5.5e-6, 13.0e-6, 22.4e-6 };
static constexpr double RayleighScatterHeight = 8e3;
static constexpr double MieScatterCoefficient = 21e-6;
static constexpr double MieScatterHeight = 1.2e3;

static bool IntersectSphere(double originHeight, double directionY, double sphereRadius, double& outNear, double& outFar)
{
    const double halfB = originHeight * directionY;
    const double c = originHeight * originHeight - sphereRadius * sphereRadius;
    const double discriminant = halfB * halfB - c;

    if (discriminant < 0.0)
    {
        return false;
    }

    const double root = MathUtil::Sqrt(discriminant);

    outNear = -halfB - root;
    outFar = -halfB + root;

    return true;
}

Vec3f ComputeSunAtmosphereTint(const Vec3f& directionToSun)
{
    const double originHeight = PlanetRadius + 1.0;
    const double directionY = double(directionToSun.Normalized().y);

    double nearDistance = 0.0;
    double farDistance = 0.0;

    if (IntersectSphere(originHeight, directionY, PlanetRadius, nearDistance, farDistance) && nearDistance > 0.0)
    {
        return Vec3f::Zero();
    }

    const double pathLength = IntersectSphere(originHeight, directionY, AtmosphereRadius, nearDistance, farDistance)
        ? MathUtil::Max(farDistance, 0.0)
        : 0.0;

    static constexpr uint32 NumSteps = 8;

    const double stepLength = pathLength / double(NumSteps);
    const double directionXZLength = MathUtil::Sqrt(MathUtil::Max(1.0 - directionY * directionY, 0.0));

    double rayleighDepth = 0.0;
    double mieDepth = 0.0;

    for (uint32 step = 0; step < NumSteps; step++)
    {
        const double distance = (double(step) + 0.5) * stepLength;
        const double sampleY = originHeight + directionY * distance;
        const double sampleXZ = directionXZLength * distance;
        const double sampleAltitude = MathUtil::Sqrt(sampleY * sampleY + sampleXZ * sampleXZ) - PlanetRadius;

        rayleighDepth += MathUtil::Exp(-sampleAltitude / RayleighScatterHeight) * stepLength;
        mieDepth += MathUtil::Exp(-sampleAltitude / MieScatterHeight) * stepLength;
    }

    const auto channelTint = [&](uint32 channel) -> float
    {
        const double opticalDepth = MieScatterCoefficient * mieDepth + RayleighScatterCoefficients[channel] * rayleighDepth;
        const double zenithOpticalDepth = MieScatterCoefficient * MieScatterHeight + RayleighScatterCoefficients[channel] * RayleighScatterHeight;

        return float(MathUtil::Exp(zenithOpticalDepth - opticalDepth));
    };

    return Vec3f(channelTint(0), channelTint(1), channelTint(2));
}

} // namespace LightHelpers

} // namespace Hyperion
