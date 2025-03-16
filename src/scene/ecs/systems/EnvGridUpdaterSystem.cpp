/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>
#include <core/logging/Logger.hpp>

#include <system/AppContext.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvGrid);

EnvGridUpdaterSystem::EnvGridUpdaterSystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(NAME("OnWorldChange"), OnWorldChanged.Bind([this](World *new_world, World *previous_world)
    {
        Threads::AssertOnThread(g_game_thread);

        // Trigger removal and addition of render subsystems
        for (auto [entity_id, env_grid_component, bounding_box_component] : GetEntityManager().GetEntitySet<EnvGridComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
            if (env_grid_component.env_grid) {
                env_grid_component.env_grid->RemoveFromEnvironment();
            }

            if (!new_world) {
                env_grid_component.env_grid.Reset();
            }

            if (GetScene()->IsForegroundScene()) {
                AddRenderSubsystemToEnvironment(env_grid_component, bounding_box_component, new_world);
            }
        }
    }));
}

void EnvGridUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    EnvGridComponent &env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);
    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    if (env_grid_component.env_grid) {
        return;
    }

    if (!GetWorld()) {
        return;
    }

    if (GetScene()->IsForegroundScene()) {
        AddRenderSubsystemToEnvironment(env_grid_component, bounding_box_component, GetWorld());
    }
}

void EnvGridUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    EnvGridComponent &env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);

    if (env_grid_component.env_grid) {
        env_grid_component.env_grid->RemoveFromEnvironment();
        env_grid_component.env_grid.Reset();
    }
}

void EnvGridUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, env_grid_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
        if (!env_grid_component.env_grid) {
            continue;
        }
        
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        const BoundingBox &world_aabb = bounding_box_component.world_aabb;

        if (env_grid_component.transform_hash_code != transform_hash_code || env_grid_component.env_grid->GetAABB() != world_aabb) {
            env_grid_component.env_grid->SetCameraData(world_aabb, transform_component.transform.GetTranslation());

            env_grid_component.transform_hash_code = transform_hash_code;
        }

        // Update movable envgrids
        if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA) {
            const Handle<Camera> &camera = GetScene()->GetCamera();

            if (camera.IsValid()) {
                Vec3f translation = transform_component.transform.GetTranslation();

                if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_X) {
                    translation.x = camera->GetTranslation().x;
                }

                if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_Y) {
                    translation.y = camera->GetTranslation().y;
                }

                if (env_grid_component.mobility & EnvGridMobility::FOLLOW_CAMERA_Z) {
                    translation.z = camera->GetTranslation().z;
                }

                if (translation != transform_component.transform.GetTranslation()) {
                    transform_component.transform.SetTranslation(translation);
                }
            }
        }
    }
}

void EnvGridUpdaterSystem::AddRenderSubsystemToEnvironment(EnvGridComponent &env_grid_component, BoundingBoxComponent &bounding_box_component, World *world)
{
    AssertThrow(world != nullptr);
    AssertThrow(world->IsReady());

    if (env_grid_component.env_grid) {
        world->GetRenderResource().GetEnvironment()->AddRenderSubsystem(env_grid_component.env_grid);
    } else {
        EnumFlags<EnvGridFlags> flags = EnvGridFlags::NONE;

        if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.reflections.enabled").ToBool()
            || g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.mode").ToString().ToLower() == "voxel")
        {
            flags |= EnvGridFlags::USE_VOXEL_GRID;
        }
        
        BoundingBox world_aabb = bounding_box_component.world_aabb;

        if (!world_aabb.IsFinite()) {
            world_aabb = BoundingBox::Empty();
        }

        if (!world_aabb.IsValid()) {
            return;
        }

        env_grid_component.env_grid = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<EnvGrid>(
            Name::Unique("env_grid_renderer"),
            GetScene()->HandleFromThis(),
            EnvGridOptions {
                env_grid_component.env_grid_type,
                world_aabb,
                env_grid_component.grid_size,
                flags
            }
        );
    }
}

} // namespace hyperion
