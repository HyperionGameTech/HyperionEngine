#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void BLASUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    if (!g_engine->GetConfig().Get(CONFIG_RT_ENABLED)) {
        return;
    }

    for (auto [entity_id, blas_component, mesh_component, transform_component] : entity_manager.GetEntitySet<BLASComponent, MeshComponent, TransformComponent>()) {
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();
        
        if (!blas_component.blas.IsValid()) {
            if (!mesh_component.mesh.IsValid() || !mesh_component.material.IsValid()) {
                continue;
            }

            blas_component.transform_hash_code = transform_hash_code;

            blas_component.blas = CreateObject<BLAS>(
                entity_id,
                mesh_component.mesh,
                mesh_component.material,
                transform_component.transform
            );

            if (InitObject(blas_component.blas)) {
                if (const Handle<TLAS> &tlas = entity_manager.GetScene()->GetTLAS(); tlas.IsValid()) {
                    // @TODO: Way to remove BLAS from TLAS.
                    tlas->AddBLAS(blas_component.blas);
                }
            }

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