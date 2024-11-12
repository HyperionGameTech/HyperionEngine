/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderCollection.hpp>

#include <math/MathUtil.hpp>

#include <core/threading/Threads.hpp>
#include <core/logging/Logger.hpp>

#include <core/system/AppContext.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvGrid);

void EnvGridUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    EnvGridComponent &env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);
    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    if (env_grid_component.env_grid) {
        return;
    }

    bool use_voxel_grid = false;

    if (g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.reflections.enabled").ToBool()
        || g_engine->GetAppContext()->GetConfiguration().Get("rendering.env_grid.gi.mode").ToString().ToLower() == "voxel") {
        use_voxel_grid = true;
    }

    BoundingBox world_aabb = bounding_box_component.world_aabb;

    if (!world_aabb.IsFinite()) {
        world_aabb = BoundingBox::Empty();
    }

    if (!world_aabb.IsValid()) {
        HYP_LOG(EnvGrid, LogLevel::WARNING, "EnvGridUpdaterSystem::OnEntityAdded: Entity #{} has invalid bounding box", entity.GetID().Value());
    }

    if (!(GetEntityManager().GetScene()->GetFlags() & (SceneFlags::NON_WORLD | SceneFlags::DETACHED))) {
        HYP_LOG(EnvGrid, LogLevel::DEBUG, "Adding EnvGrid render component to scene {}", GetEntityManager().GetScene()->GetName());

        env_grid_component.env_grid = GetEntityManager().GetScene()->GetEnvironment()->AddRenderComponent<EnvGrid>(
            Name::Unique("env_grid_renderer"),
            EnvGridOptions {
                env_grid_component.env_grid_type,
                world_aabb,
                env_grid_component.grid_size,
                use_voxel_grid
            }
        );
    }
}

void EnvGridUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    EnvGridComponent &env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);

    if (env_grid_component.env_grid != nullptr) {
        GetEntityManager().GetScene()->GetEnvironment()->RemoveRenderComponent<EnvGrid>(env_grid_component.env_grid->GetName());

        env_grid_component.env_grid = nullptr;
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
    }
}

} // namespace hyperion
