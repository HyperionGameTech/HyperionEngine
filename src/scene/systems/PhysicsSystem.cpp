/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scene/systems/PhysicsSystem.hpp>
#include <scene/EntityManager.hpp>

#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <core/reflection/Handle.hpp>

#include <PhysicsSystem.generated.inl>

namespace hyperion {

bool PhysicsSystem::ShouldCreateForScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | ExpectedFlags)) == ExpectedFlags;
}

void PhysicsSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!GetEntityManager().GetScene()->GetWorld())
    {
        return;
    }

    RigidBodyComponent& rigidBodyComponent = GetEntityManager().GetComponent<RigidBodyComponent>(entity);
    TransformComponent& transformComponent = GetEntityManager().GetComponent<TransformComponent>(entity);

    if (rigidBodyComponent.rigidBody)
    {
        InitObject(rigidBodyComponent.rigidBody);

        rigidBodyComponent.rigidBody->SetTransform(transformComponent.transform);
        rigidBodyComponent.transformHashCode = transformComponent.transform.GetHashCode();

        rigidBodyComponent.flags |= RigidBodyComponentFlags::INIT;

        GetEntityManager().GetScene()->GetWorld()->GetPhysicsWorld()->AddRigidBody(rigidBodyComponent.rigidBody);
    }
}

void PhysicsSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (!GetEntityManager().GetScene()->GetWorld())
    {
        return;
    }

    RigidBodyComponent& rigidBodyComponent = GetEntityManager().GetComponent<RigidBodyComponent>(entity);

    if (rigidBodyComponent.rigidBody)
    {
        GetEntityManager().GetScene()->GetWorld()->GetPhysicsWorld()->RemoveRigidBody(rigidBodyComponent.rigidBody);
    }
}

bool PhysicsSystem::NeedsUpdateThisFrame() const
{
    const auto* es = GetEntityManager().TryGetEntitySet<RigidBodyComponent, TransformComponent>();
    return es && es->GetElements().Any();
}

void PhysicsSystem::Process(float delta)
{
    for (auto [entity, rigidBodyComponent, transformComponent] : GetEntityManager().GetEntitySet<RigidBodyComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
    {
        Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;
        Transform& transform = transformComponent.transform;

        if (!rigidBody)
        {
            continue;
        }

        Transform rigidBodyTransform = rigidBody->GetTransform();
        transform.SetTranslation(rigidBodyTransform.GetTranslation());
        transform.SetRotation(rigidBodyTransform.GetRotation());

        rigidBody->SetTransform(rigidBodyTransform);
    }
}

} // namespace hyperion
