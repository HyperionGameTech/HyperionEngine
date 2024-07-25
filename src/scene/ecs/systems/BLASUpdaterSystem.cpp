/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(UpdateBLASTransform) : renderer::RenderCommand
{
    BLASRef blas;
    Matrix4 transform;

    RENDER_COMMAND(UpdateBLASTransform)(
        const BLASRef &blas,
        const Matrix4 &transform
    ) : blas(blas),
        transform(transform)
    {
    }

    virtual ~RENDER_COMMAND(UpdateBLASTransform)() override = default;

    virtual Result operator()() override
    {
        blas->SetTransform(transform);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(AddBLASToTLAS) : renderer::RenderCommand
{
    TLASRef tlas;
    BLASRef blas;

    RENDER_COMMAND(AddBLASToTLAS)(
        const TLASRef &tlas,
        const BLASRef &blas
    ) : tlas(tlas),
        blas(blas)
    {
    }

    virtual ~RENDER_COMMAND(AddBLASToTLAS)() override = default;

    virtual Result operator()() override
    {
        tlas->AddBLAS(blas);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveBLASFromTLAS) : renderer::RenderCommand
{
    TLASRef tlas;
    BLASRef blas;

    RENDER_COMMAND(RemoveBLASFromTLAS)(
        const TLASRef &tlas,
        const BLASRef &blas
    ) : tlas(tlas),
        blas(blas)
    {
    }

    virtual ~RENDER_COMMAND(RemoveBLASFromTLAS)() override = default;

    virtual Result operator()() override
    {
        tlas->RemoveBLAS(blas);

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

void BLASUpdaterSystem::OnEntityAdded(ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!g_engine->GetConfig().Get(CONFIG_RT_ENABLED).GetBool()) {
        return;
    }

    BLASComponent &blas_component = GetEntityManager().GetComponent<BLASComponent>(entity);
    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);
    TransformComponent &transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);

    if (!mesh_component.mesh.IsValid() || !mesh_component.material.IsValid()) {
        return;
    }

    blas_component.transform_hash_code = transform_component.transform.GetHashCode();

    blas_component.blas = MakeRenderObject<BLAS>(transform_component.transform.GetMatrix());

    AccelerationGeometryRef geometry = MakeRenderObject<AccelerationGeometry>(
        mesh_component.mesh->BuildPackedVertices(),
        mesh_component.mesh->BuildPackedIndices(),
        entity.ToIndex(),
        mesh_component.material.GetID().ToIndex()
    );

    DeferCreate(geometry, g_engine->GetGPUDevice(), g_engine->GetGPUInstance());

    blas_component.blas->AddGeometry(std::move(geometry));

    DeferCreate(blas_component.blas, g_engine->GetGPUDevice(), g_engine->GetGPUInstance());
    
    if (const TLASRef &tlas = GetEntityManager().GetScene()->GetTLAS(); tlas.IsValid()) {
        PUSH_RENDER_COMMAND(AddBLASToTLAS, tlas, blas_component.blas);
    }
}

void BLASUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (!g_engine->GetConfig().Get(CONFIG_RT_ENABLED).GetBool()) {
        return;
    }

    BLASComponent &blas_component = GetEntityManager().GetComponent<BLASComponent>(entity);

    if (blas_component.blas.IsValid()) {
        if (const TLASRef &tlas = GetEntityManager().GetScene()->GetTLAS(); tlas.IsValid()) {
            PUSH_RENDER_COMMAND(RemoveBLASFromTLAS, tlas, blas_component.blas);
        }

        SafeRelease(std::move(blas_component.blas));
    }
}

void BLASUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    if (!g_engine->GetConfig().Get(CONFIG_RT_ENABLED).GetBool()) {
        return;
    }

    for (auto [entity_id, blas_component, mesh_component, transform_component] : GetEntityManager().GetEntitySet<BLASComponent, MeshComponent, TransformComponent>().GetScopedView(GetComponentInfos())) {
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        if (!blas_component.blas.IsValid()) {
            continue;
        }

        /// vvv FIXME
        // if (blas_component.blas->GetMesh() != mesh_component.mesh) {
        //     blas_component.blas->SetMesh(mesh_component.mesh);
        // }

        // if (blas_component.blas->GetMaterial() != mesh_component.material) {
        //     blas_component.blas->SetMaterial(mesh_component.material);
        // }

        if (transform_hash_code != blas_component.transform_hash_code) {
            PUSH_RENDER_COMMAND(UpdateBLASTransform, blas_component.blas, transform_component.transform.GetMatrix());

            blas_component.transform_hash_code = transform_hash_code;
        }
    }
}

} // namespace hyperion
