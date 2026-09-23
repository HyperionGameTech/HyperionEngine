/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Utilities/ClockTimer.hpp>

#include <chrono>

namespace Hyperion {

using ClockTimerClock = std::chrono::high_resolution_clock;

static_assert(sizeof(ClockTimerClock::rep) == sizeof(ClockTimer::TimePoint), "TimePoint must hold the clock's tick count");

ClockTimer::TimePoint ClockTimer::Now()
{
    return TimePoint(ClockTimerClock::now().time_since_epoch().count());
}

ClockTimer::TickUnit ClockTimer::Interval(TimePoint endTimePoint) const
{
    return std::chrono::duration_cast<std::chrono::duration<TickUnit, std::ratio<1>>>(ClockTimerClock::duration(endTimePoint - lastTimePoint)).count();
}

ClockTimer::TickUnitHighPrec ClockTimer::IntervalHighPrec(TimePoint endTimePoint) const
{
    return std::chrono::duration_cast<std::chrono::duration<TickUnitHighPrec, std::ratio<1>>>(ClockTimerClock::duration(endTimePoint - lastTimePoint)).count();
}

} // namespace Hyperion
