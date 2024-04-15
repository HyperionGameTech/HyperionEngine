/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/PhysicsSystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <Engine.hpp>

namespace hyperion::v2 {

void PhysicsSystem::OnEntityAdded(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityAdded(entity_manager, entity);

    if (!entity_manager.GetScene()->GetWorld()) {
        return;
    }

    RigidBodyComponent &rigid_body_component = entity_manager.GetComponent<RigidBodyComponent>(entity);
    TransformComponent &transform_component = entity_manager.GetComponent<TransformComponent>(entity);

    if (rigid_body_component.rigid_body) {
        InitObject(rigid_body_component.rigid_body);

        rigid_body_component.rigid_body->SetTransform(transform_component.transform);
        rigid_body_component.transform_hash_code = transform_component.transform.GetHashCode();

        rigid_body_component.flags |= RIGID_BODY_COMPONENT_FLAG_INIT;

        entity_manager.GetScene()->GetWorld()->GetPhysicsWorld().AddRigidBody(rigid_body_component.rigid_body);
    }
}

void PhysicsSystem::OnEntityRemoved(EntityManager &entity_manager, ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity_manager, entity);

    if (!entity_manager.GetScene()->GetWorld()) {
        return;
    }

    RigidBodyComponent &rigid_body_component = entity_manager.GetComponent<RigidBodyComponent>(entity);

    if (rigid_body_component.rigid_body) {
        entity_manager.GetScene()->GetWorld()->GetPhysicsWorld().RemoveRigidBody(rigid_body_component.rigid_body);
    }
}

void PhysicsSystem::Process(EntityManager &entity_manager, GameCounter::TickUnit delta)
{
    for (auto [entity_id, rigid_body_component, transform_component] : entity_manager.GetEntitySet<RigidBodyComponent, TransformComponent>()) {
        Handle<physics::RigidBody> &rigid_body = rigid_body_component.rigid_body;
        Transform &transform = transform_component.transform;

        if (!rigid_body) {
            continue;
        }

        Transform &rigid_body_transform = rigid_body->GetTransform();

        transform.SetTranslation(rigid_body_transform.GetTranslation());
        transform.SetRotation(rigid_body_transform.GetRotation());
    }
}

} // namespace hyperion::v2
