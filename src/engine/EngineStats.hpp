/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

#include <core/object/HypObject.hpp>
#include <core/object/Handle.hpp>

#include <core/Name.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <cfloat>

namespace hyperion {

static constexpr uint32 g_maxStatGroups = 32;
static constexpr uint32 g_maxStatEntriesPerGroup = 32;

HYP_STRUCT()
struct EngineStatEntry
{ 
    Name name;

    union
    {
        float valueFloat;
        double valueDouble;
        int32 valueInt32;
        uint32 valueUint32;
        int64 valueInt64;
        uint64 valueUint64;
    };

    enum
    {
        TYPE_FLOAT = 0,
        TYPE_DOUBLE,
        TYPE_INT32,
        TYPE_UINT32,
        TYPE_INT64,
        TYPE_UINT64
    } type;
};

HYP_CLASS()
class HYP_API EngineStatGroup : public HypObjectBase
{
    HYP_OBJECT_BODY(EngineStatGroup);

public:
    ~EngineStatGroup() = default;

    HYP_FIELD()
    Name name;

    HYP_FIELD()
    EngineStatEntry entries[g_maxStatEntriesPerGroup];

    HYP_FIELD()
    uint32 numEntries = 0;
};

HYP_CLASS()
class HYP_API EngineStats final : public HypObjectBase
{
    HYP_OBJECT_BODY(EngineStats);

public:
    ~EngineStats() override = default;

    HYP_FIELD()
    Handle<EngineStatGroup> statGroups[g_maxStatGroups];
};

class EngineStatTimer
{
public:
    EngineStatTimer(Name name)
        : m_name(name),
          m_clock()
    {
    }

    void Reset()
    {
        m_clock = PerformanceClock();
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
    
private:
    Name m_name;
    PerformanceClock m_clock;
};

} // namespace hyperion
