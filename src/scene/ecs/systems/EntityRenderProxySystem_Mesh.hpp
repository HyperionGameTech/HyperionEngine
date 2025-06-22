/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_RENDER_PROXY_SYSTEM_MESH_HPP
#define HYPERION_ECS_ENTITY_RENDER_PROXY_SYSTEM_MESH_HPP

#include <scene/ecs/System.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>

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
    EntityRenderProxySystem_Mesh(EntityManager& entity_manager)
        : SystemBase(entity_manager)
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
            ComponentDescriptor<MeshComponent, COMPONENT_RW_FLAGS_READ_WRITE> {},
            ComponentDescriptor<TransformComponent, COMPONENT_RW_FLAGS_READ> {},
            ComponentDescriptor<BoundingBoxComponent, COMPONENT_RW_FLAGS_READ> {},
            ComponentDescriptor<EntityTagComponent<EntityTag::UPDATE_RENDER_PROXY>, COMPONENT_RW_FLAGS_READ, false> {}
        };
    }
};

} // namespace hyperion

#endif