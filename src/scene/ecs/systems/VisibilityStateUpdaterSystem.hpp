/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_VISIBILITY_STATE_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_VISIBILITY_STATE_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

namespace hyperion {

class VisibilityStateUpdaterSystem : public System<VisibilityStateUpdaterSystem, ComponentDescriptor<VisibilityStateComponent, COMPONENT_RW_FLAGS_READ_WRITE>, ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ>,

                                         ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_VISIBILITY_STATE>, COMPONENT_RW_FLAGS_READ, false>>
{
public:
    VisibilityStateUpdaterSystem(EntityManager& entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~VisibilityStateUpdaterSystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity>& entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif