/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SKY_SYSTEM_HPP
#define HYPERION_ECS_SKY_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

namespace hyperion {

class SkySystem : public System<
    ComponentDescriptor<SkyComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ>
>
{
public:
    SkySystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~SkySystem() override = default;

    virtual void OnEntityAdded(ID<Entity> entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif