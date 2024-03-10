#include <scene/ecs/systems/LightVisibilityUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void LightVisibilityUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    LightComponent &light_component = entity_manager.GetComponent<LightComponent>(entity);

    if (!light_component.light) {
        return;
    }

    Handle<Light> light = light_component.light;
    InitObject(light);

    const Transform initial_transform(light->GetPosition());

    // Set initial transform on the TransformComponent
    TransformComponent &transform_component = entity_manager.GetComponent<TransformComponent>(entity);
    transform_component.transform = initial_transform;

    { // Add a bounding box component to the entity or update if it already exists
        BoundingBoxComponent *bounding_box_component = entity_manager.TryGetComponent<BoundingBoxComponent>(entity);

        if (!bounding_box_component) {
            entity_manager.AddComponent<BoundingBoxComponent>(entity, { });

            bounding_box_component = &entity_manager.GetComponent<BoundingBoxComponent>(entity);
        }

        const BoundingBox local_aabb = BoundingBox(BoundingSphere(light->GetPosition(), light->GetRadius()));

        bounding_box_component->local_aabb = local_aabb;
        bounding_box_component->world_aabb = local_aabb * transform_component.transform;
    }

    { // Add a visibility state component if it doesn't exist yet
        VisibilityStateComponent *visibility_state_component = entity_manager.TryGetComponent<VisibilityStateComponent>(entity);

        if (!visibility_state_component) {
            entity_manager.AddComponent<VisibilityStateComponent>(entity, { });
        }
    }
}

void LightVisibilityUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
}

void LightVisibilityUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    const Handle<Camera> &camera = entity_manager.GetScene()->GetCamera();

    for (auto [entity_id, light_component, transform_component] : entity_manager.GetEntitySet<LightComponent, TransformComponent>()) {
        if (!light_component.light) {
            continue;
        }
        
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        if (transform_hash_code != light_component.transform_hash_code) {
            if (light_component.light->GetType() == LightType::DIRECTIONAL) {
                light_component.light->SetPosition(transform_component.transform.GetTranslation().Normalized());
            } else {
                light_component.light->SetPosition(transform_component.transform.GetTranslation());
            }

            light_component.transform_hash_code = transform_hash_code;
        }

        const bool is_light_in_frustum = light_component.light->GetType() == LightType::DIRECTIONAL
            || (camera.IsValid() && camera->GetFrustum().ContainsBoundingSphere(BoundingSphere(light_component.light->GetPosition(), light_component.light->GetRadius())));

        light_component.light->SetIsVisible(camera.GetID(), is_light_in_frustum);
        light_component.light->Update();
    }
}

} // namespace hyperion::v2