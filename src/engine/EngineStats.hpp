/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/Name.hpp>

#include <core/memory/Pimpl.hpp>
#include <core/memory/pool/Pool.hpp>

#include <core/threading/ThreadId.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/containers/FixedArray.hpp>

#include <util/GameCounter.hpp>

#include <cfloat>

namespace hyperion {

class EngineStatsRecorder;
struct EngineStatsSnapshot;

static constexpr uint32 EngineStatsNumSamples = 1000;
static constexpr uint32 EngineStatsMinSamples = 10;
static constexpr uint32 EngineStatsMaxStats = 32;

static constexpr int StatIdMsPerFrame = 0;
static constexpr int StatIdFps = 1;

HYP_API extern Pool* EngineStats_GetPool();

HYP_API extern void EngineStats_Initialize();
HYP_API extern void EngineStats_Shutdown();

HYP_API extern EngineStatsRecorder* g_engineStatsRecorder;

enum EngineStatType : int
{
    EST_INVALID = -1,

    EST_TIMER = 0,
    EST_COUNTER,
    EST_GROUP,
    EST_FPS,

    EST_MAX
};

enum EngineStatThreadType : int
{
    ESTT_INVALID = -1,

    ESTT_RENDER = 0,
    ESTT_GAME,

    ESTT_MAX
};

class HYP_API EngineStatBase
{
protected:
    EngineStatBase(EngineStatType type, UTF8StringView path, EngineStatThreadType threadType);
    EngineStatBase(EngineStatType type, UTF8StringView path, EngineStatThreadType threadType, bool skipPathParsing);

public:
    HYP_DEF_POOL_NEW_DELETE(EngineStats_GetPool());

    virtual ~EngineStatBase() = default;

    int id;
    Name name;
    EngineStatType type;
    EngineStatThreadType threadType;

    virtual double GetValue() const
    {
        return 0.0;
    }

    virtual void Reset()
    {
    }
};

class HYP_API EngineStatsRecorder
{
public:
    HYP_DEF_POOL_NEW_DELETE(EngineStats_GetPool());

    EngineStatsRecorder();
    ~EngineStatsRecorder() = default;

    EngineStatsSnapshot& GetCurrentSnapshot();
    const EngineStatsSnapshot& GetCurrentSnapshot() const;

    void Suppress();
    void Unsuppress();

    void Prepare();
    void Advance();

    void BeginGameStatsFrame();
    void PublishGameChannel();

private:
    void CalculateFps(uint32 sampleIdx, struct EngineStatsSnapshotValue& out) const;

    Pimpl<struct EngineStatsRecorderImpl> m_pImpl;
};

class HYP_API EngineStatGroup : public EngineStatBase
{
public:
    explicit EngineStatGroup(UTF8StringView path)
        : EngineStatBase(EST_GROUP, path, ESTT_INVALID)
    {
    }

private:
    friend class EngineStats;
    friend class EngineStatBase;

    // Internal constructor for creating intermediate groups during path parsing
    EngineStatGroup(UTF8StringView path, bool skipPathParsing)
        : EngineStatBase(EST_GROUP, path, ESTT_INVALID, skipPathParsing)
    {
    }

public:
    virtual ~EngineStatGroup() override;

    HYP_FIELD()
    Array<EngineStatBase*> stats;
};

template <class T>
class HYP_API EngineStatCounter : public EngineStatBase
{
public:
    explicit EngineStatCounter(UTF8StringView path, EngineStatThreadType threadType = ESTT_RENDER)
        : EngineStatBase(EST_COUNTER, path, threadType)
    {
    }

    HYP_FORCE_INLINE EngineStatCounter& operator+=(T amount)
    {
        m_value += amount;
        return *this;
    }

    HYP_FORCE_INLINE EngineStatCounter& operator++()
    {
        ++m_value;
        return *this;
    }

    HYP_FORCE_INLINE T operator++(int)
    {
        T oldValue = m_value;
        ++m_value;
        return oldValue;
    }

    HYP_FORCE_INLINE EngineStatCounter& operator-=(T amount)
    {
        m_value -= amount;
        return *this;
    }

    HYP_FORCE_INLINE EngineStatCounter& operator--()
    {
        --m_value;
        return *this;
    }

    HYP_FORCE_INLINE T operator--(int)
    {
        T oldValue = m_value;
        --m_value;
        return oldValue;
    }

    HYP_FORCE_INLINE EngineStatCounter& operator=(T value)
    {
        m_value = value;
        return *this;
    }

    HYP_FORCE_INLINE explicit operator T() const
    {
        return m_value;
    }

    virtual double GetValue() const override
    {
        return static_cast<double>(m_value);
    }

    virtual void Reset() override
    {
        m_value = T();
    }

private:
    T m_value;
};

class HYP_API EngineStatTimer : public EngineStatBase
{
public:
    explicit EngineStatTimer(UTF8StringView path, EngineStatThreadType threadType = ESTT_RENDER)
        : EngineStatBase(EST_TIMER, path, threadType),
          m_clock()
    {
    }

    void StartTiming()
    {
        m_clock.Start();
    }

    void StopTiming()
    {
        m_clock.Stop();
    }

    double GetElapsedMs() const
    {
        return m_clock.ElapsedMs();
    }

    virtual double GetValue() const override
    {
        return GetElapsedMs();
    }
    
    virtual void Reset() override
    {
        m_clock = PerformanceClock();
    }

private:
    PerformanceClock m_clock;
};

struct EngineStatScope
{
    EngineStatScope(EngineStatTimer* timer)
        : stat(timer)
    {
        if (timer)
        {
            timer->StartTiming();
        }
    }

    EngineStatScope(const EngineStatScope&) = delete;
    EngineStatScope& operator=(const EngineStatScope&) = delete;

    EngineStatScope(EngineStatScope&&) noexcept = delete;
    EngineStatScope& operator=(EngineStatScope&&) noexcept = delete;

    ~EngineStatScope()
    {
        if (stat)
        {
            switch (stat->type)
            {
            case EST_TIMER:
                static_cast<EngineStatTimer*>(stat)->StopTiming();
                break;
            default:
                break;
            }
        }
    }

    EngineStatBase* stat;
};

struct EngineStatsSnapshotValue
{
    int statId;
    EngineStatType type;
    double value;
    double min;
    double max;
    double avg;
};

struct EngineStatsSnapshot
{
    EngineStatsSnapshotValue values[EngineStatsMaxStats];

    EngineStatsSnapshot()
    {
        for (uint32 i = 0; i < EngineStatsMaxStats; i++)
        {
            values[i] = {};
            values[i].statId = i;
            values[i].type = EST_MAX;
            values[i].value = 0.0;
            values[i].min = DBL_MAX;
            values[i].max = -DBL_MAX;
            values[i].avg = 0.0;
        }
    }

    EngineStatsSnapshotValue& operator[](int statId)
    {
        return values[statId];
    }

    const EngineStatsSnapshotValue& operator[](int statId) const
    {
        return values[statId];
    }

    double& operator[](const EngineStatBase& stat)
    {
        return values[stat.id].value;
    }

    double operator[](const EngineStatBase& stat) const
    {
        return values[stat.id].value;
    }

    HYP_FORCE_INLINE double GetValue(int statId, double defaultValue = 0.0) const
    {
        const EngineStatsSnapshotValue* snapshotValue = statId >= 0 && statId < int(EngineStatsMaxStats)
            ? &values[statId]
            : nullptr;

        return snapshotValue ? snapshotValue->value : defaultValue;
    }

    HYP_FORCE_INLINE double GetAverage(int statId, double defaultValue = 0.0) const
    {
        const EngineStatsSnapshotValue* snapshotValue = statId >= 0 && statId < int(EngineStatsMaxStats)
            ? &values[statId]
            : nullptr;

        return snapshotValue ? snapshotValue->avg : defaultValue;
    }

    EngineStatsSnapshot& Merge(const EngineStatsSnapshot& other)
    {
        for (uint32 i = 0; i < EngineStatsMaxStats; i++)
        {
            EngineStatsSnapshotValue& thisValue = values[i];
            const EngineStatsSnapshotValue& otherValue = other.values[i];

            if (otherValue.type != EST_INVALID)
            {
                thisValue.statId = otherValue.statId;
                thisValue.type = otherValue.type;
                thisValue.value += otherValue.value;
                thisValue.min = MathUtil::Min(thisValue.min, otherValue.min);
                thisValue.max = MathUtil::Max(thisValue.max, otherValue.max);
                // @NOTE avg is not merged here as it would break, due to using weighted averages
            }
        }

        return *this;
    }
};

#define ENGINE_STAT_SCOPE(timer) EngineStatScope HYP_CONCAT(engineStatScope, __LINE__)(timer)

} // namespace hyperion
