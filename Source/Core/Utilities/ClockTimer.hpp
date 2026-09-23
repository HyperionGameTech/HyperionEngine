/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

struct ClockTimer
{
    using TickUnit = float;
    using TickUnitHighPrec = double;

    using TimePoint = int64;

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

    CORE_API static TimePoint Now();

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

    CORE_API TickUnit Interval(TimePoint endTimePoint) const;

    CORE_API TickUnitHighPrec IntervalHighPrec(TimePoint endTimePoint) const;

    HYP_FORCE_INLINE bool Waiting() const
    {
        return targetInterval > 0
            && Interval(Now()) < targetInterval;
    }
};

} // namespace Hyperion
