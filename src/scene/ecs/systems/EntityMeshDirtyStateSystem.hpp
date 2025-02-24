/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_MESH_DIRTY_STATE_SYSTEM_HPP
#define HYPERION_ECS_ENTITY_MESH_DIRTY_STATE_SYSTEM_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

#include <rendering/RenderProxy.hpp>

namespace hyperion {

class EntityMeshDirtyStateSystem : public System<
    EntityMeshDirtyStateSystem,
    ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE>,
    ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ>,

    ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>, COMPONENT_RW_FLAGS_READ_WRITE, false>
>
{
public:
    EntityMeshDirtyStateSystem(EntityManager &entity_manager)
        : System(entity_manager)
    {
    }

    virtual ~EntityMeshDirtyStateSystem() override = default;

    virtual EnumFlags<SceneFlags> GetRequiredSceneFlags() const override;

    virtual void OnEntityAdded(const Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(ID<Entity> entity) override;

    virtual void Process(GameCounter::TickUnit delta) override;
};

} // namespace hyperion

#endif