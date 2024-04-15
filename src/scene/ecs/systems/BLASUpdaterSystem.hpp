/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_BLAS_UPDATER_SYSTEM_HPP
#define HYPERION_V2_ECS_BLAS_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/BLASComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>

namespace hyperion::v2 {

class BLASUpdaterSystem : public System<
    ComponentDescriptor<BLASComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    virtual ~BLASUpdaterSystem() override = default;

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity) override;
    virtual void OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity) override;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif