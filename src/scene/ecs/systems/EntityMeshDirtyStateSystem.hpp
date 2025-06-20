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

HYP_CLASS(NoScriptBindings)
class EntityMeshDirtyStateSystem : public SystemBase
{
    HYP_OBJECT_BODY(EntityMeshDirtyStateSystem);

public:
    EntityMeshDirtyStateSystem(EntityManager& entity_manager)
        : SystemBase(entity_manager)
    {
    }

    virtual ~EntityMeshDirtyStateSystem() override = default;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ> {},

            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }
};

} // namespace hyperion

#endif