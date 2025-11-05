/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/systems/EntityMeshDirtyStateSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/BVH.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <EntityMeshDirtyStateSystem.generated.inl>

namespace hyperion {

void EntityMeshDirtyStateSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (!meshComponent.mesh || !meshComponent.material)
    {
        HYP_LOG(Mesh, Warning, "Entity {} (name: {}) has a MeshComponent with an invalid mesh or material", entity->Id(), entity->GetName());
        HYP_BREAKPOINT_DEBUG_MODE;
    }

    InitObject(meshComponent.mesh);
    InitObject(meshComponent.material);

    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
}

void EntityMeshDirtyStateSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);
}

void EntityMeshDirtyStateSystem::Process(float delta)
{
}

} // namespace hyperion
