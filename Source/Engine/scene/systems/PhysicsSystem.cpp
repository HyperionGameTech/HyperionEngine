/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/systems/PhysicsSystem.hpp>

#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <Core/reflection/Handle.hpp>

#include <physics/PhysicsWorld.hpp>

#include <engine/Game.hpp>
#include <Engine/EngineGlobals.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <PhysicsSystem.generated.inl>

namespace Hyperion {

extern PhysicsMaterial& GetDefaultPhysicsMaterial();
extern PhysicsShape& GetDefaultPhysicsShape();

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

    // temp debug
    rigidBodyComponent.physicsMaterial.mass = 1.0f;

    if (!rigidBodyComponent.shape)
    {
        Handle<PhysicsShape> shape = MakeHandle<BoxPhysicsShape>(NAME_FMT("{}_BoxPhysicsShape", entity->GetName()), entity->GetLocalBounds());
        shape->Register("$Import/PhysicsShapes");

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

    entity->GetWorld()->GetPhysicsWorld()->AddRigidBody(rigidBodyComponent.rigidBody);
}

void PhysicsSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    RigidBodyComponent& rigidBodyComponent = entity->GetEntityManager()->GetComponent<RigidBodyComponent>(entity);

    Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

    if (rigidBody.IsValid())
    {
        entity->GetWorld()->GetPhysicsWorld()->RemoveRigidBody(rigidBody);

        rigidBody->physicsMaterial = &GetDefaultPhysicsMaterial();
        rigidBody->shape = &GetDefaultPhysicsShape();

        rigidBody.Reset();
    }
}

void PhysicsSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    if (!GetWorld()->GetGameState().IsSimulating())
    {
    //    return;
    }

    PhysicsWorld& physicsWorld = static_cast<PhysicsWorld&>(*GetWorld()->GetPhysicsWorld());

    // To remove tag from after update.
    Array<Entity*, SceneAllocator> updatedEntities;

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, rigidBodyComponent, _] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TagComponent<EntityTag::UpdatePhysicsShape>>().GetScopedView(GetComponentInfos()))
        {
            Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

            if (!rigidBody)
            {
                continue;
            }
            
            physicsWorld.GetAdapter().OnChangePhysicsShape(rigidBody.Get());

            updatedEntities.PushBack(entity);
        }

        for (auto [entity, rigidBodyComponent, _] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TagComponent<EntityTag::UpdatePhysicsMaterial>>().GetScopedView(GetComponentInfos()))
        {
            Handle<RigidBody>& rigidBody = rigidBodyComponent.rigidBody;

            if (!rigidBody)
            {
                continue;
            }
            
            physicsWorld.GetAdapter().OnChangePhysicsMaterial(rigidBody.Get());

            updatedEntities.PushBack(entity);
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

            // @TODO Sync entity world transforms.
        }
    }

    if (updatedEntities.Any())
    {
        AfterProcess([entities = std::move(updatedEntities)]() mutable
        {
            for (Entity* entity : entities)
            {
                entity->RemoveTag<EntityTag::UpdatePhysicsMaterial>();
                entity->RemoveTag<EntityTag::UpdatePhysicsShape>();
            }
        });
    }
}

} // namespace Hyperion
