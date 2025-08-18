/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/util/EntityScripting.hpp>
#include <scene/EntityManager.hpp>
#include <scene/EntityTag.hpp>
#include <scene/ComponentInterface.hpp>
#include <scene/components/MeshComponent.hpp>
#include <scene/components/ScriptComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/BoundingBoxComponent.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/RenderProxy.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineDriver.hpp>

namespace hyperion {

Entity::Entity()
    : m_world(nullptr),
      m_entityManager(nullptr),
      m_renderProxyVersion(0),
      m_transformChanged(false)
{
}

Entity::~Entity()
{
    m_scene = nullptr;
    m_world = nullptr;

    // Keep a WeakHandle of Entity so the Id doesn't get reused while we're using it
    EntityManager* entityManager = GetEntityManager();
    if (entityManager == nullptr)
    {
        return;
    }

    if (Threads::IsOnThread(entityManager->GetOwnerThreadId()))
    {
        HYP_NAMED_SCOPE("Remove Entity from EntityManager (sync)");

        HYP_LOG(Entity, Debug, "Removing Entity {} from entity manager", Id());

        if (!entityManager->RemoveEntity(Id()))
        {
            HYP_LOG(Entity, Error, "Failed to remove Entity {} from EntityManager", Id());
        }
    }
    else
    {
        // If not on the correct thread, perform the removal asynchronously
        Threads::GetThread(entityManager->GetOwnerThreadId())->GetScheduler().Enqueue([weakThis = MakeWeakRef(this), entityManagerWeak = MakeWeakRef(entityManager)]()
            {
                Handle<EntityManager> entityManager = entityManagerWeak.Lock();
                if (!entityManager)
                {
                    HYP_LOG(Entity, Error, "EntityManager is no longer valid while removing Entity {}", weakThis.Id());
                    return;
                }

                HYP_NAMED_SCOPE("Remove Entity from EntityManager (async)");

                HYP_LOG(Entity, Debug, "Removing Entity {} from entity manager", weakThis.Id());

                if (!entityManager->RemoveEntity(weakThis.Id()))
                {
                    HYP_LOG(Entity, Error, "Failed to remove Entity {} from EntityManager", weakThis.Id());
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void Entity::Init()
{
    AssertDebug(m_scene != nullptr);
    SetEntityManager(m_scene->GetEntityManager());

    Node::Init();

    // If a TransformComponent already exists on the Entity, allow it to keep its current transform by moving the Node
    // to match it, as long as we're not locked
    // If transform is locked, the Entity's TransformComponent will be synced with the Node's current transform
    if (TransformComponent* transformComponent = m_entityManager->TryGetComponent<TransformComponent>(this))
    {
        if (!IsTransformLocked())
        {
            SetWorldTransform(transformComponent->transform);
        }
    }
    else
    {
        m_entityManager->AddComponent<TransformComponent>(this, TransformComponent { m_worldTransform });
    }

    if (BoundingBoxComponent* boundingBoxComponent = m_entityManager->TryGetComponent<BoundingBoxComponent>(this))
    {
        SetEntityAABB(boundingBoxComponent->localAabb);
    }
    else
    {
        SetEntityAABB(BoundingBox::Empty());
    }

    if (!m_entityManager->HasComponent<VisibilityStateComponent>(this))
    {
        m_entityManager->AddComponent<VisibilityStateComponent>(this, {});
    }

    m_entityManager->AddTags<EntityTag::UPDATE_AABB>(this);

    // set entity to static by default
    if (m_entityManager->HasTag<EntityTag::DYNAMIC>(this))
    {
        m_entityManager->RemoveTag<EntityTag::STATIC>(this);
    }
    else
    {
        m_entityManager->AddTag<EntityTag::STATIC>(this);
        m_entityManager->RemoveTag<EntityTag::DYNAMIC>(this);
    }

    // set transformChanged to false until entity is set to DYNAMIC
    m_transformChanged = false;

    SetReady(true);
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
        entityManager->AddTag<EntityTag::RECEIVES_UPDATE>(this);
    }
    else
    {
        entityManager->RemoveTag<EntityTag::RECEIVES_UPDATE>(this);
    }
}

void Entity::OnAttachedToNode(Node* node)
{
    Node::OnAttachedToNode(node);

    // SetScene() should've been called before this,
    // so EntityManager should be updated
    AssertDebug(GetEntityManager() == node->GetScene()->GetEntityManager());
}

void Entity::OnDetachedFromNode(Node* node)
{
    Node::OnDetachedFromNode(node);
}

void Entity::OnAddedToWorld(World* world)
{
    AssertDebug(world != nullptr);

    m_world = world;
}

void Entity::OnRemovedFromWorld(World* world)
{
    AssertDebug(world != nullptr);
    AssertDebug(m_world == world);

    m_world = nullptr;
}

void Entity::OnAddedToScene(Scene* scene)
{
    AssertDebug(scene != nullptr);

    EntityManager* entityManager = nullptr;
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

    if (BoundingBoxComponent* boundingBoxComponent = component.TryGet<BoundingBoxComponent>())
    {
        SetEntityAABB(boundingBoxComponent->localAabb);

        return;
    }
}

void Entity::OnComponentRemoved(AnyRef component)
{
    if (BoundingBoxComponent* boundingBoxComponent = component.TryGet<BoundingBoxComponent>())
    {
        SetEntityAABB(BoundingBox::Empty());

        return;
    }
}

void Entity::OnTagAdded(EntityTag tag)
{
}

void Entity::OnTagRemoved(EntityTag tag)
{
}

void Entity::SetScene(Scene* scene)
{
    if (scene == m_scene)
    {
        return;
    }

    Node::SetScene(scene);

    // Move entity from previous scene to new scene's EntityManager
    SetEntityManager(m_scene->GetEntityManager());
}

void Entity::LockTransform()
{
    Node::LockTransform();

    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr);

    // set entity to static
    entityManager->AddTag<EntityTag::STATIC>(this);
    entityManager->RemoveTag<EntityTag::DYNAMIC>(this);

    m_transformChanged = false;
}

void Entity::UnlockTransform()
{
    Node::UnlockTransform();
}

void Entity::OnTransformUpdated(const Transform& transform)
{
    Node::OnTransformUpdated(transform);

    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr);
    AssertDebug(entityManager == m_scene->GetEntityManager());

    if (!m_transformChanged)
    {
        // Set to dynamic
        entityManager->AddTag<EntityTag::DYNAMIC>(this);
        entityManager->RemoveTag<EntityTag::STATIC>(this);

        m_transformChanged = true;
    }

    TransformComponent& transformComponent = entityManager->GetComponent<TransformComponent>(this);
    transformComponent.transform = m_worldTransform;

    entityManager->AddTags<EntityTag::UPDATE_AABB>(this);

    SetNeedsRenderProxyUpdate();
}

void Entity::SetEntityManager(const Handle<EntityManager>& entityManager)
{
    AssertDebug(entityManager != nullptr);

    EntityManager* previousEntityManager = GetEntityManager();

    if (previousEntityManager)
    {
        if (previousEntityManager != entityManager)
        {
            Handle<Entity> thisHandle = HandleFromThis();
            previousEntityManager->MoveEntity(thisHandle, entityManager);
        }
    }
    else
    {
        Handle<Entity> thisHandle = HandleFromThis();
        entityManager->AddExistingEntity(thisHandle);
    }

    AssertDebug(m_entityManager == entityManager);
}

} // namespace hyperion
