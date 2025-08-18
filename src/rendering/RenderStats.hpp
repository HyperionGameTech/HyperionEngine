/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <core/containers/FixedArray.hpp>

#include <util/GameCounter.hpp>
#include <core/Types.hpp>

#include <cfloat>

#define HYP_ENABLE_RENDER_STATS
#define HYP_ENABLE_RENDER_STATS_COUNTERS

namespace hyperion {

enum RenderStatsCountType : uint32
{
    ERS_DRAW_CALLS = 0,
    ERS_INSTANCED_DRAW_CALLS,
    ERS_TRIANGLES,
    ERS_RENDER_GROUPS,
    ERS_VIEWS,
    ERS_TEXTURES,
    ERS_MATERIALS,
    ERS_LIGHTS,
    ERS_LIGHTMAP_VOLUMES,
    ERS_ENV_PROBES,
    ERS_ENV_GRIDS,

    ERS_MAX
};

static_assert(ERS_MAX <= 16, "RenderStatsCountType must not exceed 16 types");

struct RenderStatsCounts
{
    uint32 counts[16] = { 0 };

    HYP_FORCE_INLINE constexpr uint32& operator[](RenderStatsCountType type)
    {
        return counts[type];
    }

    HYP_FORCE_INLINE constexpr uint32 operator[](RenderStatsCountType type) const
    {
        return counts[type];
    }
};

HYP_STRUCT()
struct RenderStats
{
    double framesPerSecond = INFINITY;
    double millisecondsPerFrame = 0.0;
    double millisecondsPerFrameAvg = 0.0;
    double millisecondsPerFrameMax = 0.0;
    double millisecondsPerFrameMin = DBL_MAX;

    uint32 deletionQueueNumElements = 0;
    uint32 deletionQueueTotalBytes = 0;

    RenderStatsCounts counts;
};

struct HYP_API SuppressRenderStatsScope
{
    SuppressRenderStatsScope();
    ~SuppressRenderStatsScope();
};

class RenderStatsCalculator
{
    static constexpr uint32 g_maxSamples = 1000;
    static constexpr uint32 g_minSamples = 10; // to wait before calculating

public:
    friend struct SuppressRenderStatsScope;

    RenderStatsCalculator()
        : m_deltaAccum(0.0),
          m_numSamples(0),
          m_sampleIndex(0),
          m_suppressCount(0)
    {
        Memory::MemSet(&m_sampleData[0], 0, sizeof(m_sampleData));

        m_counter.delta = 1.0;
    }

    HYP_FORCE_INLINE void Suppress()
    {
        m_suppressCount++;
    }

    HYP_FORCE_INLINE void Unsuppress()
    {
        if (m_suppressCount > 0)
        {
            m_suppressCount--;
        }
    }

    void AddCounts(const RenderStatsCounts& counts);
    void Advance(RenderStats& renderStats);

private:
    double CalculateFramesPerSecond() const;
    double CalculateMillisecondsPerFrame() const;

    void AddSample(double delta);

    GameCounter m_counter;
    double m_deltaAccum;
    uint32 m_numSamples;
    uint32 m_sampleIndex;
    RenderStatsCounts m_counts;
    int m_suppressCount;

    double m_sampleData[g_maxSamples];
};

} // namespace hyperion
