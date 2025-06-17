/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/EngineRenderStats.hpp>

#include <core/math/MathUtil.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/threading/Threads.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region SuppressEngineRenderStatsScope

SuppressEngineRenderStatsScope::SuppressEngineRenderStatsScope()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    g_engine->GetRenderStatsCalculator().Suppress();
}

SuppressEngineRenderStatsScope::~SuppressEngineRenderStatsScope()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    g_engine->GetRenderStatsCalculator().Unsuppress();
}

#pragma endregion SuppressEngineRenderStatsScope

#pragma region EngineRenderStatsCalculator

void EngineRenderStatsCalculator::AddCounts(const EngineRenderStatsCounts& counts)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_suppress_count > 0)
    {
        return;
    }

    for (uint32 i = 0; i < ERS_MAX; ++i)
    {
        m_counts.counts[i] += counts.counts[i];
    }
}

void EngineRenderStatsCalculator::AddSample(double delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_suppress_count > 0)
    {
        return;
    }

    const uint32 sample_index = m_num_samples++;

    m_samples[sample_index % max_samples] = delta;
}

void EngineRenderStatsCalculator::Advance(EngineRenderStats& render_stats)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

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
    new_render_stats.counts = m_counts;

    render_stats = new_render_stats;

    if (reset_min_max)
    {
        m_delta_accum = 0.0;
    }

    Memory::MemSet(m_counts.counts, 0, sizeof(m_counts.counts[0]) * ERS_MAX);
}

double EngineRenderStatsCalculator::CalculateFramesPerSecond() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_num_samples == 0)
    {
        return 0.0;
    }

    const uint32 count = m_num_samples < max_samples ? m_num_samples : max_samples;
    double total = 0.0;

    for (uint32 i = 0; i < count; ++i)
    {
        total += 1.0 / m_samples[i];
    }

    return total / MathUtil::Max(double(count), MathUtil::epsilon_d);
}

double EngineRenderStatsCalculator::CalculateMillisecondsPerFrame() const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_num_samples == 0)
    {
        return 0.0;
    }

    const uint32 count = m_num_samples < max_samples ? m_num_samples : max_samples;
    double total = 0.0;

    for (uint32 i = 0; i < count; ++i)
    {
        total += m_samples[i] * 1000.0;
    }

    return total / MathUtil::Max(double(count), MathUtil::epsilon_d);
}

#pragma endregion EngineRenderStatsCalculator

} // namespace hyperion
