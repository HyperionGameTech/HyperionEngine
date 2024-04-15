/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_ECS_SKY_SYSTEM_HPP
#define HYPERION_V2_ECS_SKY_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

namespace hyperion::v2 {

class SkySystem : public System<
    ComponentDescriptor<SkyComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    virtual ~SkySystem() override = default;

    virtual void OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity) override;
    virtual void OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity) override;

    virtual void Process(EntityManager &entity_manager, GameCounter::TickUnit delta) override;
};

} // namespace hyperion::v2

#endif