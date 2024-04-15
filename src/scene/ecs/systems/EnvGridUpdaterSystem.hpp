/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_ENV_GRID_UPDATER_SYSTEM_HPP
#define HYPERION_V2_ECS_ENV_GRID_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/EnvGridComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

namespace hyperion::v2 {

class EnvGridUpdaterSystem : public System<
    ComponentDescriptor<EnvGridComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    virtual ~EnvGridUpdaterSystem() override = default;

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity) override;
    virtual void OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity) override;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif