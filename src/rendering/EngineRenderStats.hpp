/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENGINE_RENDER_STATS_HPP
#define HYPERION_ENGINE_RENDER_STATS_HPP

#include <core/containers/FixedArray.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

#include <cfloat>

namespace hyperion {

HYP_STRUCT()
struct EngineRenderStats
{
    double  frames_per_second = 0.0;
    double  milliseconds_per_frame = 0.0;
    double  milliseconds_per_frame_avg = 0.0;
    double  milliseconds_per_frame_max = 0.0;
    double  milliseconds_per_frame_min = DBL_MAX;
};

class EngineRenderStatsCalculator
{
    static constexpr uint32 max_samples = 512;

public:
    void Advance(EngineRenderStats &render_stats);

    double CalculateFramesPerSecond() const;
    double CalculateMillisecondsPerFrame() const;

private:
    HYP_FORCE_INLINE uint32 GetSampleIndex() const
    {
        return m_num_samples % max_samples;
    }

    HYP_FORCE_INLINE void AddSample(double delta)
    {
        const uint32 sample_index = m_num_samples++;

        m_samples[sample_index % max_samples] = delta;
    }

    GameCounter                     m_counter;
    double                          m_delta_accum = 0.0;
    FixedArray<double, max_samples> m_samples { 0.0 };
    uint32                          m_num_samples = 0;
};

} // namespace hyperion

#endif // HYPERION_ENGINE_RENDER_STATS_HPP