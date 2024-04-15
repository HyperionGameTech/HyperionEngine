/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void BLASUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    if (!g_engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
        return;
    }

    BLASComponent &blas_component = entity_manager.GetComponent<BLASComponent>(entity);
    MeshComponent &mesh_component = entity_manager.GetComponent<MeshComponent>(entity);
    TransformComponent &transform_component = entity_manager.GetComponent<TransformComponent>(entity);

    if (!mesh_component.mesh.IsValid() || !mesh_component.material.IsValid()) {
        return;
    }

    blas_component.transform_hash_code = transform_component.transform.GetHashCode();

    blas_component.blas = CreateObject<BLAS>(
        entity,
        mesh_component.mesh,
        mesh_component.material,
        transform_component.transform
    );

    if (InitObject(blas_component.blas)) {
        if (const Handle<TLAS> &tlas = entity_manager.GetScene()->GetTLAS(); tlas.IsValid()) {
            tlas->AddBLAS(blas_component.blas);
        }
    }
}

void BLASUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    if (!g_engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
        return;
    }

    BLASComponent &blas_component = entity_manager.GetComponent<BLASComponent>(entity);

    if (blas_component.blas.IsValid()) {
        if (const Handle<TLAS> &tlas = entity_manager.GetScene()->GetTLAS(); tlas.IsValid()) {
            tlas->RemoveBLAS(blas_component.blas.GetID());
        }

        blas_component.blas.Reset();
    }
}

void BLASUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    if (!g_engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
        return;
    }

    for (auto [entity_id, blas_component, mesh_component, transform_component] : entity_manager.GetEntitySet<BLASComponent, MeshComponent, TransformComponent>()) {
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        if (!blas_component.blas.IsValid()) {
            continue;
        }

        if (blas_component.blas->GetMesh() != mesh_component.mesh) {
            blas_component.blas->SetMesh(mesh_component.mesh);
        }

        if (blas_component.blas->GetMaterial() != mesh_component.material) {
            blas_component.blas->SetMaterial(mesh_component.material);
        }

        if (transform_hash_code != blas_component.transform_hash_code) {
            blas_component.blas->SetTransform(transform_component.transform);

            blas_component.transform_hash_code = transform_hash_code;
        }
    }
}

} // namespace hyperion::v2
