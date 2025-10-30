/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/Name.hpp>

#include <core/memory/Pimpl.hpp>
#include <core/memory/pool/Pool.hpp>

#include <core/threading/util/ThreadId.hpp>

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

    /*! \brief Record a value set to be integrated into samples.
     *  Call this to add values that will be included in the next Advance() calculation.
     * Call only from Render thread! */
    void RecordValueSet(const struct EngineStatsValueSet& valueSet);

private:
    double CalculateFps() const;

    void SetSampleData(int statId, uint32 sampleIdx, double value);
    double GetSampleData(int statId, uint32 sampleIdx) const;

    void RecordStat(int statId, EngineStatType type, double value);

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
        m_value = 0;
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

struct EngineStatsValueSet
{
    double values[EngineStatsMaxStats];

    EngineStatsValueSet()
        : values()
    {
    }

    double& operator[](int statId)
    {
        return values[statId];
    }

    double operator[](int statId) const
    {
        return values[statId];
    }

    double& operator[](const EngineStatBase& stat)
    {
        return values[stat.id];
    }

    double operator[](const EngineStatBase& stat) const
    {
        return values[stat.id];
    }

    EngineStatsValueSet& operator+=(const EngineStatsValueSet& other)
    {
        for (uint32 i = 0; i < EngineStatsMaxStats; i++)
        {
            values[i] += other.values[i];
        }

        return *this;
    }
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

    const EngineStatsSnapshotValue& operator[](int statId) const
    {
        return values[statId];
    }

    const EngineStatsSnapshotValue& operator[](const EngineStatBase& stat) const
    {
        return values[stat.id];
    }
};

#define ENGINE_STAT_SCOPE(timer) EngineStatScope HYP_CONCAT(engineStatScope, __LINE__)(timer)

} // namespace hyperion
