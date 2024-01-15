#include <scene/ecs/systems/EnvGridUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <math/MathUtil.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void EnvGridUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, env_grid_component, transform_component, bounding_box_component] : entity_manager.GetEntitySet<EnvGridComponent, TransformComponent, BoundingBoxComponent>()) {
        if (!env_grid_component.render_component) {
            // @TODO way to get rid of it 
            env_grid_component.render_component = entity_manager.GetScene()->GetEnvironment()->AddRenderComponent<EnvGrid>(
                Name::Unique("env_grid_renderer"),
                env_grid_component.env_grid_type,
                bounding_box_component.world_aabb,
                env_grid_component.grid_size
            );
        }

        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        if (env_grid_component.transform_hash_code != transform_hash_code) {
            env_grid_component.render_component->SetCameraData(transform_component.transform.GetTranslation());

            env_grid_component.transform_hash_code = transform_hash_code;
        }
    }
}

} // namespace hyperion::v2