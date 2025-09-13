/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/System.hpp>
#include <scene/EntityTag.hpp>

#include <scene/components/MeshComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/RenderProxy.hpp>

namespace hyperion {

/*! \brief System that updates the render proxy for entities with MeshComponent.
 *  This system processes entities with MeshComponent and updates their render proxies
 *  based on the mesh, material, and other properties defined in the MeshComponent.
 */
HYP_CLASS(NoScriptBindings)
class EntityRenderProxySystem_Mesh : public SystemBase
{
    HYP_OBJECT_BODY(EntityRenderProxySystem_Mesh);

public:
    EntityRenderProxySystem_Mesh(EntityManager& entityManager)
        : SystemBase(entityManager)
    {
    }

    virtual ~EntityRenderProxySystem_Mesh() override = default;

    virtual void OnEntityAdded(Entity* entity) override;
    virtual void OnEntityRemoved(Entity* entity) override;

    virtual void Process(float delta) override;

private:
    virtual SystemComponentDescriptors GetComponentDescriptors() const override
    {
        return {
            ComponentDescriptor<MeshComponent, ComponentRWFlags::READ_WRITE> {},
            ComponentDescriptor<TransformComponent, ComponentRWFlags::READ> {},
            ComponentDescriptor<BoundingBoxComponent, ComponentRWFlags::READ> {},
            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>, ComponentRWFlags::READ, false> {}
        };
    }
};

} // namespace hyperion

