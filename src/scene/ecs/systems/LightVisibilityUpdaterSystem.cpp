#include <scene/ecs/systems/LightVisibilityUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void LightVisibilityUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    const Handle<Camera> &camera = entity_manager.GetScene()->GetCamera();

    for (auto [entity_id, light_component, transform_component] : entity_manager.GetEntitySet<LightComponent, TransformComponent>()) {
        if (!light_component.light) {
            continue;
        }

        if (!(light_component.flags & LIGHT_COMPONENT_FLAGS_INIT)) {
            InitObject(light_component.light);

            light_component.flags |= LIGHT_COMPONENT_FLAGS_INIT;

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

        const Bool is_light_in_frustum = light_component.light->GetType() == LightType::DIRECTIONAL
            || (camera.IsValid() && camera->GetFrustum().ContainsBoundingSphere(BoundingSphere(light_component.light->GetPosition(), light_component.light->GetRadius())));

        light_component.light->SetIsVisible(camera.GetID(), is_light_in_frustum);
        light_component.light->Update();
    }
}

} // namespace hyperion::v2