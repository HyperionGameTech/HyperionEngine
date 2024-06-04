/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_DRAW_DATA_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_ENTITY_DRAW_DATA_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <rendering/RenderProxy.hpp>

namespace hyperion {

class EntityDrawDataUpdaterSystem : public System<
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    EntityDrawDataUpdaterSystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~EntityDrawDataUpdaterSystem() override = default;

    virtual void OnEntityAdded(ID<Entity> entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif