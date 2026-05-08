/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/memory/Pimpl.hpp>
#include <Core/memory/pool/Pool.hpp>

#include <Core/threading/util/ThreadId.hpp>

#include <Core/profiling/PerformanceClock.hpp>

#include <Core/utilities/ClockTimer.hpp>

#include <cfloat>

namespace Hyperion {

class EngineStatsRecorder;
struct EngineStatsSnapshot;
class EngineStatGroup;

static constexpr uint32 EngineStatsNumSamples = 1000;
static constexpr uint32 EngineStatsMinSamples = 10;
static constexpr uint32 EngineStatsMaxStats = 32;

static constexpr int StatIdMsPerFrame = 0;
static constexpr int StatIdFps = 1;

enum EngineStatType : uint8
{
    EST_TIMER = 0,
    EST_COUNTER,
    EST_GROUP,
    EST_MAX
};

class EngineStatBase
{
protected:
    EngineStatBase(EngineStatType type, UTF8StringView path);
    EngineStatBase(EngineStatType type, UTF8StringView path, bool skipPathParsing);

public:
    int id;
    Name name;

    EngineStatType type : 2;
    bool isHeapAllocated : 1;
    bool resetPerFrame : 1;

    virtual ~EngineStatBase() = default;

    virtual double GetValue() const
    {
        return 0.0;
    }

    virtual void Reset()
    {
    }
};

class HYP_API EngineStatGroup : public EngineStatBase
{
public:
    friend class EngineStats;
    friend class EngineStatBase;

    explicit EngineStatGroup(UTF8StringView path)
        : EngineStatBase(EST_GROUP, path)
    {
    }

    EngineStatGroup(UTF8StringView path, bool skipPathParsing)
        : EngineStatBase(EST_GROUP, path, skipPathParsing)
    {
    }

public:
    virtual ~EngineStatGroup() override;

    Array<EngineStatBase*> stats;
};

template <class T>
class HYP_API EngineStatCounter : public EngineStatBase
{
public:
    static_assert(sizeof(T) <= 8, "sizeof(T) must be <= 8");
    static_assert(std::is_integral_v<T>, "EngineStatCounter can only be instantiated with integral types");

    explicit EngineStatCounter(UTF8StringView path, bool resetPerFrame = true)
        : EngineStatBase(EST_COUNTER, path),
          m_value {}
    {
        EngineStatBase::resetPerFrame = resetPerFrame;
    }

    HYP_FORCE_INLINE EngineStatCounter& operator+=(T amount)
    {
        AtomicAdd(&m_value, static_cast<InternalType>(amount));
        return *this;
    }

    HYP_FORCE_INLINE EngineStatCounter& operator++()
    {
        AtomicIncrement(&m_value);
        return *this;
    }

    HYP_FORCE_INLINE T operator++(int)
    {
        InternalType oldValue = AtomicAdd(&m_value, 1);
        return static_cast<T>(oldValue);
    }

    HYP_FORCE_INLINE EngineStatCounter& operator-=(T amount)
    {
        AtomicSub(&m_value, static_cast<InternalType>(amount));
        return *this;
    }

    HYP_FORCE_INLINE EngineStatCounter& operator--()
    {
        AtomicDecrement(&m_value);
        return *this;
    }

    HYP_FORCE_INLINE T operator--(int)
    {
        InternalType oldValue = AtomicSub(&m_value, 1);
        return static_cast<T>(oldValue);
    }

    HYP_FORCE_INLINE EngineStatCounter& operator=(T value)
    {
        AtomicExchange(&m_value, static_cast<InternalType>(value));
        return *this;
    }

    HYP_FORCE_INLINE explicit operator T() const
    {
        return static_cast<T>(AtomicAdd(const_cast<volatile InternalType*>(&m_value), 0));
    }

    virtual double GetValue() const override
    {
        return static_cast<double>(static_cast<T>(AtomicAdd(const_cast<volatile InternalType*>(&m_value), InternalType(0))));
    }

    virtual void Reset() override
    {
        AtomicExchange(&m_value, InternalType(0));
    }

private:
    using InternalType = std::conditional_t<sizeof(T) == 8, int64, int32>;

    volatile InternalType m_value;
};

class HYP_API EngineStatTimer : public EngineStatBase
{
public:
    explicit EngineStatTimer(UTF8StringView path, bool resetPerFrame = true)
        : EngineStatBase(EST_TIMER, path),
          m_clock(),
          m_totalMs(0.0)
    {
        EngineStatBase::resetPerFrame = resetPerFrame;
    }

    /// @TODO Make thread safe with atomics.
    /// Just record the duration, use EngineStatScope to handle the actual timing and recording of the value.
    /// Then use uint64 for the value and convert to double in GetValue() to avoid issues with atomics and floating point types.

    void StartTiming()
    {
        m_clock.Start();
    }

    void StopTiming()
    {
        m_clock.Stop();
        m_totalMs += m_clock.ElapsedMs();
    }

    virtual double GetValue() const override
    {
        return m_totalMs;
    }

    virtual void Reset() override
    {
        m_clock = PerformanceClock();
        m_totalMs = 0.0;
    }

private:
    PerformanceClock m_clock;
    double m_totalMs;
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

HYP_CLASS()
class EngineStats : public ObjectBase
{
    HYP_OBJECT_BODY(EngineStats);

public:
    HYP_METHOD()
    static const Handle<EngineStats>& GetInstance();

    EngineStats();

    EngineStats(const EngineStats&) = delete;
    EngineStats& operator=(const EngineStats&) = delete;

    ~EngineStats();

    EngineStatsSnapshot& GetCurrentSnapshot();
    const EngineStatsSnapshot& GetCurrentSnapshot() const;

    EngineStatBase* GetStat(UTF8StringView path) const;

    HYP_METHOD()
    double GetFps() const;

    HYP_METHOD()
    double GetMsPerFrame() const;

    HYP_METHOD()
    double QueryStatValue(UTF8StringView path, double valueIfNotFound = 0.0) const;

    void Prepare();
    void Advance();

    /*! \brief Record a value set to be integrated into samples.
     *  Call this to add values that will be included in the next Advance() calculation.
     * Call only from Render thread! */
    void RecordValueSet(const struct EngineStatsValueSet& valueSet);

    EngineStatGroup* root;
    FixedArray<EngineStatBase*, EngineStatsMaxStats> linearStats;

private:
    double CalculateFps() const;

    void SetSampleData(int statId, uint32 sampleIdx, double value);
    double GetSampleData(int statId, uint32 sampleIdx) const;

    void RecordStat(int statId, EngineStatType type, double value);

    Pimpl<struct EngineStatsRecorderImpl> m_impl;
};

#define ENGINE_STAT_SCOPE(timer) EngineStatScope HYP_CONCAT(engineStatScope, __LINE__)(timer)

} // namespace Hyperion
