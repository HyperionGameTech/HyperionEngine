/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/BLASUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/World.hpp>

#include <rendering/RenderMesh.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RenderCommand.hpp>
#include <rendering/backend/rt/RendererAccelerationStructure.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region Render commands

struct RENDER_COMMAND(UpdateBLASTransform)
    : renderer::RenderCommand
{
    FixedArray<BLASRef, max_frames_in_flight> bottom_level_acceleration_structures;
    Matrix4 transform;

    RENDER_COMMAND(UpdateBLASTransform)(
        const FixedArray<BLASRef, max_frames_in_flight>& bottom_level_acceleration_structures,
        const Matrix4& transform)
        : bottom_level_acceleration_structures(bottom_level_acceleration_structures),
          transform(transform)
    {
    }

    virtual ~RENDER_COMMAND(UpdateBLASTransform)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            if (!bottom_level_acceleration_structures[frame_index].IsValid())
            {
                continue;
            }

            bottom_level_acceleration_structures[frame_index]->SetTransform(transform);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(AddBLASToTLAS)
    : renderer::RenderCommand
{
    TResourceHandle<RenderWorld> render_world;
    FixedArray<BLASRef, max_frames_in_flight> bottom_level_acceleration_structures;

    RENDER_COMMAND(AddBLASToTLAS)(
        const TResourceHandle<RenderWorld>& render_world,
        const FixedArray<BLASRef, max_frames_in_flight>& bottom_level_acceleration_structures)
        : render_world(render_world),
          bottom_level_acceleration_structures(bottom_level_acceleration_structures)
    {
    }

    virtual ~RENDER_COMMAND(AddBLASToTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        RenderEnvironment* environment = render_world->GetEnvironment();
        AssertThrow(environment != nullptr);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            if (bottom_level_acceleration_structures[frame_index].IsValid())
            {
                environment->GetTopLevelAccelerationStructures()[frame_index]->AddBLAS(bottom_level_acceleration_structures[frame_index]);
            }
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveBLASFromTLAS)
    : renderer::RenderCommand
{
    TResourceHandle<RenderWorld> render_world;
    FixedArray<BLASRef, max_frames_in_flight> bottom_level_acceleration_structures;

    RENDER_COMMAND(RemoveBLASFromTLAS)(
        const TResourceHandle<RenderWorld>& render_world,
        const FixedArray<BLASRef, max_frames_in_flight>& bottom_level_acceleration_structures)
        : render_world(render_world),
          bottom_level_acceleration_structures(bottom_level_acceleration_structures)
    {
    }

    virtual ~RENDER_COMMAND(RemoveBLASFromTLAS)() override = default;

    virtual RendererResult operator()() override
    {
        RenderEnvironment* environment = render_world->GetEnvironment();
        AssertThrow(environment != nullptr);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
        {
            if (bottom_level_acceleration_structures[frame_index].IsValid())
            {
                environment->GetTopLevelAccelerationStructures()[frame_index]->RemoveBLAS(bottom_level_acceleration_structures[frame_index]);
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

BLASUpdaterSystem::BLASUpdaterSystem(EntityManager& entity_manager)
    : SystemBase(entity_manager)
{
}

bool BLASUpdaterSystem::ShouldCreateForScene(Scene* scene) const
{
    if (!scene->IsForegroundScene())
    {
        return false;
    }

    if (scene->GetFlags() & SceneFlags::UI)
    {
        return false;
    }

    return true;
}

void BLASUpdaterSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        return;
    }

    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);
    TransformComponent& transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);

    if (!mesh_component.mesh.IsValid() || !mesh_component.material.IsValid())
    {
        return;
    }

    AssertThrow(mesh_component.raytracing_data == nullptr);

    InitObject(mesh_component.mesh);
    AssertThrow(mesh_component.mesh->IsReady());

    InitObject(mesh_component.material);
    AssertThrow(mesh_component.material->IsReady());

    BLASRef blas;

    {
        TResourceHandle<RenderMesh> mesh_resource_handle(mesh_component.mesh->GetRenderResource());

        blas = mesh_resource_handle->BuildBLAS(mesh_component.material);

        if (!blas)
        {
            HYP_LOG(Rendering, Error, "Failed to build BLAS for mesh #{} ({})", mesh_component.mesh.GetID().Value(), mesh_component.mesh->GetName());

            return;
        }
    }

    blas->SetTransform(transform_component.transform.GetMatrix());
    DeferCreate(blas);

    MeshRaytracingData* mesh_raytracing_data = new MeshRaytracingData;

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        mesh_raytracing_data->bottom_level_acceleration_structures[frame_index] = blas;
    }

    mesh_component.raytracing_data = mesh_raytracing_data;

    if (!GetWorld())
    {
        return;
    }

    PUSH_RENDER_COMMAND(AddBLASToTLAS, TResourceHandle<RenderWorld>(GetWorld()->GetRenderResource()), mesh_component.raytracing_data->bottom_level_acceleration_structures);

    GetEntityManager().RemoveTag<EntityTag::UPDATE_BLAS>(entity);
}

void BLASUpdaterSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    MeshComponent& mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (!mesh_component.raytracing_data)
    {
        return;
    }

    PUSH_RENDER_COMMAND(RemoveBLASFromTLAS, TResourceHandle<RenderWorld>(GetWorld()->GetRenderResource()), mesh_component.raytracing_data->bottom_level_acceleration_structures);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        BLASRef& blas = mesh_component.raytracing_data->bottom_level_acceleration_structures[frame_index];

        if (blas.IsValid())
        {
            SafeRelease(std::move(blas));
        }
    }

    delete mesh_component.raytracing_data;
    mesh_component.raytracing_data = nullptr;
}

void BLASUpdaterSystem::Process(float delta)
{
    if (!g_engine->GetAppContext()->GetConfiguration().Get("rendering.rt.enabled").ToBool())
    {
        return;
    }

    HashSet<WeakHandle<Entity>> updated_entities;

    for (auto [entity, mesh_component, transform_component, _] : GetEntityManager().GetEntitySet<MeshComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_BLAS>>().GetScopedView(GetComponentInfos()))
    {
        if (!mesh_component.raytracing_data)
        {
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

        updated_entities.Insert(entity->WeakHandleFromThis());
    }

    if (updated_entities.Any())
    {
        AfterProcess([this, updated_entities = std::move(updated_entities)]()
            {
                for (const WeakHandle<Entity>& entity_weak : updated_entities)
                {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_BLAS>(entity_weak.GetUnsafe());
                }
            });
    }
}

} // namespace hyperion
