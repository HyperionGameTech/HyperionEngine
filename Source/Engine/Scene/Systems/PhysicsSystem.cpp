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
#include <Scene/Components/MeshComponent.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Physics/PhysicsWorld.hpp>
#include <Physics/PhysicsShape.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Framework/Client/GameClient.hpp>

#include <Rendering/Mesh.hpp>

#include <PhysicsSystem.generated.inl>

namespace Hyperion {

extern PhysicsMaterial& GetDefaultPhysicsMaterial();
extern PhysicsShape* GetDefaultPhysicsShape();

bool PhysicsSystem::CanSimulate(const RigidBodyComponent&)
{
    return EngineGlobals::HasAuthority();
}

void PhysicsSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    RigidBodyComponent& rigidBodyComponent = entity->GetEntityManager()->GetComponent<RigidBodyComponent>(entity);
    TransformComponent& transformComponent = entity->GetEntityManager()->GetComponent<TransformComponent>(entity);
    if (!rigidBodyComponent.shape)
    {
        BoundingBox boxBounds = entity->GetLocalBounds();

        if (!boxBounds.IsValid())
        {
            if (MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>(); meshComponent != nullptr && meshComponent->mesh.IsValid())
            {
                boxBounds = meshComponent->mesh->GetAABB();
            }
        }

        if (!boxBounds.IsValid())
        {
            boxBounds = BoundingBox(Vec3f(-0.5f), Vec3f(0.5f));
        }

        Handle<PhysicsShape> shape = MakeHandle<BoxPhysicsShape>(NAME_FMT("{}_{}_BoxPhysicsShape", entity->GetName(), entity->Id().Value()), boxBounds);
        GetCurrentAssetRegistry()->PutAsset(shape);

        rigidBodyComponent.shape = shape;
    }

    Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

    // Create if it doesn't exist
    if (!rigidBody)
    {
        rigidBody = MakeHandle<RigidBody>();
    }

    rigidBody->shape = rigidBodyComponent.shape.Get();
    rigidBody->physicsMaterial = &rigidBodyComponent.physicsMaterial;

    rigidBody->SetVelocity(rigidBodyComponent.initialVelocity);
    rigidBody->SetAngularVelocity(rigidBodyComponent.initialAngularVelocity);

    Transform transform;
    transform.SetTranslation(transformComponent.translation);
    transform.SetRotation(transformComponent.rotation);
    transform.SetScale(transformComponent.scale);

    rigidBody->SetTransform(transform);

    // The physics world is only ticked with authority, on clients they act as colliders.
    // This gives clients static geometry to collide with
    {
        PhysicsWorldBase* physicsWorld = GetWorld()->GetPhysicsWorld();
        AssertDebug(physicsWorld != nullptr);

        if (!physicsWorld)
        {
            return;
        }

        physicsWorld->AddRigidBody(rigidBodyComponent.rigidBody);
    }
}

void PhysicsSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    RigidBodyComponent& rigidBodyComponent = entity->GetEntityManager()->GetComponent<RigidBodyComponent>(entity);

    {
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
            rigidBody->shape = GetDefaultPhysicsShape();

            rigidBody.Reset();
        }
    }
}

void PhysicsSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
}

} // namespace Hyperion
