/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/EngineRenderStats.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

void EngineRenderStatsCalculator::Advance(EngineRenderStats &render_stats)
{
    m_counter.NextTick();
    m_delta_accum += m_counter.delta;

    const bool reset_min_max = m_delta_accum >= 1.0;

    AddSample(m_counter.delta);

    EngineRenderStats new_render_stats;
    new_render_stats.frames_per_second = CalculateFramesPerSecond();
    new_render_stats.milliseconds_per_frame = m_counter.delta * 1000.0;
    new_render_stats.milliseconds_per_frame_avg = CalculateMillisecondsPerFrame();
    new_render_stats.milliseconds_per_frame_max = reset_min_max
        ? new_render_stats.milliseconds_per_frame
        : MathUtil::Max(render_stats.milliseconds_per_frame_max, new_render_stats.milliseconds_per_frame);
    new_render_stats.milliseconds_per_frame_min = reset_min_max
        ? new_render_stats.milliseconds_per_frame
        : MathUtil::Min(render_stats.milliseconds_per_frame_min, new_render_stats.milliseconds_per_frame);

    if (reset_min_max) {
        m_delta_accum = 0.0;
    }
    
    render_stats = new_render_stats;
}

double EngineRenderStatsCalculator::CalculateFramesPerSecond() const
{
    const uint32 count = m_num_samples < max_samples ? m_num_samples : max_samples;
    double total = 0.0;

    for (uint32 i = 0; i < count; ++i) {
        total += 1.0 / m_samples[i];
    }

    return total / MathUtil::Max(double(count), MathUtil::epsilon_d);
}

double EngineRenderStatsCalculator::CalculateMillisecondsPerFrame() const
{
    const uint32 count = m_num_samples < max_samples ? m_num_samples : max_samples;
    double total = 0.0;

    for (uint32 i = 0; i < count; ++i) {
        total += m_samples[i] * 1000.0;
    }

    return total / MathUtil::Max(double(count), MathUtil::epsilon_d);
}

} // namespace hyperion
