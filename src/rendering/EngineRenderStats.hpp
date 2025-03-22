/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENGINE_RENDER_STATS_HPP
#define HYPERION_ENGINE_RENDER_STATS_HPP

#include <core/containers/FixedArray.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

#include <cfloat>

namespace hyperion {

struct EngineRenderStatsCounts
{
    uint32  num_draw_calls = 0;
    uint32  num_triangles = 0;
};

HYP_STRUCT()
struct EngineRenderStats
{
    double                  frames_per_second = 0.0;
    double                  milliseconds_per_frame = 0.0;
    double                  milliseconds_per_frame_avg = 0.0;
    double                  milliseconds_per_frame_max = 0.0;
    double                  milliseconds_per_frame_min = DBL_MAX;
    EngineRenderStatsCounts counts;
};

struct HYP_API SuppressEngineRenderStatsScope
{
    SuppressEngineRenderStatsScope();
    ~SuppressEngineRenderStatsScope();
};

class EngineRenderStatsCalculator
{
    static constexpr uint32 max_samples = 512;

public:
    friend struct SuppressEngineRenderStatsScope;

    void AddCounts(const EngineRenderStatsCounts &counts);
    void Advance(EngineRenderStats &render_stats);

private:
    HYP_FORCE_INLINE void Suppress()
    {
        m_suppress_count++;
    }

    HYP_FORCE_INLINE void Unsuppress()
    {
        if (m_suppress_count > 0) {
            m_suppress_count--;
        }
    }

    double CalculateFramesPerSecond() const;
    double CalculateMillisecondsPerFrame() const;

    void AddSample(double delta);

    GameCounter                     m_counter;
    double                          m_delta_accum = 0.0;
    FixedArray<double, max_samples> m_samples { 0.0 };
    uint32                          m_num_samples = 0;
    EngineRenderStatsCounts         m_counts;

    int                             m_suppress_count = 0;
};

} // namespace hyperion

#endif // HYPERION_ENGINE_RENDER_STATS_HPP