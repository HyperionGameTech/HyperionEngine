/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SKY_SYSTEM_HPP
#define HYPERION_ECS_SKY_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>
#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>

namespace hyperion {

class SkySystem : public System<
    SkySystem,
    ComponentDescriptor<SkyComponent, COMPONENT_RW_FLAGS_READ_WRITE>,

    // calling EnvProbe::Update() calls View::Update() which reads the following components on entities it processes.
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ, false>,
    ComponentDescriptor<LightComponent, COMPONENT_RW_FLAGS_READ, false>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ, false>,
    ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ, false>,

    ComponentDescriptor<EntityTagComponent<EntityTag::STATIC>, COMPONENT_RW_FLAGS_READ, false>
>
{
public:
    SkySystem(EntityManager &entity_manager);
    virtual ~SkySystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;

private:
    void AddRenderSubsystemToEnvironment(World *world, EntityManager &mgr, const Handle<Entity> &entity, SkyComponent &sky_component, MeshComponent *mesh_component);
};

} // namespace hyperion

#endif