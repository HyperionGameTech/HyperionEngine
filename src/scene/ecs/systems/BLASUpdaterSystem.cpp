/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/World.hpp>

#include <rendering/RenderMesh.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <core/containers/HashSet.hpp>

#include <system/AppContext.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(UpdateBLASTransform) : renderer::RenderCommand
{
    FixedArray<BLASRef, max_frames_in_flight>   bottom_level_acceleration_structures;
    Matrix4                                     transform;

    RENDER_COMMAND(UpdateBLASTransform)(
        const FixedArray<BLASRef, max_frames_in_flight> &bottom_level_acceleration_structures,
        const Matrix4 &transform
    ) : bottom_level_acceleration_structures(bottom_level_acceleration_structures),
        transform(transform)
    {
    }

    virtual ~RENDER_COMMAND(UpdateBLASTransform)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            if (!bottom_level_acceleration_structures[frame_index].IsValid()) {
                continue;
            }

            bottom_level_acceleration_structures[frame_index]->SetTransform(transform);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(AddBLASToTLAS) : renderer::RenderCommand
{
    WeakHandle<RenderEnvironment>               environment_weak;
    FixedArray<BLASRef, max_frames_in_flight>   bottom_level_acceleration_structures;

    RENDER_COMMAND(AddBLASToTLAS)(
        const WeakHandle<RenderEnvironment> &environment_weak,
        const FixedArray<BLASRef, max_frames_in_flight> &bottom_level_acceleration_structures
    ) : environment_weak(environment_weak),
        bottom_level_acceleration_structures(bottom_level_acceleration_structures)
    {
    }

    virtual ~RENDER_COMMAND(AddBLASToTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        if (Handle<RenderEnvironment> environment = environment_weak.Lock()) {
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                if (bottom_level_acceleration_structures[frame_index].IsValid()) {
                    environment->GetTopLevelAccelerationStructures()[frame_index]->AddBLAS(bottom_level_acceleration_structures[frame_index]);
                }
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveBLASFromTLAS) : renderer::RenderCommand
{
    WeakHandle<RenderEnvironment>               environment_weak;
    FixedArray<BLASRef, max_frames_in_flight>   bottom_level_acceleration_structures;

    RENDER_COMMAND(RemoveBLASFromTLAS)(
        const WeakHandle<RenderEnvironment> &environment_weak,
        const FixedArray<BLASRef, max_frames_in_flight> &bottom_level_acceleration_structures
    ) : environment_weak(environment_weak),
        bottom_level_acceleration_structures(bottom_level_acceleration_structures)
    {
    }

    virtual ~RENDER_COMMAND(RemoveBLASFromTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        if (Handle<RenderEnvironment> environment = environment_weak.Lock()) {
            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                if (bottom_level_acceleration_structures[frame_index].IsValid()) {
                    environment->GetTopLevelAccelerationStructures()[frame_index]->RemoveBLAS(bottom_level_acceleration_structures[frame_index]);
                }
            }
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

        for (auto [entity_id, mesh_component] : GetEntityManager().GetEntitySet<MeshComponent>().GetScopedView(GetComponentInfos())) {
            if (previous_world != nullptr && mesh_component.raytracing_data != nullptr) {
                PUSH_RENDER_COMMAND(RemoveBLASFromTLAS, previous_world->GetRenderResource().GetEnvironment().ToWeak(), mesh_component.raytracing_data->bottom_level_acceleration_structures);
            }

            if (new_world != nullptr && mesh_component.raytracing_data != nullptr) {
                PUSH_RENDER_COMMAND(AddBLASToTLAS, new_world->GetRenderResource().GetEnvironment().ToWeak(), mesh_component.raytracing_data->bottom_level_acceleration_structures);
            }
        }
    }));
}

EnumFlags<SceneFlags> BLASUpdaterSystem::GetRequiredSceneFlags() const
{
    return SceneFlags::FOREGROUND;
}

void BLASUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool()) {
        return;
    }

    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);
    TransformComponent &transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);

    if (!mesh_component.mesh.IsValid() || !mesh_component.material.IsValid()) {
        return;
    }

    AssertThrow(mesh_component.raytracing_data == nullptr);

    InitObject(mesh_component.mesh);
    AssertThrow(mesh_component.mesh->IsReady());

    AccelerationGeometryRef geometry;
    
    {
        TResourceHandle<MeshRenderResource> mesh_resource_handle(mesh_component.mesh->GetRenderResource());

        geometry = MakeRenderObject<AccelerationGeometry>(
            mesh_resource_handle->BuildPackedVertices(),
            mesh_resource_handle->BuildPackedIndices(),
            entity,
            mesh_component.material
        );

        DeferCreate(geometry, g_engine->GetGPUDevice(), g_engine->GetGPUInstance());
    }

    MeshRaytracingData *mesh_raytracing_data = new MeshRaytracingData;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        BLASRef blas = MakeRenderObject<BLAS>(transform_component.transform.GetMatrix());

        blas->AddGeometry(geometry);

        DeferCreate(blas, g_engine->GetGPUDevice(), g_engine->GetGPUInstance());

        mesh_raytracing_data->bottom_level_acceleration_structures[frame_index] = std::move(blas);
    }

    mesh_component.raytracing_data = mesh_raytracing_data;
    
    if (!GetWorld()) {
        return;
    }

    PUSH_RENDER_COMMAND(AddBLASToTLAS, GetWorld()->GetRenderResource().GetEnvironment().ToWeak(), mesh_component.raytracing_data->bottom_level_acceleration_structures);

    GetEntityManager().RemoveTag<EntityTag::UPDATE_BLAS>(entity);
}

void BLASUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (!mesh_component.raytracing_data) {
        return;
    }

    PUSH_RENDER_COMMAND(RemoveBLASFromTLAS, GetWorld()->GetRenderResource().GetEnvironment().ToWeak(), mesh_component.raytracing_data->bottom_level_acceleration_structures);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        BLASRef &blas = mesh_component.raytracing_data->bottom_level_acceleration_structures[frame_index];

        if (blas.IsValid()) {
            SafeRelease(std::move(blas));
        }
    }

    delete mesh_component.raytracing_data;
    mesh_component.raytracing_data = nullptr;
}

void BLASUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool()) {
        return;
    }

    HashSet<ID<Entity>> updated_entity_ids;

    for (auto [entity_id, mesh_component, transform_component, _] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_BLAS>>().GetScopedView(GetComponentInfos())) {
        if (!mesh_component.raytracing_data) {
            continue;
        }


        /// vvv FIXME
        // if (blas_component.blas->GetMesh() != mesh_component.mesh) {
        //     blas_component.blas->SetMesh(mesh_component.mesh);
        // }

        // if (blas_component.blas->GetMaterial() != mesh_component.material) {
        //     blas_component.blas->SetMaterial(mesh_component.material);
        // }

        PUSH_RENDER_COMMAND(UpdateBLASTransform, mesh_component.raytracing_data->bottom_level_acceleration_structures, transform_component.transform.GetMatrix());

        updated_entity_ids.Insert(entity_id);
    }

    if (updated_entity_ids.Any()) {
        AfterProcess([this, entity_ids = std::move(updated_entity_ids)]()
        {
            for (const ID<Entity> &entity_id : entity_ids) {
                GetEntityManager().RemoveTag<EntityTag::UPDATE_BLAS>(entity_id);
            }
        });
    }
}

} // namespace hyperion
