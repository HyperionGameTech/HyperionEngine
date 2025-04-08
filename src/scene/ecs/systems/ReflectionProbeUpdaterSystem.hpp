/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_REFLECTION_PROBE_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_REFLECTION_PROBE_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>
#include <scene/ecs/components/ReflectionProbeComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

namespace hyperion {

class ReflectionProbeUpdaterSystem : public System<
    ReflectionProbeUpdaterSystem,
    ComponentDescriptor<ReflectionProbeComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,
    ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ>,

    ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_ENV_PROBE_TRANSFORM>, COMPONENT_RW_FLAGS_READ, false>
>
{
public:
    ReflectionProbeUpdaterSystem(EntityManager &entity_manager);
    virtual ~ReflectionProbeUpdaterSystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;

private:
    void AddRenderSubsystemToEnvironment(ReflectionProbeComponent &reflection_probe_component, World *world);
};

} // namespace hyperion

#endif