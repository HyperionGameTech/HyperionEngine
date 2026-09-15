/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Sky/CloudEffectVolume.hpp>

#include <Rendering/RenderProxy.hpp>

#include <Core/Math/MathUtil.hpp>

#include <CloudEffectVolume.generated.inl>

namespace Hyperion {

static constexpr float MinNoiseScale = 1.0f;

double CloudEffectVolume::WrapToPeriod(double value, double period)
{
    if (period <= 0.0)
    {
        return 0.0;
    }

    const double wrapped = MathUtil::Mod(value, period);

    return wrapped < 0.0 ? wrapped + period : wrapped;
}

CloudEffectVolume::CloudEffectVolume()
    : EffectVolume(BoundingBox::Infinity()),
      m_evolutionTime(0.0),
      m_windOffsetX(0.0),
      m_windOffsetZ(0.0)
{
}

void CloudEffectVolume::SetSettings(const CloudSettings& settings)
{
    m_settings = settings;

    SetNeedsRenderProxyUpdate();
}

void CloudEffectVolume::AdvanceClock(float delta)
{
    const double windDirectionRadians = MathUtil::DegToRad(double(m_settings.wind.directionDegrees));
    const double windDistance = double(m_settings.wind.speed) * double(delta);

    m_windOffsetX += MathUtil::Cos(windDirectionRadians) * windDistance;
    m_windOffsetZ += MathUtil::Sin(windDirectionRadians) * windDistance;

    // weather keyframes only step forward in time
    const double evolutionSpeed = MathUtil::Max(double(m_settings.wind.evolutionSpeed), 0.0);

    m_evolutionTime = WrapToPeriod(m_evolutionTime + evolutionSpeed * double(delta), EvolutionTimePeriod);

    SetNeedsRenderProxyUpdate();
}

void CloudEffectVolume::UpdateRenderProxy(RenderProxyEffectVolume* proxy)
{
    EffectVolume::UpdateRenderProxy(proxy);

    const float weatherScale = MathUtil::Max(m_settings.noise.weatherScale, MinNoiseScale);
    const float shapeNoiseScale = MathUtil::Max(m_settings.noise.shapeScale, MinNoiseScale);
    const float detailNoiseScale = MathUtil::Max(m_settings.noise.detailScale, MinNoiseScale);

    const double windDirectionRadians = MathUtil::DegToRad(double(m_settings.wind.directionDegrees));
    const double weatherPeriod = double(weatherScale) * double(WeatherNoisePeriodCells);

    CloudVolumeShaderData cloudData {};

    cloudData.coverage = MathUtil::Clamp(m_settings.shape.coverage, 0.0f, 1.0f);
    cloudData.cloudTypeBias = MathUtil::Clamp(m_settings.shape.cloudTypeBias, 0.0f, 1.0f);
    cloudData.densityMultiplier = MathUtil::Max(m_settings.shape.densityMultiplier, 0.0f);
    cloudData.detailErosion = MathUtil::Clamp(m_settings.shape.detailErosion, 0.0f, 1.0f);

    cloudData.baseAltitude = m_settings.layer.baseAltitude;
    cloudData.layerThickness = MathUtil::Max(m_settings.layer.thickness, 1.0f);
    cloudData.hazeDistance = MathUtil::Max(m_settings.lighting.hazeDistance, 1.0f);
    cloudData.seed = m_settings.noise.seed;

    cloudData.weatherScale = weatherScale;
    cloudData.shapeNoiseScale = shapeNoiseScale;
    cloudData.detailNoiseScale = detailNoiseScale;
    cloudData.evolutionTime = float(m_evolutionTime);

    cloudData.windDirection = Vec2f(float(MathUtil::Cos(windDirectionRadians)), float(MathUtil::Sin(windDirectionRadians)));
    cloudData.shadowStrength = MathUtil::Clamp(m_settings.lighting.shadowStrength, 0.0f, 1.0f);
    cloudData.shadowSoftness = MathUtil::Max(m_settings.lighting.shadowSoftness, 0.0f);

    cloudData.weatherWindOffset = Vec2f(
        float(WrapToPeriod(m_windOffsetX, weatherPeriod)),
        float(WrapToPeriod(m_windOffsetZ, weatherPeriod)));

    cloudData.shapeWindOffset = Vec2f(
        float(WrapToPeriod(m_windOffsetX, double(shapeNoiseScale))),
        float(WrapToPeriod(m_windOffsetZ, double(shapeNoiseScale))));

    cloudData.detailWindOffset = Vec2f(
        float(WrapToPeriod(m_windOffsetX * DetailWindSpeedMultiplier, double(detailNoiseScale))),
        float(WrapToPeriod(m_windOffsetZ * DetailWindSpeedMultiplier, double(detailNoiseScale))));

    cloudData.enabled = m_settings.enabled ? 1u : 0u;
    cloudData.cloudSize = MathUtil::Max(m_settings.noise.cloudSize, MinNoiseScale);

    proxy->bufferData.SetParams(cloudData);
}

} // namespace Hyperion
