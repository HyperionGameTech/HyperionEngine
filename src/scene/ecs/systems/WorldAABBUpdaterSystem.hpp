/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_WORLD_AABB_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_WORLD_AABB_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class WorldAABBUpdaterSystem : public SystemBase
{
    HYP_OBJECT_BODY(WorldAABBUpdaterSystem);

public:
    WorldAABBUpdaterSystem(EntityManager& entity_manager)
        : SystemBase(entity_manager)
    {
    }

    virtual ~WorldAABBUpdaterSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ> {},

            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_AABB>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }

    bool ProcessEntity(Entity* entity, BoundingBoxComponent& bounding_box_component, TransformComponent& transform_component);
};

} // namespace hyperion

#endif