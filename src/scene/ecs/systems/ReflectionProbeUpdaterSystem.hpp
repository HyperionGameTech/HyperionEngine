/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_REFLECTION_PROBE_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_REFLECTION_PROBE_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>
#include <scene/ecs/components/ReflectionProbeComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/LightmapVolumeComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class ReflectionProbeUpdaterSystem : public SystemBase
{
    HYP_OBJECT_BODY(ReflectionProbeUpdaterSystem);

public:
    ReflectionProbeUpdaterSystem(EntityManager& entity_manager);
    virtual ~ReflectionProbeUpdaterSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<ReflectionProbeComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ> {},
            ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ> {},

            // calling EnvProbe::Update() calls View::Update() which reads the following of entities.
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<LightComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<VisibilityStateComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<LightmapVolumeComponent, COMPONENT_RW_FLAGS_READ, false> {},

            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_ENV_PROBE_TRANSFORM>, COMPONENT_RW_FLAGS_READ, false> {},

            // EnvProbe::Update() collects static entities
            ComponentDescriptor<EntityTagComponent<EntityTag::STATIC>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }

    void AddRenderSubsystemToEnvironment(ReflectionProbeComponent& reflection_probe_component);
};

} // namespace hyperion

#endif