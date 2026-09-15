/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/EffectVolume.hpp>
#include <Scene/Sky/CloudSettings.hpp>

namespace Hyperion {

/*! \brief Carries the world's cloud settings and cloud clock to the render thread. Unbounded; owned by DynamicSkySystem. */
HYP_CLASS()
class ENGINE_API CloudEffectVolume final : public EffectVolume
{
    HYP_OBJECT_BODY(CloudEffectVolume);

public:
    // weather noise repeats every this many WeatherScale-sized cells
    static constexpr uint32 WeatherNoisePeriodCells = 64;

    // the weather noise's time axis: one cell per EvolutionSecondsPerCell of evolution, repeating every EvolutionNoisePeriodCells
    static constexpr uint32 EvolutionNoisePeriodCells = 16;
    static constexpr double EvolutionSecondsPerCell = 256.0;
    static constexpr double EvolutionTimePeriod = double(EvolutionNoisePeriodCells) * EvolutionSecondsPerCell;

    static constexpr double DetailWindSpeedMultiplier = 1.5;

    /*! \brief Wraps \p value into [0, period). Offsets fed to periodic noise are wrapped in double precision before being narrowed to float. */
    static double WrapToPeriod(double value, double period);

    CloudEffectVolume();

    CloudEffectVolume(const CloudEffectVolume&) = delete;
    CloudEffectVolume& operator=(const CloudEffectVolume&) = delete;

    ~CloudEffectVolume() override = default;

    HYP_FORCE_INLINE const CloudSettings& GetSettings() const
    {
        return m_settings;
    }

    void SetSettings(const CloudSettings& settings);

    /*! \brief Advances the evolution time and wind offset. Sim thread only. */
    void AdvanceClock(float delta);

    virtual void UpdateRenderProxy(RenderProxyEffectVolume* proxy) override;

private:
    CloudSettings m_settings;

    double m_evolutionTime;
    double m_windOffsetX;
    double m_windOffsetZ;
};

} // namespace Hyperion
