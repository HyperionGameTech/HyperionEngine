/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SHADOW_MAP_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_SHADOW_MAP_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class ShadowMapUpdaterSystem : public SystemBase
{
    HYP_OBJECT_BODY(ShadowMapUpdaterSystem);

public:
    ShadowMapUpdaterSystem(EntityManager& entity_manager);
    virtual ~ShadowMapUpdaterSystem() override = default;

    virtual void OnEntityAdded(const Handle<Entity>& entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<ShadowMapComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<LightComponent, COMPONENT_RW_FLAGS_READ> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ> {},
            ComponentDescriptor<VisibilityStateComponent, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }

    void AddRenderSubsystemToEnvironment(ShadowMapComponent& shadow_map_component, LightComponent& light_component);
};

} // namespace hyperion

#endif