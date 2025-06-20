/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_LIGHT_VISIBILITY_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_LIGHT_VISIBILITY_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/CameraComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>

#include <rendering/RenderProxy.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class LightVisibilityUpdaterSystem : public SystemBase
{
    HYP_OBJECT_BODY(LightVisibilityUpdaterSystem);

public:
    LightVisibilityUpdaterSystem(EntityManager& entity_manager)
        : SystemBase(entity_manager)
    {
    }

    virtual ~LightVisibilityUpdaterSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<LightComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ_WRITE, false> {},
            ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ_WRITE, false> {},
            ComponentDescriptor<VisibilityStateComponent, COMPONENT_RW_FLAGS_READ_WRITE, false> {},

            // Can read and write the MeshComponent but does not receive events (updates material render data for area lights)
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE, false> {},
            ComponentDescriptor<CameraComponent, COMPONENT_RW_FLAGS_READ, false> {},

            // Note: EntityTag::LIGHT is only added/removed from OnEntityAdded/OnEntityRemoved, so we don't need to add it here.

            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_LIGHT_TRANSFORM>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }
};

} // namespace hyperion

#endif