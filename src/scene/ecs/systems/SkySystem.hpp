/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SKY_SYSTEM_HPP
#define HYPERION_ECS_SKY_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

namespace hyperion {

class SkySystem : public System<
    SkySystem,
    ComponentDescriptor<SkyComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE, false>
>
{
public:
    SkySystem(EntityManager &entity_manager);
    virtual ~SkySystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;

private:
    void AddRenderSubsystemToEnvironment(SkyComponent &sky_component, MeshComponent &mesh_component, World *world);
};

} // namespace hyperion

#endif