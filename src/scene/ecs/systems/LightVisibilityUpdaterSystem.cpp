/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/LightVisibilityUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/Light.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

namespace hyperion {

void LightVisibilityUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
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
    TransformComponent *transform_component = GetEntityManager().TryGetComponent<TransformComponent>(entity);

    if (!transform_component) {
        GetEntityManager().AddComponent<TransformComponent>(entity, TransformComponent { initial_transform });
        transform_component = &GetEntityManager().GetComponent<TransformComponent>(entity);
    }

    transform_component->transform = initial_transform;

    const Transform transform = transform_component->transform;

    { // Add a bounding box component to the entity or update if it already exists
        BoundingBoxComponent *bounding_box_component = GetEntityManager().TryGetComponent<BoundingBoxComponent>(entity);

        if (!bounding_box_component) {
            GetEntityManager().AddComponent<BoundingBoxComponent>(entity, BoundingBoxComponent { });
            bounding_box_component = &GetEntityManager().GetComponent<BoundingBoxComponent>(entity);
        }

        switch (light->GetLightType()) {
        case LightType::DIRECTIONAL:
            bounding_box_component->local_aabb = BoundingBox::Infinity();
            bounding_box_component->world_aabb = BoundingBox::Infinity();
            break;
        case LightType::POINT: // fallthrough
        case LightType::AREA_RECT:
            bounding_box_component->local_aabb = light->GetAABB();
            bounding_box_component->world_aabb = transform * bounding_box_component->local_aabb;
            break;
        default:
            break;
        }
    }

    { // Add a VisibilityStateComponent if it doesn't exist yet
        VisibilityStateComponent *visibility_state_component = GetEntityManager().TryGetComponent<VisibilityStateComponent>(entity);

        if (!visibility_state_component) {
            // Directional light sources are always visible
            if (light->GetLightType() == LightType::DIRECTIONAL) {
                GetEntityManager().AddComponent<VisibilityStateComponent>(entity, VisibilityStateComponent {
                    VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE
                });
            } else {
                GetEntityManager().AddComponent<VisibilityStateComponent>(entity, VisibilityStateComponent { });
            }
        }
    }

    GetEntityManager().RemoveTag<EntityTag::UPDATE_LIGHT_TRANSFORM>(entity);
}

void LightVisibilityUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    GetEntityManager().RemoveTag<EntityTag::LIGHT>(entity);
}

void LightVisibilityUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    const Handle<Camera> &camera = GetEntityManager().GetScene()->GetCamera();

    { // Invalidate visibility state of directional lights that have had their transforms update to refresh the octree
        for (auto [entity_id, light_component, visibility_state_component, _] : GetEntityManager().GetEntitySet<LightComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::UPDATE_LIGHT_TRANSFORM>>().GetScopedView(GetComponentInfos())) {
            if (!light_component.light.IsValid() || !light_component.light->IsReady()) {
                continue;
            }
            
            if (light_component.light->GetLightType() == LightType::DIRECTIONAL) {
                visibility_state_component.flags |= VISIBILITY_STATE_FLAG_INVALIDATED;
            }
        }
    }

    { // Update light transforms if they have the UPDATE_LIGHT_TRANSFORM tag
        HashSet<ID<Entity>> updated_entity_ids;

        for (auto [entity_id, light_component, transform_component, _] : GetEntityManager().GetEntitySet<LightComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_LIGHT_TRANSFORM>>().GetScopedView(GetComponentInfos())) {
            if (!light_component.light.IsValid() || !light_component.light->IsReady()) {
                continue;
            }
            
            if (light_component.light->GetLightType() == LightType::DIRECTIONAL) {
                light_component.light->SetPosition(transform_component.transform.GetTranslation().Normalized());
            } else {
                light_component.light->SetPosition(transform_component.transform.GetTranslation());
            }

            HYP_LOG(Scene, Debug, "Updating light transform for entity #{}", entity_id.Value());

            updated_entity_ids.Insert(entity_id);
        }

        if (updated_entity_ids.Any()) {
            AfterProcess([this, entity_ids = std::move(updated_entity_ids)]()
            {
                for (const ID<Entity> &entity_id : entity_ids) {
                    GetEntityManager().RemoveTag<EntityTag::UPDATE_LIGHT_TRANSFORM>(entity_id);
                }
            });
        }
    }

    // Recalculate light visibility for the scene's camera
    for (auto [entity_id, light_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<LightComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
        if (!light_component.light.IsValid() || !light_component.light->IsReady()) {
            continue;
        }

        // For area lights, update the material ID if the entity has a MeshComponent.
        if (light_component.light->GetLightType() == LightType::AREA_RECT) {
            /*if (MeshComponent *mesh_component = entity_manager.TryGetComponent<MeshComponent>(entity)) {
                light_component.light->SetMaterial(mesh_component->material);
            } else {
                light_component.light->SetMaterial(Handle<Material>::empty);
            }*/
        }

        bool is_light_in_frustum = false;
        
        if (camera.IsValid()) {
            is_light_in_frustum = camera->GetFrustum().ContainsAABB(bounding_box_component.world_aabb);

            switch (light_component.light->GetLightType()) {
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
