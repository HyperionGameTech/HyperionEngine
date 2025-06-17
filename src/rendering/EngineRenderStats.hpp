/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENGINE_RENDER_STATS_HPP
#define HYPERION_ENGINE_RENDER_STATS_HPP

#include <core/containers/FixedArray.hpp>

#include <GameCounter.hpp>
#include <Types.hpp>

#include <cfloat>

namespace hyperion {

enum EngineRenderStatsCountType : uint32
{
    ERS_DRAW_CALLS = 0,
    ERS_INSTANCED_DRAW_CALLS,
    ERS_TRIANGLES,
    ERS_RENDER_GROUPS,
    ERS_VIEWS,
    ERS_SCENES,
    ERS_LIGHTS,
    ERS_LIGHTMAP_VOLUMES,
    ERS_ENV_PROBES,
    ERS_ENV_GRIDS,

    ERS_MAX
};

static_assert(ERS_MAX <= 16, "EngineRenderStatsCountType must not exceed 16 types");

struct EngineRenderStatsCounts
{
    uint32 counts[16] = { 0 };

    HYP_FORCE_INLINE constexpr uint32& operator[](EngineRenderStatsCountType type)
    {
        return counts[type];
    }

    HYP_FORCE_INLINE constexpr uint32 operator[](EngineRenderStatsCountType type) const
    {
        return counts[type];
    }
};

HYP_STRUCT()
struct EngineRenderStats
{
    double frames_per_second = 0.0;
    double milliseconds_per_frame = 0.0;
    double milliseconds_per_frame_avg = 0.0;
    double milliseconds_per_frame_max = 0.0;
    double milliseconds_per_frame_min = DBL_MAX;
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

    void AddCounts(const EngineRenderStatsCounts& counts);
    void Advance(EngineRenderStats& render_stats);

private:
    HYP_FORCE_INLINE void Suppress()
    {
        m_suppress_count++;
    }

    HYP_FORCE_INLINE void Unsuppress()
    {
        if (m_suppress_count > 0)
        {
            m_suppress_count--;
        }
    }

    double CalculateFramesPerSecond() const;
    double CalculateMillisecondsPerFrame() const;

    void AddSample(double delta);

    GameCounter m_counter;
    double m_delta_accum = 0.0;
    FixedArray<double, max_samples> m_samples { 0.0 };
    uint32 m_num_samples = 0;
    EngineRenderStatsCounts m_counts;

    int m_suppress_count = 0;
};

} // namespace hyperion

#endif // HYPERION_ENGINE_RENDER_STATS_HPP