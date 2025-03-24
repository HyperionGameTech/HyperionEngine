/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/ReflectionProbeUpdaterSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/EnvProbe.hpp>

#include <rendering/ReflectionProbeRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderWorld.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/Threads.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(EnvProbe);

ReflectionProbeUpdaterSystem::ReflectionProbeUpdaterSystem(EntityManager &entity_manager)
    : System(entity_manager)
{
    m_delegate_handlers.Add(NAME("OnWorldChange"), OnWorldChanged.Bind([this](World *new_world, World *previous_world)
    {
        Threads::AssertOnThread(g_game_thread);

        // Trigger removal and addition of render subsystems

        for (auto [entity_id, reflection_probe_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<ReflectionProbeComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
            if (reflection_probe_component.reflection_probe_renderer) {
                reflection_probe_component.reflection_probe_renderer->RemoveFromEnvironment();

                if (new_world != nullptr) {
                    new_world->GetRenderResource().GetEnvironment()->AddRenderSubsystem(reflection_probe_component.reflection_probe_renderer);
                }
            } else if (!previous_world) {
                OnEntityAdded(Handle<Entity> { entity_id });
            }
        }
    }));
}

void ReflectionProbeUpdaterSystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    ReflectionProbeComponent &reflection_probe_component = GetEntityManager().GetComponent<ReflectionProbeComponent>(entity);
    BoundingBoxComponent &bounding_box_component = GetEntityManager().GetComponent<BoundingBoxComponent>(entity);

    if (reflection_probe_component.reflection_probe_renderer != nullptr) {
        return;
    }

    BoundingBox world_aabb = bounding_box_component.world_aabb;

    if (!world_aabb.IsFinite()) {
        world_aabb = BoundingBox::Empty();
    }

    if (!world_aabb.IsValid()) {
        HYP_LOG(EnvProbe, Warning, "Entity #{} has invalid bounding box", entity.GetID().Value());
    }

    if (!GetWorld()) {
        return;
    }

    if (GetScene()->IsForegroundScene()) {
        reflection_probe_component.reflection_probe_renderer = GetWorld()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<ReflectionProbeRenderer>(
            Name::Unique("reflection_probe"),
            GetScene()->HandleFromThis(),
            world_aabb
        );
    }
}

void ReflectionProbeUpdaterSystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    ReflectionProbeComponent &reflection_probe_component = GetEntityManager().GetComponent<ReflectionProbeComponent>(entity);

    if (reflection_probe_component.reflection_probe_renderer != nullptr) {
        reflection_probe_component.reflection_probe_renderer->RemoveFromEnvironment();

        reflection_probe_component.reflection_probe_renderer = nullptr;
    }
}

void ReflectionProbeUpdaterSystem::Process(GameCounter::TickUnit delta)
{
    for (auto [entity_id, reflection_probe_component, transform_component, bounding_box_component] : GetEntityManager().GetEntitySet<ReflectionProbeComponent, TransformComponent, BoundingBoxComponent>().GetScopedView(GetComponentInfos())) {
        if (!reflection_probe_component.reflection_probe_renderer) {
            continue;
        }
        
        const HashCode transform_hash_code = transform_component.transform.GetHashCode();

        const BoundingBox &world_aabb = bounding_box_component.world_aabb;

        if (reflection_probe_component.transform_hash_code != transform_hash_code || reflection_probe_component.reflection_probe_renderer->GetAABB() != world_aabb) {
            // @TODO
            // reflection_probe_component.reflection_probe_renderer->SetAABB(world_aabb);

            reflection_probe_component.transform_hash_code = transform_hash_code;
        }
    }
}

} // namespace hyperion
