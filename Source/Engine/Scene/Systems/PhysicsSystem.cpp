/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/PhysicsSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Physics/PhysicsWorld.hpp>
#include <Physics/PhysicsShape.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <PhysicsSystem.generated.inl>

namespace Hyperion {

extern PhysicsMaterial& GetDefaultPhysicsMaterial();
extern PhysicsShape& GetDefaultPhysicsShape();

void PhysicsSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    RigidBodyComponent& rigidBodyComponent = entity->GetEntityManager()->GetComponent<RigidBodyComponent>(entity);
    TransformComponent& transformComponent = entity->GetEntityManager()->GetComponent<TransformComponent>(entity);

    if (!rigidBodyComponent.shape)
    {
        Handle<PhysicsShape> shape = MakeHandle<BoxPhysicsShape>(NAME_FMT("{}_BoxPhysicsShape", entity->GetName()), entity->GetLocalBounds());
        GetCurrentAssetRegistry()->PutAsset(shape);

        rigidBodyComponent.shape = shape;
    }

    Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

    if (!rigidBody)
    {
        rigidBody = MakeHandle<RigidBody>();
    }

    rigidBody->shape = rigidBodyComponent.shape.Get();
    rigidBody->physicsMaterial = &rigidBodyComponent.physicsMaterial;

    Transform transform;
    transform.SetTranslation(transformComponent.translation);
    transform.SetRotation(transformComponent.rotation);
    transform.SetScale(transformComponent.scale);

    rigidBody->SetTransform(transform);

    PhysicsWorldBase* physicsWorld = GetWorld()->GetPhysicsWorld();
    AssertDebug(physicsWorld != nullptr);

    if (!physicsWorld)
    {
        return;
    }

    physicsWorld->AddRigidBody(rigidBodyComponent.rigidBody);
}

void PhysicsSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    RigidBodyComponent& rigidBodyComponent = entity->GetEntityManager()->GetComponent<RigidBodyComponent>(entity);

    Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

    if (rigidBody.IsValid())
    {

        PhysicsWorldBase* physicsWorld = GetWorld()->GetPhysicsWorld();
        AssertDebug(physicsWorld != nullptr);

        if (physicsWorld)
        {
            physicsWorld->RemoveRigidBody(rigidBody);
        }

        rigidBody->physicsMaterial = &GetDefaultPhysicsMaterial();
        rigidBody->shape = &GetDefaultPhysicsShape();

        rigidBody.Reset();
    }
}

void PhysicsSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
}

} // namespace Hyperion
