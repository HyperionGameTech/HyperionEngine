#ifndef HYPERION_V2_ECS_SYSTEM_HPP
#define HYPERION_V2_ECS_SYSTEM_HPP

#include <GameCounter.hpp>

namespace hyperion::v2 {

class SystemBase
{
public:
    virtual ~SystemBase() = default;

    virtual void Process(GameCounter::TickUnit delta) = 0;
};

} // namespace hyperion::v2

#endif