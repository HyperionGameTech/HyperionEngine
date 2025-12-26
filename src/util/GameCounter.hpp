/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <chrono>

namespace Hyperion {

struct GameCounter
{
    using Clock = std::chrono::high_resolution_clock;

    using TickUnit = float;
    using TickUnitHighPrec = double;
    using TimePoint = Clock::time_point;

    TimePoint lastTimePoint = Now();
    TickUnit delta {};

    HYP_FORCE_INLINE static TimePoint Now()
    {
        return Clock::now();
    }

    void NextTick()
    {
        const TimePoint current = Now();

        delta = Interval(current);
        lastTimePoint = current;
    }

    void Reset()
    {
        lastTimePoint = Now();
        delta = TickUnit(0.0);
    }

    TickUnit Interval(TimePoint endTimePoint) const
    {
        return std::chrono::duration_cast<std::chrono::duration<TickUnit, std::ratio<1>>>(endTimePoint - lastTimePoint).count();
    }

    TickUnitHighPrec IntervalHighPrec(TimePoint endTimePoint) const
    {
        return std::chrono::duration_cast<std::chrono::duration<TickUnitHighPrec, std::ratio<1>>>(endTimePoint - lastTimePoint).count();
    }
};

struct LockstepGameCounter : GameCounter
{
    TickUnit targetInterval;
    TickUnit padding;

    LockstepGameCounter(TickUnit targetInterval, TickUnit padding = TickUnit(0.0))
        : GameCounter(),
          targetInterval(targetInterval),
          padding(padding)
    {
    }

    HYP_FORCE_INLINE bool Waiting() const
    {
        return Interval(Now()) < targetInterval - padding;
    }
};

} // namespace Hyperion
