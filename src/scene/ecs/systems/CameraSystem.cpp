/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/CameraSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>

#include <scene/camera/Camera.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Camera);

void CameraSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    CameraComponent &camera_component = GetEntityManager().GetComponent<CameraComponent>(entity);
    InitObject(camera_component.camera);

    HYP_LOG(Camera, Debug, "CameraSystem::OnEntityAdded: CameraComponent added to scene {} entity #{}", GetEntityManager().GetScene()->GetName(), entity->GetID().Value());

    // GetEntityManager().AddTag<EntityTag::CAMERA>(entity);
}

void CameraSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    // GetEntityManager().RemoveTag<EntityTag::CAMERA>(entity);

    HYP_LOG(Camera, Debug, "CameraSystem::OnEntityRemoved: CameraComponent removed from scene {} entity #{}", GetEntityManager().GetScene()->GetName(), entity.Value());
}

void CameraSystem::Process(GameCounter::TickUnit delta)
{
    HashSet<ID<Entity>> updated_entity_ids;

    Scene *scene = GetEntityManager().GetScene();

    for (auto [entity_id, camera_component, transform_component, _] : GetEntityManager().GetEntitySet<CameraComponent, TransformComponent, EntityTagComponent<EntityTag::UPDATE_CAMERA_TRANSFORM>>().GetScopedView(GetComponentInfos())) {
        if (!camera_component.camera.IsValid()) {
            continue;
        }
        
        camera_component.camera->SetTranslation(transform_component.transform.GetTranslation());
        camera_component.camera->SetDirection((transform_component.transform.GetRotation() * Vec3f(0.0f, 0.0f, 1.0f)).Normalize());
    }

    for (auto [entity_id, camera_component] : GetEntityManager().GetEntitySet<CameraComponent>().GetScopedView(GetComponentInfos())) {
        if (!camera_component.camera.IsValid()) {
            continue;
        }
        
        camera_component.camera->Update(delta);

        if (scene != nullptr) {
            scene->GetOctree().CalculateVisibility(camera_component.camera);
        }

        updated_entity_ids.Insert(entity_id);
    }

    for (auto [entity_id, camera_component, node_link_component] : GetEntityManager().GetEntitySet<CameraComponent, NodeLinkComponent>().GetScopedView(GetComponentInfos())) {
        if (!camera_component.camera.IsValid()) {
            continue;
        }
        
        if (!node_link_component.node.IsValid()) {
            continue;
        }

        Handle<Node> node = node_link_component.node.Lock();

        if (!node) {
            continue;
        }

        Transform new_transform(camera_component.camera->GetTranslation(), Vec3f::One(), camera_component.camera->GetViewMatrix().ExtractRotation());
        node->SetWorldTransform(new_transform);
    }

    if (updated_entity_ids.Any()) {
        AfterProcess([this, entity_ids = std::move(updated_entity_ids)]()
        {
            for (const ID<Entity> &entity_id : entity_ids) {
                GetEntityManager().RemoveTag<EntityTag::UPDATE_CAMERA_TRANSFORM>(entity_id);
            }
        });
    }
}

} // namespace hyperion
