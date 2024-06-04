/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderCollection.hpp>

#include <math/MathUtil.hpp>

#include <core/threading/Threads.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvGrid);

void EnvGridUpdaterSystem::OnEntityAdded(ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity);

    EnvGridComponent &env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);
    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    if (env_grid_component.render_component) {
        return;
    }

    bool use_voxel_grid = false;

    if (g_engine->GetConfig().Get(CONFIG_ENV_GRID_REFLECTIONS).GetBool()) {
        use_voxel_grid = true;
    }

    BoundingBox world_aabb = bounding_box_component.world_aabb;

    if (!world_aabb.IsFinite()) {
        world_aabb = BoundingBox::Empty();
    }

    if (!world_aabb.IsValid()) {
        HYP_LOG(EnvGrid, LogLevel::WARNING, "EnvGridUpdaterSystem::OnEntityAdded: Entity #{} has invalid bounding box", entity.Value());
    }

    env_grid_component.render_component = GetEntityManager().GetScene()->GetEnvironment()->AddRenderComponent<EnvGrid>(
        Name::Unique("env_grid_renderer"),
        EnvGridOptions {
            env_grid_component.env_grid_type,
            world_aabb,
            env_grid_component.grid_size,
            use_voxel_grid
        }
    );
}

void EnvGridUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    EnvGridComponent &env_grid_component = GetEntityManager().GetComponent<EnvGridComponent>(entity);

    if (env_grid_component.render_component) {
        GetEntityManager().GetScene()->GetEnvironment()->RemoveRenderComponent<EnvGrid>(env_grid_component.render_component->GetName());
        env_grid_component.render_component = nullptr;
    }
}

void EnvGridUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, env_grid_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent>()) {
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        const BoundingBox &world_aabb = bounding_box_component.world_aabb;

        if (env_grid_component.transform_hash_code != transform_hash_code || env_grid_component.render_component->GetAABB() != world_aabb) {
            env_grid_component.render_component->SetCameraData(world_aabb, transform_component.transform.GetTranslation());

            env_grid_component.transform_hash_code = transform_hash_code;
        }
    }
}

} // namespace hyperion
