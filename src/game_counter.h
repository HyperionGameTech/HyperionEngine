#ifndef HYPERION_V2_GAME_COUNTER_H
#define HYPERION_V2_GAME_COUNTER_H

#include <types.h>

#include <chrono>

namespace hyperion::v2 {

struct GameCounter {
    using Clock      = std::chrono::high_resolution_clock;

    using TickUnit   = Float32;
    using TimePoint  = Clock::time_point;

    TimePoint last_time_point = Now();
    TickUnit  delta{};

    void NextTick();
    static TimePoint Now();

    TickUnit Interval(TimePoint end_time_point) const
    {
        using namespace std::chrono;

        return duration_cast<duration<TickUnit, std::ratio<1>>>(end_time_point - last_time_point).count();
    }
};

struct LockstepGameCounter : GameCounter {
    TickUnit target_interval;
    TickUnit padding;

    LockstepGameCounter(TickUnit target_interval, TickUnit padding = TickUnit(0.00015))
        : GameCounter(),
          target_interval(target_interval),
          padding(padding)
    {
    }

    bool Waiting() const
    {
        return Interval(Now()) < target_interval - padding;
    }
};

} // namespace hyperion::v2

#endif