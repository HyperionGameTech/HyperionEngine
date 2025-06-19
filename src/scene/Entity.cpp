/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

Entity::Entity()
    : m_world(nullptr),
      m_scene(nullptr),
      m_parent(nullptr)
{
}

Entity::~Entity()
{
    for (const Handle<Entity>& child : m_children)
    {
        // Detach all children from this entity
        child->m_parent = nullptr;
    }

    m_children.Clear();

    // Keep a WeakHandle of Entity so the ID doesn't get reused while we're using it
    EntityManager* entity_manager = GetEntityManager();
    if (entity_manager == nullptr)
    {
        return;
    }

    m_scene = nullptr;
    m_world = nullptr;
    m_parent = nullptr;

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
            });
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
    AssertThrowMsg(entity_manager != nullptr, "EntityManager is null! Entity must be added to a Scene before attaching children.");

    Threads::AssertOnThread(entity_manager->GetOwnerThreadID());

    if (child->m_parent == this)
    {
        HYP_LOG(ECS, Warning, "Entity {} is already attached to this parent, skipping attaching child to parent.", child->GetID());

        return;
    }

    if (child->m_parent != nullptr)
    {
        HYP_LOG(ECS, Warning, "Entity {} is already attached to another parent, detaching it first.", child->GetID());

        // Detach the child from its current parent
        child->m_parent->DetachChild(child);
    }

    m_children.PushBack(child->HandleFromThis());

    entity_manager->AddExistingEntity(child);
}

void Entity::DetachChild(const Handle<Entity>& child)
{
    if (!child)
    {
        return;
    }

    EntityManager* entity_manager = child->GetEntityManager();
    AssertThrowMsg(entity_manager != nullptr, "EntityManager is null! Entity must be added to a Scene before detaching children.");

    Threads::AssertOnThread(entity_manager->GetOwnerThreadID());

    if (!child->HasParent(this))
    {
        HYP_LOG(ECS, Warning, "Entity {} is not a child of this parent, skipping detaching child from parent.", child->GetID());

        return;
    }

    // To prevent erasing causing the Handle to become invalid, store a copy (increments the reference count)
    Handle<Entity> child_copy = child;

    Entity* parent = child->m_parent;
    if (parent)
    {
        auto it = parent->m_children.Find(child);
        AssertThrowMsg(it != parent->m_children.End(), "Child entity not found in parent's children list");

        parent->m_children.Erase(it);
    }

    child->m_parent = nullptr;
    entity_manager->MoveEntity(child, g_engine->GetDefaultWorld()->GetDetachedScene(Threads::CurrentThreadID())->GetEntityManager());
}

bool Entity::HasParent(const Entity* parent) const
{
    if (!parent || !m_parent)
    {
        return false;
    }

    const Entity* current_parent = m_parent;

    while (current_parent)
    {
        if (current_parent == parent)
        {
            return true;
        }

        current_parent = current_parent->m_parent;
    }

    return false;
}

} // namespace hyperion