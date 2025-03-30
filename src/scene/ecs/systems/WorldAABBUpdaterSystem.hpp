/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_WORLD_AABB_UPDATER_SYSTEM_HPP
#define HYPERION_ECS_WORLD_AABB_UPDATER_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

namespace hyperion {

class WorldAABBUpdaterSystem : public System<
    WorldAABBUpdaterSystem,

    ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,

    ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_AABB>, COMPONENT_RW_FLAGS_READ, false>
>
{
public:
    WorldAABBUpdaterSystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~WorldAABBUpdaterSystem() override = default;

    virtual EnumFlags<SceneFlags> GetRequiredSceneFlags() const override;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;

private:
    bool ProcessEntity(ID<Entity>, BoundingBoxComponent &bounding_box_component, TransformComponent &transform_component);
};

} // namespace hyperion

#endif