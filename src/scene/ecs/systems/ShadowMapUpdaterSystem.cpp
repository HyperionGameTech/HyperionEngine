/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ShadowMapUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/PointLightShadowRenderer.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <math/MathUtil.hpp>
#include <Threads.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void ShadowMapUpdaterSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    ShadowMapComponent &shadow_map_component = entity_manager.GetComponent<ShadowMapComponent>(entity);
    LightComponent &light_component = entity_manager.GetComponent<LightComponent>(entity);

    if (!light_component.light) {
        return;
    }

    if (shadow_map_component.render_component) {
        return;
    }

    switch (light_component.light->GetType()) {
    case LightType::DIRECTIONAL:
        shadow_map_component.render_component = entity_manager.GetScene()->GetEnvironment()->AddRenderComponent<DirectionalLightShadowRenderer>(
            Name::Unique("shadow_map_renderer_directional"),
            shadow_map_component.resolution,
            shadow_map_component.mode
        );

        break;
    case LightType::POINT:
        shadow_map_component.render_component = entity_manager.GetScene()->GetEnvironment()->AddRenderComponent<PointLightShadowRenderer>(
            Name::Unique("shadow_map_renderer_point"),
            light_component.light,
            shadow_map_component.resolution
        );

        break;
    default:
        DebugLog(
            LogType::Warn,
            "Light type %u not supported for shadow mapping\n",
            uint32(light_component.light->GetType())
        );

        break;
    }
}

void ShadowMapUpdaterSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    ShadowMapComponent &shadow_map_component = entity_manager.GetComponent<ShadowMapComponent>(entity);
    LightComponent &light_component = entity_manager.GetComponent<LightComponent>(entity);

    if (shadow_map_component.render_component) {
        switch (light_component.light->GetType()) {
        case LightType::DIRECTIONAL:
            entity_manager.GetScene()->GetEnvironment()->RemoveRenderComponent<DirectionalLightShadowRenderer>(shadow_map_component.render_component->GetName());

            break;
        case LightType::POINT:
            entity_manager.GetScene()->GetEnvironment()->RemoveRenderComponent<PointLightShadowRenderer>(shadow_map_component.render_component->GetName());

            break;
        default:
            break;
        }

        shadow_map_component.render_component = nullptr;
    }
}

void ShadowMapUpdaterSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, shadow_map_component, light_component, transform_component] : entity_manager.GetEntitySet<ShadowMapComponent, LightComponent, TransformComponent>()) {
        if (!light_component.light) {
            continue;
        }

        if (!shadow_map_component.render_component) {
            continue;
        }

        // only update shadow map every 10 ticks
        if (shadow_map_component.update_counter++ % 10 != 0) {
            continue;
        }

        switch (light_component.light->GetType()) {
        case LightType::DIRECTIONAL: {
            DirectionalLightShadowRenderer *shadow_renderer = static_cast<DirectionalLightShadowRenderer *>(shadow_map_component.render_component.Get());

            const Vec3f &center = transform_component.transform.GetTranslation();
            const Vec3f light_direction = light_component.light->GetPosition().Normalized() * -1.0f;

            const Handle<Camera> &shadow_camera = shadow_renderer->GetPass()->GetCamera();

            if (!shadow_camera) {
                continue;
            }

            shadow_camera->SetTranslation(center + light_direction);
            shadow_camera->SetTarget(center);

            BoundingBox aabb { center - shadow_map_component.radius, center + shadow_map_component.radius };

            FixedArray<Vec3f, 8> corners = aabb.GetCorners();

            for (Vec3f &corner : corners) {
                corner = shadow_camera->GetViewMatrix() * corner;

                aabb.max = MathUtil::Max(aabb.max, corner);
                aabb.min = MathUtil::Min(aabb.min, corner);
            }

            aabb.max.z = shadow_map_component.radius;
            aabb.min.z = -shadow_map_component.radius;

            light_component.light->SetShadowMapIndex(shadow_renderer->GetComponentIndex());

            shadow_renderer->GetPass()->GetCamera()->SetToOrthographicProjection(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z);

            // shadow_renderer->GetPass()->GetCamera()->Update(delta);
            // entity_manager.GetScene()->GetOctree().CalculateVisibility(shadow_renderer->GetPass()->GetCamera().Get());

            shadow_renderer->SetCameraData({
                shadow_camera->GetViewMatrix(),
                shadow_camera->GetProjectionMatrix(),
                aabb
            });

            break;
        }
        case LightType::POINT:
            // No update needed
            break;
        default:
            break;
        }
    }
}

} // namespace hyperion::v2
