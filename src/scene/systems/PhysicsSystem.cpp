/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/systems/PhysicsSystem.hpp>

#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <core/reflection/Handle.hpp>

#include <physics/PhysicsWorld.hpp>

#include <PhysicsSystem.generated.inl>

namespace hyperion {

bool PhysicsSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | ExpectedFlags)) == ExpectedFlags;
}

void PhysicsSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    RigidBodyComponent& rigidBodyComponent = entity->GetEntityManager()->GetComponent<RigidBodyComponent>(entity);
    TransformComponent& transformComponent = entity->GetEntityManager()->GetComponent<TransformComponent>(entity);

    if (rigidBodyComponent.rigidBody)
    {
        InitObject(rigidBodyComponent.rigidBody);

        Transform transform;
        transform.SetTranslation(transformComponent.translation);
        transform.SetRotation(transformComponent.rotation);
        transform.SetScale(transformComponent.scale);

        rigidBodyComponent.rigidBody->SetTransform(transform);
        rigidBodyComponent.transformHashCode = transform.GetHashCode();

        rigidBodyComponent.flags |= RigidBodyComponentFlags::INIT;

        entity->GetWorld()->GetPhysicsWorld()->AddRigidBody(rigidBodyComponent.rigidBody);
    }
}

void PhysicsSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    RigidBodyComponent& rigidBodyComponent = entity->GetEntityManager()->GetComponent<RigidBodyComponent>(entity);

    if (rigidBodyComponent.rigidBody)
    {
        entity->GetWorld()->GetPhysicsWorld()->RemoveRigidBody(rigidBodyComponent.rigidBody);
    }
}

void PhysicsSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    if (!GetWorld()->GetGameState().IsSimulating())
    {
        return;
    }

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, rigidBodyComponent, transformComponent] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
        {
            Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

            if (!rigidBody)
            {
                continue;
            }

            Transform rigidBodyTransform = rigidBody->GetTransform();
            transformComponent.translation = rigidBodyTransform.GetTranslation();
            transformComponent.rotation = rigidBodyTransform.GetRotation();

            rigidBody->SetTransform(rigidBodyTransform);
        }
    }
}

} // namespace hyperion
