#ifndef HYPERION_V2_ECS_LIGHT_VISIBILITY_UPDATER_SYSTEM_HPP
#define HYPERION_V2_ECS_LIGHT_VISIBILITY_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <rendering/EntityDrawData.hpp>

namespace hyperion::v2 {

class LightVisibilityUpdaterSystem : public System<
    ComponentDescriptor<LightComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    virtual ~LightVisibilityUpdaterSystem() override = default;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif