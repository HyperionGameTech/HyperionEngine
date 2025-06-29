/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>
#include <scene/Node.hpp>
#include <scene/Mesh.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>
#include <scene/ecs/components/NodeLinkComponent.hpp>
#include <scene/ecs/components/MeshComponent.hpp>
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
    m_scene = nullptr;
    m_world = nullptr;

    // Keep a WeakHandle of Entity so the Id doesn't get reused while we're using it
    EntityManager* entity_manager = GetEntityManager();
    if (entity_manager == nullptr)
    {
        return;
    }

    if (Threads::IsOnThread(entity_manager->GetOwnerThreadId()))
    {
        HYP_NAMED_SCOPE("Remove Entity from EntityManager (sync)");

        HYP_LOG(ECS, Debug, "Removing Entity {} from entity manager", Id());

        AssertDebug(entity_manager->HasEntity(this));

        if (!entity_manager->RemoveEntity(this))
        {
            HYP_LOG(ECS, Error, "Failed to remove Entity {} from EntityManager", Id());
        }
    }
    else
    {
        // If not on the correct thread, perform the removal asynchronously
        Threads::GetThread(entity_manager->GetOwnerThreadId())->GetScheduler().Enqueue([weak_this = WeakHandleFromThis(), entity_manager_weak = entity_manager->WeakHandleFromThis()]()
            {
                Handle<EntityManager> entity_manager = entity_manager_weak.Lock();
                if (!entity_manager)
                {
                    HYP_LOG(ECS, Error, "EntityManager is no longer valid while removing Entity {}", weak_this.Id());
                    return;
                }

                HYP_NAMED_SCOPE("Remove Entity from EntityManager (async)");

                HYP_LOG(ECS, Debug, "Removing Entity {} from entity manager", weak_this.Id());

                AssertDebug(entity_manager->HasEntity(weak_this.GetUnsafe()));

                if (!entity_manager->RemoveEntity(weak_this.GetUnsafe()))
                {
                    HYP_LOG(ECS, Error, "Failed to remove Entity {} from EntityManager", weak_this.Id());
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

bool Entity::ReceivesUpdate() const
{
    if (!m_entity_init_info.can_ever_update)
    {
        return false;
    }

    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while checking receives update", Id().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadId());

    return entity_manager->HasTag<EntityTag::RECEIVES_UPDATE>(this);
}

void Entity::SetReceivesUpdate(bool receives_update)
{
    if (!m_entity_init_info.can_ever_update)
    {
        AssertDebugMsg(!receives_update, "Entity #%u cannot receive updates, but SetReceivesUpdate() was called with true", Id().Value());

        return;
    }

    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while setting receives update", Id().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadId());

    if (receives_update)
    {
        entity_manager->AddTag<EntityTag::RECEIVES_UPDATE>(this);
    }
    else
    {
        entity_manager->RemoveTag<EntityTag::RECEIVES_UPDATE>(this);
    }
}

void Entity::Attach(const Handle<Node>& attach_node)
{
    { // detach from node if already attached.
        EntityManager* entity_manager = GetEntityManager();
        AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while attaching to Node", Id().Value());

        Threads::AssertOnThread(entity_manager->GetOwnerThreadId());

        if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(this))
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
    }

    // Attach() called with empty node, so just leave it as detached from any node, and return.
    if (!attach_node.IsValid())
    {
        return;
    }

    Handle<Entity> strong_this = HandleFromThis();

    // add a subnode to the attach_node, and set the entity to this.
    Handle<Node> subnode = attach_node->AddChild();
    subnode->SetEntity(strong_this);
}

void Entity::Detach()
{
    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while detaching from Node", Id().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadId());

    if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(this))
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
}

void Entity::OnDetachedFromNode(Node* node)
{
    AssertThrow(node != nullptr);

    // Do nothing in default implementation.
}

void Entity::OnAddedToWorld(World* world)
{
    HYP_LOG(ECS, Debug, "Entity {} added to world {}", Id(), world->Id());
    AssertDebug(world != nullptr);

    m_world = world;
}

void Entity::OnRemovedFromWorld(World* world)
{
    HYP_LOG(ECS, Debug, "Entity {} removed from world {}", Id(), world->Id());

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

void Entity::OnComponentAdded(AnyRef component)
{
    if (MeshComponent* mesh_component = component.TryGet<MeshComponent>())
    {
        if (!mesh_component->mesh.IsValid())
        {
            HYP_LOG(ECS, Warning, "Entity {} has a MeshComponent with an invalid mesh", Id());

            return;
        }

        if (m_entity_init_info.bvh_depth > 0)
        {
            if (mesh_component->mesh->GetBVH().IsValid())
            {
                HYP_LOG(ECS, Debug, "Entity {} has a MeshComponent with a BVH, skipping BVH build", Id());

                return;
            }

            if (!mesh_component->mesh->BuildBVH(m_entity_init_info.bvh_depth))
            {
                HYP_LOG(ECS, Error, "Failed to build BVH for MeshComponent on Entity {}!", Id());

                return;
            }
        }
    }
}

void Entity::OnComponentRemoved(AnyRef component)
{
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

    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while attaching child #%u", Id().Value(), child.Id().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadId());

    if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(this))
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

        HYP_LOG(ECS, Warning, "Entity {} has a NodeLinkComponent but the node is not valid, cannot attach child {}", Id(), child.Id());
    }

    HYP_LOG(ECS, Warning, "Entity {} does not have a NodeLinkComponent, cannot attach child {}", Id(), child.Id());
}

void Entity::DetachChild(const Handle<Entity>& child)
{
    if (!child)
    {
        return;
    }

    EntityManager* entity_manager = GetEntityManager();
    AssertDebugMsg(entity_manager != nullptr, "EntityManager is null for Entity #%u while detaching child #%u", Id().Value(), child.Id().Value());

    Threads::AssertOnThread(entity_manager->GetOwnerThreadId());

    if (NodeLinkComponent* node_link_component = entity_manager->TryGetComponent<NodeLinkComponent>(this))
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

                    HYP_LOG(ECS, Warning, "Failed to detach child {} node ({}) from parent's node ({})", child.Id(), child_node->GetName(), node->GetName());
                }
            }

            HYP_LOG(ECS, Warning, "Entity {} does not have a NodeLinkComponent for child {}", Id(), child.Id());
        }
        else
        {
            HYP_LOG(ECS, Warning, "Entity {} has a NodeLinkComponent but the node is not valid, cannot detach child {}", Id(), child.Id());
        }
    }
}

} // namespace hyperion