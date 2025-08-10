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
    : m_world(nullptr),
      m_entityManager(nullptr),
      m_renderProxyVersion(0)
{
}

Entity::~Entity()
{
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

void Entity::AttachTo(const Handle<Node>& attachNode)
{
    { // detach from node if already attached.
        EntityManager* entityManager = GetEntityManager();
        AssertDebug(entityManager != nullptr, "EntityManager is null for Entity {} while attaching to Node", Id());

        Threads::AssertOnThread(entityManager->GetOwnerThreadId());

        if (NodeLinkComponent* nodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(this))
        {
            if (Handle<Node> node = nodeLinkComponent->node.Lock())
            {
                if (node == attachNode)
                {
                    return;
                }

                AssertDebug(node->GetEntity() == this);

                // Unset Entity first.
                node->SetEntity(Handle<Entity>::empty);
            }
        }
    }

    // Attach() called with empty node, so just leave it as detached from any node, and return.
    if (!attachNode.IsValid())
    {
        return;
    }

    Handle<Entity> strongThis = HandleFromThis();

    // add a subnode to the attachNode, and set the entity to this.
    Handle<Node> subnode = attachNode->AddChild();
    subnode->SetEntity(strongThis);
}

void Entity::Detach()
{
    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr, "EntityManager is null for Entity {} while detaching from Node", Id());

    Threads::AssertOnThread(entityManager->GetOwnerThreadId());

    if (NodeLinkComponent* nodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(this))
    {
        if (Handle<Node> node = nodeLinkComponent->node.Lock())
        {
            node->SetEntity(Handle<Entity>::empty);
        }
    }
}

void Entity::OnAttachedToNode(Node* node)
{
    Node::OnAttachedToNode(node);

    AssertDebug(node->GetScene() && node->GetScene()->GetEntityManager());

    // Handle<Entity> thisHandle = HandleFromThis();
    // EntityManager* previousEntityManager = GetEntityManager();

    // // need to move the entity between EntityManagers
    // if (previousEntityManager)
    // {
    //     if (previousEntityManager != node->GetScene()->GetEntityManager().Get())
    //     {
    //         previousEntityManager->MoveEntity(thisHandle, node->GetScene()->GetEntityManager());
    //     }
    // }
    // else
    // {
    //     // If the EntityManager for the entity is not found, we need to create a new EntityManager for it
    //     node->GetScene()->GetEntityManager()->AddExistingEntity(thisHandle);
    // }

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

void Entity::AttachChild(const Handle<Entity>& child)
{
    if (!child)
    {
        return;
    }

    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr, "EntityManager is null for Entity {} while attaching child {}", Id(), child.Id());

    Threads::AssertOnThread(entityManager->GetOwnerThreadId());

    // wont work as we currently need a SetEntity() call.
    // Node::AddChild(child);

    entityManager->AddExistingEntity(child);

    NodeLinkComponent* nodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(this);

    if (!nodeLinkComponent)
    {
        HYP_LOG(Entity, Warning, "Entity {} does not have a NodeLinkComponent, cannot attach child {}", Id(), child.Id());
        HYP_BREAKPOINT_DEBUG_MODE;
    }

    Handle<Node> node = nodeLinkComponent->node.Lock();

    if (!node)
    {
        HYP_LOG(Entity, Warning, "Entity {} has a NodeLinkComponent but the node is not valid, cannot attach child {}", Id(), child.Id());
        HYP_BREAKPOINT_DEBUG_MODE;

        return;
    }

    if (NodeLinkComponent* childNodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(child))
    {
        if (Handle<Node> childNode = childNodeLinkComponent->node.Lock())
        {
            node->AddChild(childNode);

            return;
        }
        else
        {
            childNode = node->AddChild();
            childNode->SetEntity(child);

            childNodeLinkComponent->node = childNode;

            return;
        }
    }

    Handle<Node> childNode = node->AddChild();
    childNode->SetEntity(child);

    if (NodeLinkComponent* childNodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(child))
    {
        childNodeLinkComponent->node = childNode;
    }
    else
    {
        entityManager->AddComponent<NodeLinkComponent>(child, NodeLinkComponent { childNode });
    }
}

void Entity::DetachChild(const Handle<Entity>& child)
{
    if (!child)
    {
        return;
    }

    EntityManager* entityManager = GetEntityManager();
    AssertDebug(entityManager != nullptr, "EntityManager is null for Entity {} while detaching child {}", Id(), child.Id());

    Threads::AssertOnThread(entityManager->GetOwnerThreadId());

    // Node::RemoveChild(child);

    if (NodeLinkComponent* nodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(this))
    {
        if (Handle<Node> node = nodeLinkComponent->node.Lock())
        {
            if (NodeLinkComponent* childNodeLinkComponent = entityManager->TryGetComponent<NodeLinkComponent>(child))
            {
                if (Handle<Node> childNode = childNodeLinkComponent->node.Lock())
                {
                    if (node->RemoveChild(childNode))
                    {
                        return;
                    }

                    HYP_LOG(Entity, Warning, "Failed to detach child {} node ({}) from parent's node ({})", child.Id(), childNode->GetName(), node->GetName());
                }
            }

            HYP_LOG(Entity, Warning, "Entity {} does not have a NodeLinkComponent for child {}", Id(), child.Id());
        }
        else
        {
            HYP_LOG(Entity, Warning, "Entity {} has a NodeLinkComponent but the node is not valid, cannot detach child {}", Id(), child.Id());
        }
    }
}

void Entity::SetScene(Scene* scene)
{
    if (scene == m_scene)
    {
        return;
    }

    Node::SetScene(scene);

    AssertDebug(m_scene && m_scene->GetEntityManager());

    // Move entity from previous scene to new scene
    EntityManager* previousEntityManager = GetEntityManager();

    if (previousEntityManager)
    {
        if (previousEntityManager != m_scene->GetEntityManager())
        {
            Handle<Entity> thisHandle = HandleFromThis();
            previousEntityManager->MoveEntity(thisHandle, m_scene->GetEntityManager());
        }
    }
    else
    {
        Handle<Entity> thisHandle = HandleFromThis();
        m_scene->GetEntityManager()->AddExistingEntity(thisHandle);
    }

    AssertDebug(GetEntityManager() == m_scene->GetEntityManager());
}

} // namespace hyperion
