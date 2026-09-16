/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/EnvironmentSettings.hpp>

#include <Rendering/RenderProxy.hpp>

#include <Core/Math/MathUtil.hpp>

#include <EnvironmentSettings.generated.inl>

namespace Hyperion {

using Mat3x3f = float[3][3];

static constexpr Mat3x3f LinearToLmsMatrix = {
    { 3.90405e-1f, 5.49941e-1f, 8.92632e-3f },
    { 7.08416e-2f, 9.63172e-1f, 1.35775e-3f },
    { 2.31082e-2f, 1.28021e-1f, 9.36245e-1f }
};

static constexpr Mat3x3f LmsToLinearMatrix = {
    { 2.85847e+0f, -1.62879e+0f, -2.48910e-2f },
    { -2.10182e-1f, 1.15820e+0f, 3.24281e-4f },
    { -4.18120e-2f, -1.18169e-1f, 1.06867e+0f }
};

static void BuildWhiteBalanceMatrix(float temperature, float tint, Mat3x3f& outMatrix)
{
    const float temperatureOffset = temperature * (100.0f / 60.0f);
    const float tintOffset = tint * (100.0f / 60.0f);

    const float whiteX = 0.31271f - temperatureOffset * (temperatureOffset < 0.0f ? 0.1f : 0.05f);
    const float standardIlluminantY = 2.87f * whiteX - 3.0f * whiteX * whiteX - 0.27509507f;
    const float whiteY = standardIlluminantY + tintOffset * 0.05f;

    const float cieX = whiteX / whiteY;
    const float cieZ = (1.0f - whiteX - whiteY) / whiteY;

    const float whiteL = 0.7328f * cieX + 0.4296f - 0.1624f * cieZ;
    const float whiteM = -0.7036f * cieX + 1.6975f + 0.0061f * cieZ;
    const float whiteS = 0.0030f * cieX + 0.0136f + 0.9834f * cieZ;

    const float balance[3] = {
        0.949237f / whiteL,
        1.03542f / whiteM,
        1.08728f / whiteS
    };

    for (uint32 row = 0; row < 3; row++)
    {
        for (uint32 column = 0; column < 3; column++)
        {
            float sum = 0.0f;

            for (uint32 lmsIndex = 0; lmsIndex < 3; lmsIndex++)
            {
                sum += LmsToLinearMatrix[row][lmsIndex] * balance[lmsIndex] * LinearToLmsMatrix[lmsIndex][column];
            }

            outMatrix[row][column] = sum;
        }
    }
}

void WriteEnvironmentShaderData(const EnvironmentSettings& settings, WorldShaderData& outShaderData)
{
    const ExposureSettings& exposure = settings.exposure;

    outShaderData.tonemapOperator = MathUtil::Min(uint32(exposure.tonemapOperator), uint32(TonemapOperator::Reinhard));

    outShaderData.exposureGrading = Vec4f(
        MathUtil::Pow(2.0f, MathUtil::Clamp(exposure.exposureCompensation, -16.0f, 16.0f)),
        MathUtil::Clamp(exposure.contrast, 0.0f, 4.0f),
        MathUtil::Clamp(exposure.saturation, 0.0f, 4.0f),
        0.0f);

    Mat3x3f whiteBalanceMatrix;
    BuildWhiteBalanceMatrix(
        MathUtil::Clamp(exposure.whiteBalanceTemperature, -1.0f, 1.0f),
        MathUtil::Clamp(exposure.whiteBalanceTint, -1.0f, 1.0f),
        whiteBalanceMatrix);

    for (uint32 row = 0; row < 3; row++)
    {
        outShaderData.whiteBalanceRows[row] = Vec4f(whiteBalanceMatrix[row][0], whiteBalanceMatrix[row][1], whiteBalanceMatrix[row][2], 0.0f);
    }

    const SkyLookSettings& sky = settings.sky;

    outShaderData.skyTintIntensity = Vec4f(
        MathUtil::Max(sky.tint, Vec3f::Zero()),
        MathUtil::Max(sky.intensity, 0.0f));

    outShaderData.skyLightParams.x = MathUtil::Max(settings.skyLight.diffuseIntensity, 0.0f);
    outShaderData.skyLightParams.y = MathUtil::Max(settings.skyLight.specularIntensity, 0.0f);
    outShaderData.skyLightParams.z = MathUtil::Max(sky.overcastBrightness, 0.0f);

    outShaderData.skyOcclusionParams = Vec4f(
        MathUtil::Clamp(settings.skyLight.occlusionStrength, 0.0f, 1.0f),
        MathUtil::Max(settings.skyLight.occlusionRadius, 0.0f),
        MathUtil::Max(settings.skyLight.occlusionBias, 0.0f),
        0.0f);

    if (sky.showSunDisk)
    {
        outShaderData.environmentFlags |= WEF_SUN_DISK;
    }

    const HeightFogSettings& heightFog = settings.heightFog;

    if (heightFog.enabled && heightFog.maxOpacity > 0.0f)
    {
        outShaderData.environmentFlags |= WEF_HEIGHT_FOG;
    }

    outShaderData.heightFogParams = Vec4f(
        MathUtil::Max(heightFog.density, 0.0f),
        MathUtil::Max(heightFog.heightFalloff, 0.0f),
        heightFog.baseHeight,
        MathUtil::Max(heightFog.startDistance, 0.0f));

    // 0 turns aerial perspective off; the shader divides by it
    outShaderData.atmosphereFogParams = Vec4f(
        heightFog.aerialPerspectiveDistance > 0.0f ? MathUtil::Max(heightFog.aerialPerspectiveDistance, 1.0f) : 0.0f,
        MathUtil::Max(heightFog.skyInscatterStrength, 0.0f),
        MathUtil::Max(heightFog.sunInscatterStrength, 0.0f),
        MathUtil::Clamp(heightFog.maxOpacity, 0.0f, 1.0f));

    outShaderData.fogPhaseParams = Vec4f(MathUtil::Clamp(heightFog.sunAnisotropy, 0.0f, 0.95f), 0.0f, 0.0f, 0.0f);
}

} // namespace Hyperion
