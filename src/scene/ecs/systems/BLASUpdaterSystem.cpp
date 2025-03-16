/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>

#include <rendering/RenderMesh.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <system/AppContext.hpp>

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

    virtual RendererResult operator()() override
    {
        blas->SetTransform(transform);

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(AddBLASToTLAS) : renderer::RenderCommand
{
    WeakHandle<RenderEnvironment>   environment_weak;
    BLASRef                         blas;

    RENDER_COMMAND(AddBLASToTLAS)(
        const WeakHandle<RenderEnvironment> &environment_weak,
        const BLASRef &blas
    ) : environment_weak(environment_weak),
        blas(blas)
    {
        AssertThrow(blas.IsValid());
    }

    virtual ~RENDER_COMMAND(AddBLASToTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        if (Handle<RenderEnvironment> environment = environment_weak.Lock()) {
            AssertThrow(environment->GetTLAS().IsValid());

            environment->GetTLAS()->AddBLAS(blas);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveBLASFromTLAS) : renderer::RenderCommand
{
    WeakHandle<RenderEnvironment>   environment_weak;
    BLASRef                         blas;

    RENDER_COMMAND(RemoveBLASFromTLAS)(
        const WeakHandle<RenderEnvironment> &environment_weak,
        const BLASRef &blas
    ) : environment_weak(environment_weak),
        blas(blas)
    {
        AssertThrow(blas.IsValid());
    }

    virtual ~RENDER_COMMAND(RemoveBLASFromTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        if (Handle<RenderEnvironment> environment = environment_weak.Lock()) {
            AssertThrow(environment->GetTLAS().IsValid());

            environment->GetTLAS()->RemoveBLAS(blas);
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

BLASUpdaterSystem::BLASUpdaterSystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(NAME("OnWorldChange"), OnWorldChanged.Bind([this](World *new_world, World *previous_world)
    {
        Threads::AssertOnThread(g_game_thread);

        for (auto [entity_id, blas_component] : GetEntityManager().GetEntitySet<BLASComponent>().GetScopedView(GetComponentInfos())) {
            if (previous_world != nullptr && blas_component.blas != nullptr) {
                PUSH_RENDER_COMMAND(RemoveBLASFromTLAS, previous_world->GetRenderResource().GetEnvironment(), blas_component.blas);
            }

            if (new_world != nullptr && blas_component.blas != nullptr) {
                PUSH_RENDER_COMMAND(AddBLASToTLAS, new_world->GetRenderResource().GetEnvironment(), blas_component.blas);
            }
        }
    }));
}

void BLASUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool()) {
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

    InitObject(mesh_component.mesh);
    AssertThrow(mesh_component.mesh->IsReady());

    AccelerationGeometryRef geometry = MakeRenderObject<AccelerationGeometry>(
        mesh_component.mesh->GetRenderResource().BuildPackedVertices(),
        mesh_component.mesh->GetRenderResource().BuildPackedIndices(),
        entity,
        mesh_component.material
    );

    DeferCreate(geometry, g_engine->GetGPUDevice(), g_engine->GetGPUInstance());

    blas_component.blas->AddGeometry(std::move(geometry));

    DeferCreate(blas_component.blas, g_engine->GetGPUDevice(), g_engine->GetGPUInstance());
    
    if (!GetWorld()) {
        return;
    }

    PUSH_RENDER_COMMAND(AddBLASToTLAS, GetWorld()->GetRenderResource().GetEnvironment(), blas_component.blas);
}

void BLASUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool()) {
        return;
    }

    BLASComponent &blas_component = GetEntityManager().GetComponent<BLASComponent>(entity);

    if (blas_component.blas.IsValid()) {
        PUSH_RENDER_COMMAND(RemoveBLASFromTLAS, GetWorld()->GetRenderResource().GetEnvironment(), blas_component.blas);

        SafeRelease(std::move(blas_component.blas));
    }
}

void BLASUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool()) {
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
