#ifndef HYPERION_V2_ECS_SYSTEM_HPP
#define HYPERION_V2_ECS_SYSTEM_HPP

#include <scene/ecs/EntitySet.hpp>
#include <GameCounter.hpp>

namespace hyperion::v2 {

class EntityManager;

class SystemBase
{
public:
    virtual ~SystemBase() = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) = 0;
};

using SystemContainer = Array<UniquePtr<SystemBase>>;

} // namespace hyperion::v2

#endif