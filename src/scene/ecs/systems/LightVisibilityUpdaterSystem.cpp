/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/LightVisibilityUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <Engine.hpp>

namespace hyperion {

void LightVisibilityUpdaterSystem::OnEntityAdded(ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity);

    GetEntityManager().AddTag<EntityTag::LIGHT>(entity);

    LightComponent &light_component = GetEntityManager().GetComponent<LightComponent>(entity);

    if (!light_component.light) {
        return;
    }

    Handle<Light> light = light_component.light;
    InitObject(light);

    const Transform initial_transform(light->GetPosition());

    // Set initial transform on the TransformComponent
    TransformComponent &transform_component = GetEntityManager().GetComponent<TransformComponent>(entity);
    transform_component.transform = initial_transform;

    const Transform transform = transform_component.transform;

    { // Add a bounding box component to the entity or update if it already exists
        BoundingBoxComponent *bounding_box_component = GetEntityManager().TryGetComponent<BoundingBoxComponent>(entity);

        if (!bounding_box_component) {
            GetEntityManager().AddComponent(entity, BoundingBoxComponent { });

            bounding_box_component = &GetEntityManager().GetComponent<BoundingBoxComponent>(entity);
        }

        switch (light->GetType()) {
        case LightType::DIRECTIONAL:
            bounding_box_component->local_aabb = BoundingBox::Infinity();
            bounding_box_component->world_aabb = BoundingBox::Infinity();
            break;
        case LightType::POINT: // fallthrough
        case LightType::AREA_RECT:
            bounding_box_component->local_aabb = light->GetAABB();
            bounding_box_component->world_aabb = bounding_box_component->local_aabb * transform;
            break;
        default:
            break;
        }
    }

    { // Add a VisibilityStateComponent if it doesn't exist yet
        VisibilityStateComponent *visibility_state_component = GetEntityManager().TryGetComponent<VisibilityStateComponent>(entity);

        if (!visibility_state_component) {
            // Directional light sources are always visible
            if (light->GetType() == LightType::DIRECTIONAL) {
                GetEntityManager().AddComponent(entity, VisibilityStateComponent {
                    VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
                });
            } else {
                GetEntityManager().AddComponent(entity, VisibilityStateComponent { });
            }
        }
    }

    //{ // Check if there is a MeshComponent to get the material from
    //    MeshComponent *mesh_component = GetEntityManager().TryGetComponent<MeshComponent>(entity);

    //    if (mesh_component) {
    //        light->SetMaterial(mesh_component->material);
    //    }
    //    
    //}
}

void LightVisibilityUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    GetEntityManager().RemoveTag<EntityTag::LIGHT>(entity);
}

void LightVisibilityUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    const Handle<Camera> &camera = GetEntityManager().GetScene()->GetCamera();

    for (auto [entity_id, light_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<LightComponent, TransformComponent, BoundingBoxComponent>()) {
        if (!light_component.light) {
            continue;
        }

        if (!light_component.light->IsReady()) {
            continue;
        }

        // For area lights, update the material ID if the entity has a MeshComponent.
        if (light_component.light->GetType() == LightType::AREA_RECT) {
            /*if (MeshComponent *mesh_component = entity_manager.TryGetComponent<MeshComponent>(entity_id)) {
                light_component.light->SetMaterial(mesh_component->material);
            } else {
                light_component.light->SetMaterial(Handle<Material>::empty);
            }*/
        }

        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        if (transform_hash_code != light_component.transform_hash_code) {
            if (light_component.light->GetType() == LightType::DIRECTIONAL) {
                light_component.light->SetPosition(transform_component.transform.GetTranslation().Normalized());

                VisibilityStateComponent *visibility_state_component = GetEntityManager().TryGetComponent<VisibilityStateComponent>(entity_id);

                if (visibility_state_component) {
                    visibility_state_component->flags |= VISIBILITY_STATE_FLAG_INVALIDATED;
                }
            } else {
                light_component.light->SetPosition(transform_component.transform.GetTranslation());
            }

            light_component.transform_hash_code = transform_hash_code;
        }

        bool is_light_in_frustum = false;
        
        if (camera.IsValid()) {
            is_light_in_frustum = camera->GetFrustum().ContainsAABB(bounding_box_component.world_aabb);

            switch (light_component.light->GetType()) {
            case LightType::DIRECTIONAL:
                is_light_in_frustum = true;
                break;
            case LightType::POINT:
                is_light_in_frustum = camera->GetFrustum().ContainsBoundingSphere(light_component.light->GetBoundingSphere());
                break;
            case LightType::SPOT:
                // @TODO Implement frustum culling for spot lights
                break;
            case LightType::AREA_RECT:
                is_light_in_frustum = true;
                break;
            default:
                break;
            }
        }
        
        light_component.light->SetIsVisible(camera.GetID(), is_light_in_frustum);

        if (light_component.light->GetMutationState().IsDirty()) {
            light_component.light->EnqueueRenderUpdates();
        }
    }
}

} // namespace hyperion
