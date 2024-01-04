#ifndef HYPERION_V2_ECS_VISIBILITY_STATE_UPDATER_SYSTEM_HPP
#define HYPERION_V2_ECS_VISIBILITY_STATE_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>

namespace hyperion::v2 {

class VisibilityStateUpdaterSystem : public SystemBase
{
public:
    virtual ~VisibilityStateUpdaterSystem() override = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif