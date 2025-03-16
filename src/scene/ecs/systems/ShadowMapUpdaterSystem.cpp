/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ShadowMapUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/camera/Camera.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <scene/World.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/PointLightShadowRenderer.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Shadows);

ShadowMapUpdaterSystem::ShadowMapUpdaterSystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(NAME("OnWorldChange"), OnWorldChanged.Bind([this](World *new_world, World *previous_world)
    {
        Threads::AssertOnThread(g_game_thread);

        // Trigger removal and addition of render subsystems

        for (auto [entity_id, shadow_map_component, light_component] : GetEntityManager().GetEntitySet<ShadowMapComponent, LightComponent>().GetScopedView(GetComponentInfos())) {
            if (shadow_map_component.render_subsystem) {
                shadow_map_component.render_subsystem->RemoveFromEnvironment();
            }

            if (!new_world) {
                shadow_map_component.render_subsystem.Reset();
            }

            AddRenderSubsystemToEnvironment(shadow_map_component, light_component, new_world);
        }
    }));
}

void ShadowMapUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    ShadowMapComponent &shadow_map_component = GetEntityManager().GetComponent<ShadowMapComponent>(entity);
    LightComponent &light_component = GetEntityManager().GetComponent<LightComponent>(entity);

    if (!light_component.light) {
        return;
    }

    if (shadow_map_component.render_subsystem) {
        return;
    }

    if (!GetWorld()) {
        return;
    }

    AddRenderSubsystemToEnvironment(shadow_map_component, light_component, GetWorld());
}

void ShadowMapUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    ShadowMapComponent &shadow_map_component = GetEntityManager().GetComponent<ShadowMapComponent>(entity);
    LightComponent &light_component = GetEntityManager().GetComponent<LightComponent>(entity);

    if (shadow_map_component.render_subsystem) {
        shadow_map_component.render_subsystem->RemoveFromEnvironment();

        shadow_map_component.render_subsystem = nullptr;
    }
}

void ShadowMapUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, shadow_map_component, light_component, transform_component] : GetEntityManager().GetEntitySet<ShadowMapComponent, LightComponent, TransformComponent>().GetScopedView(GetComponentInfos())) {
        if (!light_component.light) {
            continue;
        }

        if (!shadow_map_component.render_subsystem) {
            continue;
        }

        // only update shadow map every 10 ticks
        if (shadow_map_component.update_counter++ % 10 != 0) {
            continue;
        }

        switch (light_component.light->GetLightType()) {
        case LightType::DIRECTIONAL: {
            DirectionalLightShadowRenderer *shadow_renderer = static_cast<DirectionalLightShadowRenderer *>(shadow_map_component.render_subsystem.Get());

            const Vec3f &center = transform_component.transform.GetTranslation();
            const Vec3f light_direction = light_component.light->GetPosition().Normalized() * -1.0f;

            const Handle<Camera> &shadow_camera = shadow_renderer->GetCamera();

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

            shadow_renderer->GetCamera()->SetToOrthographicProjection(aabb.min.x, aabb.max.x, aabb.min.y, aabb.max.y, aabb.min.z, aabb.max.z);

            shadow_renderer->SetAABB(aabb);

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

void ShadowMapUpdaterSystem::AddRenderSubsystemToEnvironment(ShadowMapComponent &shadow_map_component, LightComponent &light_component, World *world)
{
    AssertThrow(world != nullptr);
    AssertThrow(world->IsReady());

    if (shadow_map_component.render_subsystem) {
        world->GetRenderResource().GetEnvironment()->AddRenderSubsystem(shadow_map_component.render_subsystem);
    } else {
        switch (light_component.light->GetLightType()) {
        case LightType::DIRECTIONAL:
            shadow_map_component.render_subsystem = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<DirectionalLightShadowRenderer>(
                Name::Unique("shadow_map_renderer_directional"),
                GetScene()->HandleFromThis(),
                shadow_map_component.resolution,
                shadow_map_component.mode
            );

            break;
        case LightType::POINT:
            shadow_map_component.render_subsystem = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<PointLightShadowRenderer>(
                Name::Unique("shadow_map_renderer_point"),
                GetScene()->HandleFromThis(),
                light_component.light,
                shadow_map_component.resolution
            );

            break;
        default:
            HYP_LOG(Shadows, Error, "Unsupported light type for shadow map");

            break;
        }
    }
}

} // namespace hyperion
