/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

Entity::Entity()
    : m_world(nullptr),
      m_scene(nullptr)
{
}

Entity::~Entity()
{
    // Keep a WeakHandle of Entity so the ID doesn't get reused while we're using it
    EntityManager* entity_manager = GetEntityManager();
    if (entity_manager == nullptr)
    {
        return;
    }

    m_scene = nullptr;
    m_world = nullptr;

    const ID<Entity> id = GetID();

    if (!id.IsValid())
    {
        HYP_LOG(ECS, Error, "Entity has invalid ID while being destroyed!");
        return;
    }

    if (Threads::IsOnThread(entity_manager->GetOwnerThreadID()))
    {
        HYP_NAMED_SCOPE("Remove Entity from EntityManager (sync)");

        HYP_LOG(ECS, Debug, "Removing Entity {} from entity manager", id.Value());

        AssertThrow(entity_manager->HasEntity(id));

        if (!entity_manager->RemoveEntity(id))
        {
            HYP_LOG(ECS, Error, "Failed to remove Entity {} from EntityManager", id.Value());
        }
    }
    else
    {
        // If not on the correct thread, perform the removal asynchronously
        Threads::GetThread(entity_manager->GetOwnerThreadID())->GetScheduler().Enqueue([weak_this = WeakHandleFromThis(), entity_manager_weak = entity_manager->WeakHandleFromThis()]()
            {
                Handle<EntityManager> entity_manager = entity_manager_weak.Lock();
                if (!entity_manager)
                {
                    HYP_LOG(ECS, Error, "EntityManager is no longer valid while removing Entity {}", weak_this.GetID());
                    return;
                }

                HYP_NAMED_SCOPE("Remove Entity from EntityManager (async)");

                HYP_LOG(ECS, Debug, "Removing Entity {} from entity manager", weak_this.GetID().Value());

                AssertThrow(entity_manager->HasEntity(weak_this.GetID()));

                if (!entity_manager->RemoveEntity(weak_this.GetID()))
                {
                    HYP_LOG(ECS, Error, "Failed to remove Entity {} from EntityManager", weak_this.GetID().Value());
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
}

void Entity::Init()
{
    SetReady(true);
}

EntityManager* Entity::GetEntityManager() const
{
    if (!m_scene)
    {
        return nullptr;
    }

    return m_scene->GetEntityManager();
}

void Entity::SetReceivesUpdate(bool receives_update)
{
    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while setting receives update", GetID().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadID());

    if (receives_update)
    {
        entity_manager->AddTag<EntityTag::RECEIVES_UPDATE>(GetID());
    }
    else
    {
        entity_manager->RemoveTag<EntityTag::RECEIVES_UPDATE>(GetID());
    }
}

void Entity::Attach(const Handle<Node>& attach_node)
{
    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while attaching to Node", GetID().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadID());

    if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(GetID()))
    {
        if (Handle<Node> node = node_link_component->node.Lock())
        {
            if (node == attach_node)
            {
                return;
            }

            AssertDebug(node->GetEntity() == this);

            // Unset Entity first.
            node->SetEntity(Handle<Entity>::empty);
        }
    }

    // Attach() called with empty node, so just leave it as detached from any node, and return.
    if (!attach_node.IsValid())
    {
        return;
    }

    Handle<Entity> strong_this = HandleFromThis();

    Handle<Node> node = attach_node->AddChild();
    node->SetEntity(strong_this);

    if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(strong_this))
    {
        node_link_component->node = node;
    }
    else
    {
        entity_manager->AddComponent<NodeLinkComponent>(strong_this, NodeLinkComponent { node });
    }
}

void Entity::Detach()
{
    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while detaching from Node", GetID().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadID());

    if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(GetID()))
    {
        if (Handle<Node> node = node_link_component->node.Lock())
        {
            node->SetEntity(Handle<Entity>::empty);
        }
    }
}

void Entity::OnAttachedToNode(Node* node)
{
    AssertThrow(node != nullptr);

    // Do nothing in default implementation.

    // @TODO Ensure has BVHComponent if node has BUILD_BVH true
}

void Entity::OnDetachedFromNode(Node* node)
{
    AssertThrow(node != nullptr);

    // Do nothing in default implementation.
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

    EntityManager* entity_manager = nullptr;

    m_scene = scene;
}

void Entity::OnRemovedFromScene(Scene* scene)
{
    AssertDebug(scene != nullptr);
    AssertDebug(m_scene == scene);

    m_scene = nullptr;
}

void Entity::AttachChild(const Handle<Entity>& child)
{
    if (!child)
    {
        return;
    }

    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while attaching child #%u", GetID().Value(), child.GetID().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadID());

    if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(GetID()))
    {
        if (Handle<Node> node = node_link_component->node.Lock())
        {
            if (NodeLinkComponent* child_node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(child))
            {
                if (Handle<Node> child_node = child_node_link_component->node.Lock())
                {
                    node->AddChild(child_node);

                    return;
                }
                else
                {
                    child_node = node->AddChild();
                    child_node->SetEntity(child);

                    child_node_link_component->node = child_node;

                    return;
                }
            }

            Handle<Node> child_node = node->AddChild();
            child_node->SetEntity(child);

            if (NodeLinkComponent* child_node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(child))
            {
                child_node_link_component->node = child_node;
            }
            else
            {
                entity_manager->AddComponent<NodeLinkComponent>(child, NodeLinkComponent { child_node });
            }

            return;
        }

        HYP_LOG(ECS, Warning, "Entity {} has a NodeLinkComponent but the node is not valid, cannot attach child {}", GetID(), child.GetID());
    }

    HYP_LOG(ECS, Warning, "Entity {} does not have a NodeLinkComponent, cannot attach child {}", GetID(), child.GetID());
}

void Entity::DetachChild(const Handle<Entity>& child)
{
    if (!child)
    {
        return;
    }

    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while detaching child #%u", GetID().Value(), child.GetID().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadID());

    if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(GetID()))
    {
        if (Handle<Node> node = node_link_component->node.Lock())
        {
            if (NodeLinkComponent* child_node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(child))
            {
                if (Handle<Node> child_node = child_node_link_component->node.Lock())
                {
                    if (node->RemoveChild(child_node))
                    {
                        return;
                    }

                    HYP_LOG(ECS, Warning, "Failed to detach child {} node ({}) from parent's node ({})", child.GetID(), child_node->GetName(), node->GetName());
                }
            }

            HYP_LOG(ECS, Warning, "Entity {} does not have a NodeLinkComponent for child {}", GetID(), child.GetID());
        }
        else
        {
            HYP_LOG(ECS, Warning, "Entity {} has a NodeLinkComponent but the node is not valid, cannot detach child {}", GetID(), child.GetID());
        }
    }
}

} // namespace hyperion