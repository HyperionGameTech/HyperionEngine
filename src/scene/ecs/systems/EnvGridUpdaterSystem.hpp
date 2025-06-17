/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENV_GRID_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_ENV_GRID_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/components/EnvGridComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightmapVolumeComponent.hpp>
#include <scene/ecs/EntityTag.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class EnvGridUpdaterSystem : public SystemBase
{
    HYP_OBJECT_BODY(EnvGridUpdaterSystem);

public:
    EnvGridUpdaterSystem(EntityManager& entity_manager);
    virtual ~EnvGridUpdaterSystem() override = default;

    virtual bool RequiresGameThread() const override
    {
        return true;
    }

    virtual void OnEntityAdded(const Handle<Entity>& entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<EnvGridComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ> {},
            ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ> {},

            // Calling EnvGrid::Update() calls View::Update() which reads the the following components.
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<LightComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<VisibilityStateComponent, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<LightmapVolumeComponent, COMPONENT_RW_FLAGS_READ, false> {},

            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_ENV_GRID_TRANSFORM>, COMPONENT_RW_FLAGS_READ, false> {},
            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_ENV_GRID>, COMPONENT_RW_FLAGS_READ, false> {},

            // EnvGrid::Update() collects static entities
            ComponentDescriptor<EntityTagComponent<EntityTag::STATIC>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }

    void UpdateEnvGrid(GameCounter::TickUnit delta, EnvGridComponent& env_grid_component, BoundingBoxComponent& bounding_box_component);
};

} // namespace hyperion

#endif