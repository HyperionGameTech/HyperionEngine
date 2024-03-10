#ifndef HYPERION_V2_ECS_SHADOW_MAP_UPDATER_SYSTEM_HPP
#define HYPERION_V2_ECS_SHADOW_MAP_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

namespace hyperion::v2 {

class ShadowMapUpdaterSystem : public System<
    ComponentDescriptor<ShadowMapComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<LightComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    virtual ~ShadowMapUpdaterSystem() override = default;

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity) override;
    virtual void OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity) override;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif