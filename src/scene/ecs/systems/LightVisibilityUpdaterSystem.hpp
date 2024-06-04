/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_LIGHT_VISIBILITY_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_LIGHT_VISIBILITY_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <rendering/RenderProxy.hpp>

namespace hyperion {

class LightVisibilityUpdaterSystem : public System<
    ComponentDescriptor<LightComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,

    // Can read and write the VisibilityStateComponent but does not receive events
    ComponentDescriptor<VisibilityStateComponent, COMPONENT_RW_FLAGS_READ_WRITE, false>,
    // Can read and write the MeshComponent but does not receive events (updates material render data)
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE, false>
>
{
public:
    LightVisibilityUpdaterSystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~LightVisibilityUpdaterSystem() override = default;

    virtual void OnEntityAdded(ID<Entity> entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif