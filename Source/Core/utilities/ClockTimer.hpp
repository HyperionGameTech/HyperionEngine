/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <chrono>

namespace Hyperion {

struct ClockTimer
{
    using Clock = std::chrono::high_resolution_clock;

    using TickUnit = float;
    using TickUnitHighPrec = double;
    using TimePoint = Clock::time_point;

    TimePoint lastTimePoint = Now();
    TickUnit delta {};
    TickUnit targetInterval {};

    ClockTimer()
        : targetInterval(0)
    {
    }

    HYP_FORCE_INLINE explicit ClockTimer(TickUnit targetInterval)
        : targetInterval(targetInterval)
    {
    }

    HYP_FORCE_INLINE static TimePoint Now()
    {
        return Clock::now();
    }

    HYP_FORCE_INLINE void NextTick()
    {
        const TimePoint current = Now();

        delta = Interval(current);
        lastTimePoint = current;
    }

    HYP_FORCE_INLINE void Reset()
    {
        lastTimePoint = Now();
        delta = TickUnit(0.0);
    }

    HYP_FORCE_INLINE TickUnit Interval(TimePoint endTimePoint) const
    {
        return std::chrono::duration_cast<std::chrono::duration<TickUnit, std::ratio<1>>>(endTimePoint - lastTimePoint).count();
    }

    HYP_FORCE_INLINE TickUnitHighPrec IntervalHighPrec(TimePoint endTimePoint) const
    {
        return std::chrono::duration_cast<std::chrono::duration<TickUnitHighPrec, std::ratio<1>>>(endTimePoint - lastTimePoint).count();
    }

    HYP_FORCE_INLINE bool Waiting() const
    {
        return targetInterval > 0
            && Interval(Now()) < targetInterval;
    }
};

} // namespace Hyperion
