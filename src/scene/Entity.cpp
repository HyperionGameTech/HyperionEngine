/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/util/EntityScripting.hpp>
#include <scene/EntityManager.hpp>
#include <scene/EntityTag.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/components/NodeLinkComponent.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/ScriptComponent.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/RenderProxy.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineDriver.hpp>

namespace hyperion {

Entity::Entity()
    : m_renderProxyVersion(0)
{
}

Entity::~Entity()
{
    if (!IsInitCalled())
    {
        return;
    }
    
    // Keep a WeakHandle of Entity so the Id doesn't get reused while we're using it
    EntityManager* entityManager = GetEntityManager();
    
    if (!entityManager)
    {
        return;
    }

    if (Threads::IsOnThread(entityManager->GetOwnerThreadId()))
    {
        HYP_NAMED_SCOPE("Remove Entity from EntityManager (sync)");

        HYP_LOG(Entity, Debug, "Removing Entity {} from entity manager", Id());

        AssertDebug(entityManager->HasEntity(this));

        if (!entityManager->RemoveEntity(this))
        {
            HYP_LOG(Entity, Error, "Failed to remove Entity {} from EntityManager", Id());
        }
    }
    else
    {
        // If not on the correct thread, perform the removal asynchronously
        Threads::GetThread(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue([weakThis = WeakHandleFromThis(), entityManagerWeak = entityManager->WeakHandleFromThis()]()
            {
                Handle<EntityManager> entityManager = entityManagerWeak.Lock();
                if (!entityManager)
                {
                    HYP_LOG(Entity, Error, "EntityManager is no longer valid while removing Entity {}", weakThis.Id());
                    return;
                }

                HYP_NAMED_SCOPE("Remove Entity from EntityManager (async)");

                HYP_LOG(Entity, Debug, "Removing Entity {} from entity manager", weakThis.Id());

                AssertDebug(entityManager->HasEntity(weakThis.GetUnsafe()));

                if (!entityManager->RemoveEntity(weakThis.GetUnsafe()))
                {
                    HYP_LOG(Entity, Error, "Failed to remove Entity {} from EntityManager", weakThis.Id());
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void Entity::Init()
{
    Node::Init();

    SetReady(true);
}

EntityManager* Entity::GetEntityManager() const
{
    HYP_SCOPE;
    AssertReady();
    
    Assert(m_scene != nullptr);

    return m_scene->GetEntityManager();
}

bool Entity::ReceivesUpdate() const
{
    if (!m_entityInitInfo.canEverUpdate)
    {
        return false;
    }

    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr, "EntityManager is null for Entity {} while checking receives update", Id());

    Threads::AssertOnThread(entityManager->GetOwnerThreadId());

    return entityManager->HasTag<EntityTag::RECEIVES_UPDATE>(this);
}

void Entity::SetReceivesUpdate(bool receivesUpdate)
{
    if (!m_entityInitInfo.canEverUpdate)
    {
        AssertDebug(!receivesUpdate, "Entity {} cannot receive updates, but SetReceivesUpdate() was called with true", Id());

        return;
    }

    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr, "EntityManager is null for Entity {} while setting receives update", Id());

    Threads::AssertOnThread(entityManager->GetOwnerThreadId());

    if (receivesUpdate)
    {
        AddTag<EntityTag::RECEIVES_UPDATE>();
    }
    else
    {
        RemoveTag<EntityTag::RECEIVES_UPDATE>();
    }
}

void Entity::OnAddedToWorld(World* world)
{
    AssertDebug(world != nullptr);
}

void Entity::OnRemovedFromWorld(World* world)
{
    AssertDebug(world != nullptr);
}

void Entity::OnAddedToScene(Scene* scene)
{
    AssertDebug(scene != nullptr);
}

void Entity::OnRemovedFromScene(Scene* scene)
{
    AssertDebug(scene != nullptr);
}

void Entity::OnComponentAdded(AnyRef component)
{
    if (MeshComponent* meshComponent = component.TryGet<MeshComponent>())
    {
        if (!meshComponent->mesh.IsValid())
        {
            HYP_LOG(Entity, Warning, "Entity {} has a MeshComponent with an invalid mesh", Id());

            return;
        }

        InitObject(meshComponent->mesh);

        if (m_entityInitInfo.bvhDepth > 0)
        {
            if (meshComponent->mesh->GetBVH().IsValid())
            {
                // already has a BVH, skip
                return;
            }

            if (!meshComponent->mesh->BuildBVH(m_entityInitInfo.bvhDepth))
            {
                HYP_LOG(Entity, Error, "Failed to build BVH for MeshComponent on Entity {}!", Id());

                return;
            }
        }

        return;
    }

    // if (ScriptComponent* scriptComponent = component.TryGet<ScriptComponent>())
    // {
    //     EntityScripting::InitEntityScriptComponent(this, *scriptComponent);

    //     return;
    // }
}

void Entity::OnComponentRemoved(AnyRef component)
{
    // if (ScriptComponent* scriptComponent = component.TryGet<ScriptComponent>())
    // {
    //     EntityScripting::DeinitEntityScriptComponent(this, *scriptComponent);

    //     return;
    // }
}

void Entity::OnTagAdded(EntityTag tag)
{
}

void Entity::OnTagRemoved(EntityTag tag)
{
}

void Entity::OnTransformUpdated(const Transform& transform)
{
    // Do nothing
}

//void Entity::SetScene_Internal(Scene* scene, Scene* previousScene)
//{
    // if (!scene)
    // {
    //     return;
    // }

    // if (!previousScene)
    // {
    //     scene->GetEntityManager()->AddExistingEntity(HandleFromThis());

    //     return;
    // }

    // if (previousScene->GetEntityManager() != scene->GetEntityManager())
    // {
    //     if (previousScene != nullptr && previousScene->GetEntityManager() != nullptr)
    //     {
    //         Assert(scene->GetEntityManager() != nullptr);

    //         previousScene->GetEntityManager()->MoveEntity(HandleFromThis(), scene->GetEntityManager());
    //     }
    //     else
    //     {
    //         // Entity manager null - exiting engine is likely cause here
    //     }
    // }
//}

} // namespace hyperion
