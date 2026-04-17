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

void PhysicsSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    RigidBodyComponent& rigidBodyComponent = entity->GetEntityManager()->GetComponent<RigidBodyComponent>(entity);
    TransformComponent& transformComponent = entity->GetEntityManager()->GetComponent<TransformComponent>(entity);

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

    if (GetWorld()->GetGameState().IsSimulating())
    {
        m_simulationOriginTransforms.Set(entity, entity->GetLocalTransform());
    }

    entity->GetWorld()->GetPhysicsWorld()->AddRigidBody(rigidBodyComponent.rigidBody);
}

void PhysicsSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    m_simulationOriginTransforms.Erase(entity);

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

void PhysicsSystem::OnAddedToWorld(World* world)
{
    SystemBase::OnAddedToWorld(world);

    if (Game* game = world->GetGame())
    {
        m_delegateHandlers.Add(
            NAME("OnGameStateChange"),
            game->OnGameStateChange.Bind([this](Game*, GameStateMode previousMode, GameStateMode currentMode)
            {
                const bool wasSimulating = previousMode == GameStateMode::SIMULATING
                    || previousMode == GameStateMode::PAUSED;
                const bool isSimulating  = currentMode  == GameStateMode::SIMULATING
                    || currentMode  == GameStateMode::PAUSED;

                if (isSimulating && !wasSimulating)
                {
                    SaveSimulationOrigins();
                }
                else if (!isSimulating && wasSimulating)
                {
                    RestoreSimulationOrigins();
                }
            }));
    }
}

void PhysicsSystem::OnRemovedFromWorld(World* world)
{
    SystemBase::OnRemovedFromWorld(world);

    m_delegateHandlers.Remove(NAME("OnGameStateChange"));
    m_simulationOriginTransforms.Clear();
}

void PhysicsSystem::SaveSimulationOrigins()
{
    m_simulationOriginTransforms.Clear();

    World* world = GetWorld();

    for (Scene* scene : world->GetScenes())
    {
        if (!scene)
        {
            continue;
        }

        for (auto [entity, rigidBodyComponent, transformComponent] : scene->GetEntityManager()->GetEntitySet<RigidBodyComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
        {
            m_simulationOriginTransforms.Set(entity, entity->GetLocalTransform());
        }
    }
}

void PhysicsSystem::RestoreSimulationOrigins()
{
    for (auto& [entity, originTransform] : m_simulationOriginTransforms)
    {
        if (entity == nullptr)
        {
            continue;
        }

        entity->SetLocalTransform(originTransform, TransformChangeType::Default);
    }

    m_simulationOriginTransforms.Clear();
}

void PhysicsSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
}

} // namespace Hyperion
