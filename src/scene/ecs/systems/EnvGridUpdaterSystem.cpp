#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <math/MathUtil.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void EnvGridUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    EnvGridComponent &env_grid_component = entity_manager.GetComponent<EnvGridComponent>(entity);
    BoundingBoxComponent &bounding_box_component = entity_manager.GetComponent<BoundingBoxComponent>(entity);

    if (env_grid_component.render_component) {
        return;
    }

    bool use_voxel_grid = false;

    if (g_engine->GetConfig().Get(CONFIG_ENV_GRID_REFLECTIONS).GetBool()) {
        use_voxel_grid = true;
    }

    BoundingBox world_aabb = bounding_box_component.world_aabb;

    if (!world_aabb.IsFinite()) {
        world_aabb = BoundingBox::empty;
    }

    if (!world_aabb.IsValid()) {
        DebugLog(LogType::Warn, "EnvGridUpdaterSystem::OnEntityAdded: Entity #%u has invalid bounding box\n", entity.Value());
    }

    env_grid_component.render_component = entity_manager.GetScene()->GetEnvironment()->AddRenderComponent<EnvGrid>(
        Name::Unique("env_grid_renderer"),
        EnvGridOptions {
            env_grid_component.env_grid_type,
            world_aabb,
            env_grid_component.grid_size,
            use_voxel_grid
        }
    );
}

void EnvGridUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    EnvGridComponent &env_grid_component = entity_manager.GetComponent<EnvGridComponent>(entity);

    if (env_grid_component.render_component) {
        entity_manager.GetScene()->GetEnvironment()->RemoveRenderComponent<EnvGrid>(env_grid_component.render_component->GetName());
        env_grid_component.render_component = nullptr;
    }
}

void EnvGridUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, env_grid_component, transform_component, bounding_box_component] : entity_manager.GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent>()) {
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        const BoundingBox &world_aabb = bounding_box_component.world_aabb;

        if (env_grid_component.transform_hash_code != transform_hash_code || env_grid_component.render_component->GetAABB() != world_aabb) {
            env_grid_component.render_component->SetCameraData(world_aabb, transform_component.transform.GetTranslation());

            env_grid_component.transform_hash_code = transform_hash_code;
        }
    }
}

} // namespace hyperion::v2
