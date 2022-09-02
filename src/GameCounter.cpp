#include "GameCounter.hpp"

#include <system/SDLSystem.hpp>

namespace hyperion::v2 {

void GameCounter::NextTick()
{
    const auto current = Now();

    delta = Interval(current);
    last_time_point = current;
}

} // namespace hyperion::v2