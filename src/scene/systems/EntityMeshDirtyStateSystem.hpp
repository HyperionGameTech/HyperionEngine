/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/EntityTag.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/RenderProxy.hpp>

namespace hyperion {

HYP_CLASS(NoScriptBindings)
class EntityMeshDirtyStateSystem : public SystemBase
{
    HYP_OBJECT_BODY(EntityMeshDirtyStateSystem);

public:
    EntityMeshDirtyStateSystem(EntityManager& entityManager)
        : SystemBase(entityManager)
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
            ComponentDescriptor<MeshComponent, ComponentRWFlags::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentRWFlags::READ> {},

            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>, ComponentRWFlags::READ, false> {}
        };
    }
};

} // namespace hyperion

