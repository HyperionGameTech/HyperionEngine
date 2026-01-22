/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ScenePch.hpp>

#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/utilities/Format.hpp>

#include <core/reflection/Handle.hpp>
#include <core/reflection/TypeInfo.hpp>
#include <core/reflection/ClassRegistry.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <EntityManager.generated.inl>

namespace Hyperion {

// if the number of systems in a group is less than this value, they will be executed sequentially
// static constexpr double SystemExecutionGroupLagSpikeThreshold = 50.0;

// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION
// #define HYP_SYSTEM_LOG_PERFORMANCE

/// \todo : Move to ComponentContainer.cpp
#pragma region ComponentContainer

bool ComponentContainerBase::TryGetComponent(ComponentId id, BoxedValue& outComponent)
{
    if (AnyRef ref = TryGetComponent(id))
    {
        outComponent = BoxedValue(ref);

        return true;
    }

    return false;
}

#pragma endregion ComponentContainer

#pragma region EntityManager

bool EntityManager::IsValidComponentType(TypeId componentTypeId)
{
    return ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId) != nullptr;
}

bool EntityManager::IsEntityTagComponent(TypeId componentTypeId)
{
    const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

    if (!componentInterface)
    {
        return false;
    }

    return componentInterface->IsEntityTag();
}

bool EntityManager::IsEntityTagComponent(TypeId componentTypeId, EntityTag& outTag)
{
    const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

    if (!componentInterface)
    {
        return false;
    }

    if (componentInterface->IsEntityTag())
    {
        outTag = componentInterface->GetEntityTag();
        return true;
    }

    return false;
}

ANSIStringView EntityManager::GetComponentTypeName(TypeId componentTypeId)
{
    const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

    if (!componentInterface)
    {
        return ANSIStringView();
    }

    return *componentInterface->GetTypeInfo().name;
}

EntityManager::EntityManager(const ThreadId& ownerThreadId, Scene* scene, EnumFlags<EntityManagerFlags> flags)
    : m_ownerThreadId(ownerThreadId),
      m_world(scene != nullptr ? scene->GetWorld() : nullptr),
      m_scene(scene),
      m_flags(flags),
      m_isLocked(false)
{
    Assert(scene != nullptr);

    // add initial component containers
    for (const IComponentInterface* componentInterface : ComponentInterfaceRegistry::GetInstance().GetComponentInterfaces())
    {
        Assert(componentInterface != nullptr);

        ComponentContainerFactoryBase* componentContainerFactory = componentInterface->GetComponentContainerFactory();
        Assert(componentContainerFactory != nullptr);

        UniquePtr<ComponentContainerBase> componentContainer = componentContainerFactory->Create();
        Assert(componentContainer != nullptr);

        m_containers.Set(componentInterface->GetTypeInfo().id, std::move(componentContainer));
    }

    if (m_world != nullptr)
    {
        for (SystemExecutionGroup* group : m_world->GetSystemExecutionGroups())
        {
            m_systemExecutionGroups.PushBack(group);
        }
    }
}

EntityManager::~EntityManager()
{
}

void EntityManager::NotifySystemOfExistingEntities(SystemBase* system)
{
    HYP_SCOPE;

    Assert(m_world != nullptr, "EntityManager must be associated with a World before initializing systems.");

    Assert(system != nullptr);

    for (auto& subtypeData : m_entities.GetSubtypeData())
    {
        for (auto entitiesIt = subtypeData.data.Begin(); entitiesIt != subtypeData.data.End(); ++entitiesIt)
        {
            EntityData& entityData = *entitiesIt;

            Entity* entity = entityData.entityWeak.GetUnsafe();
            Assert(entity != nullptr);

            const ComponentMap& componentIds = entityData.components;

            if (system->ActsOnComponents(componentIds.Keys(), true))
            {
                { // critical section
                    Mutex::Guard guard(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(system);

                    // Check if the system already has this entity initialized
                    if (systemEntityIt != m_systemEntityMap.End() && (systemEntityIt->second.FindAs(entity) != systemEntityIt->second.End()))
                    {
                        continue;
                    }

                    m_systemEntityMap[system].Insert(entity);
                }

                system->OnEntityAdded(entity);
            }
        }
    }
}

void EntityManager::NotifySystemOfAllEntitiesRemoved(SystemBase* system)
{
    HYP_SCOPE;

    Assert(m_world != nullptr, "EntityManager must be associated with a World before shutting down systems.");

    Assert(system != nullptr);

    for (auto& subtypeData : m_entities.GetSubtypeData())
    {
        for (auto entitiesIt = subtypeData.data.Begin(); entitiesIt != subtypeData.data.End(); ++entitiesIt)
        {
            EntityData& entityData = *entitiesIt;

            Entity* entity = entityData.entityWeak.GetUnsafe();
            Assert(entity != nullptr);

            const ComponentMap& componentIds = entityData.components;

            if (system->ActsOnComponents(componentIds.Keys(), true) && IsEntityInitializedForSystem(system, entity))
            {
                { // critical section
                    Mutex::Guard guard(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(system);

                    // Check if the system already has this entity initialized
                    if (systemEntityIt != m_systemEntityMap.End() && systemEntityIt->second.Contains(entity))
                    {
                        continue;
                    }

                    systemEntityIt->second.Erase(entity);
                }

                system->OnEntityRemoved(entity);
            }
        }
    }

    system->Shutdown();
}

void EntityManager::Init()
{
    AssertOnThread(m_ownerThreadId);

    Array<SystemBase*> systems;

    for (SystemExecutionGroup* group : m_systemExecutionGroups)
    {
        for (auto& systemIt : group->GetSystems())
        {
            SystemBase* system = systemIt.second;
            Assert(system != nullptr);

            systems.PushBack(system);
        }
    }

    for (SystemBase* system : systems)
    {
        // Must be called before InitObject() is called on Systems to ensure the system is initialized if
        // other systems end up adding/removing components that trigger OnEntityAdded() or OnEntityRemoved() calls.
        system->InitComponentInfos_Internal();
    }

    if (m_world != nullptr)
    {
        for (SystemBase* system : systems)
        {
            // Initialize the system
            NotifySystemOfExistingEntities(system);
        }
    }

    SetReady(true);
}

void EntityManager::Shutdown()
{
    HYP_SCOPE;

    // Notify all entities that they're being removed from the world
    for (auto& subtypeData : m_entities.GetSubtypeData())
    {
        for (EntityData& entityData : subtypeData.data)
        {
            Entity* entity = entityData.entityWeak.GetUnsafe();
            Assert(entity != nullptr);

            // call OnComponentRemoved() for all components of the entity
            HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

            if (m_world)
            {
                entity->OnRemovedFromWorld(m_world);
            }

            entity->OnRemovedFromScene(m_scene);

            NotifySystemsOfEntityRemoved(entity, entityData.components);

            for (auto componentInfoPairIt = entityData.components.Begin(); componentInfoPairIt != entityData.components.End();)
            {
                const TypeId componentTypeId = componentInfoPairIt->first;
                const ComponentId componentId = componentInfoPairIt->second;

                auto componentContainerIt = m_containers.Find(componentTypeId);
                Assert(componentContainerIt != m_containers.End(), "Component container does not exist");
                Assert(componentContainerIt->second->HasComponent(componentId), "Component does not exist in component container");

                AnyRef componentRef = componentContainerIt->second->TryGetComponent(componentId);
                Assert(componentRef.HasValue(), "Component of type '{}' with id {} does not exist in component container", *GetComponentTypeName(componentTypeId), componentId);

                // Notify the entity that the component is being removed
                // - needed to ensure proper lifecycle. every OnComponentRemoved() call must be matched with an OnComponentAdded() call and vice versa
                EntityTag tag;
                if (IsEntityTagComponent(componentTypeId, tag))
                {
                    // Remove the tag from the entity
                    entity->OnTagRemoved(tag);
                }
                else
                {
                    entity->OnComponentRemoved(componentRef);
                }

                BoxedValue component;
                if (!componentContainerIt->second->RemoveComponent(componentId, component))
                {
                    HYP_FAIL("Failed to get component of type '{}' as BoxedValue when removing it from entity '{}'",
                        *GetComponentTypeName(componentTypeId), entity->Id());
                }

                // Update iterator, erase the component from the entity's component map
                componentInfoPairIt = entityData.components.Erase(componentInfoPairIt);
            }
        }

        if (m_world != nullptr)
        {
            Array<SystemBase*> systems;

            for (SystemExecutionGroup* group : m_systemExecutionGroups)
            {
                for (auto& systemIt : group->GetSystems())
                {
                    SystemBase*& system = systemIt.second;
                    Assert(system != nullptr);

                    systems.PushBack(system);
                }
            }

            for (SystemBase* system : systems)
            {
                // Shutdown the system
                NotifySystemOfAllEntitiesRemoved(system);
            }
        }
    }

    SetReady(false);
}

void EntityManager::SetWorld(World* world)
{
    HYP_SCOPE;

    AssertOnThread(m_ownerThreadId);

    if (world == m_world)
    {
        return;
    }

    // If EntityManager is initialized we need to notify all of our systems that the world has changed.
    Array<SystemBase*> systems;

    for (SystemExecutionGroup* group : m_systemExecutionGroups)
    {
        for (auto& systemIt : group->GetSystems())
        {
            SystemBase* system = systemIt.second;
            Assert(system != nullptr);

            systems.PushBack(system);
        }
    }

    // Call OnRemovedFromWorld() now for all entities in the EntityManager if previous world is not null
    if (m_world)
    {
        for (auto& subtypeData : m_entities.GetSubtypeData())
        {
            for (EntityData& entityData : subtypeData.data)
            {
                Entity* entity = entityData.entityWeak.GetUnsafe();
                Assert(entity != nullptr);

                entity->OnRemovedFromWorld(m_world);
            }
        }

        for (SystemBase* system : systems)
        {
            NotifySystemOfAllEntitiesRemoved(system);
        }
    }

    m_world = world;
    m_systemExecutionGroups.Clear();

    if (m_world != nullptr)
    {
        for (SystemExecutionGroup* group : m_world->GetSystemExecutionGroups())
        {
            m_systemExecutionGroups.PushBack(group);
        }

        systems.Clear();

        for (SystemExecutionGroup* group : m_systemExecutionGroups)
        {
            for (auto& systemIt : group->GetSystems())
            {
                SystemBase* system = systemIt.second;
                Assert(system != nullptr);

                systems.PushBack(system);
            }
        }

        // notify systems of entity added for the new world
        for (SystemBase* system : systems)
        {
            NotifySystemOfExistingEntities(system);
        }

        for (auto& subtypeData : m_entities.GetSubtypeData())
        {
            for (EntityData& entityData : subtypeData.data)
            {
                Entity* entity = entityData.entityWeak.GetUnsafe();
                Assert(entity != nullptr);

                entity->OnAddedToWorld(m_world);
            }
        }
    }
}

Handle<Entity> EntityManager::AddBasicEntity()
{
    HYP_SCOPE;

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    Handle<Entity> entity = MakeHandle<Entity>();

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    m_entities.Add(entity);

    entity->m_entityManager = this;
    entity->SetScene(m_scene);

    InitObject(entity);

    // Use basic TypeId tag for the entity, as the type is just Entity
    AddTag<EntityTag::EntityType>(entity);

    if (entity->m_entityInitInfo.receivesUpdate)
    {
        AddTag<EntityTag::ReceivesUpdate>(entity);
    }

    if (entity->m_entityInitInfo.initialTags.Any())
    {
        AddTags(entity, entity->m_entityInitInfo.initialTags);
    }

    entity->OnAddedToScene(m_scene);

    if (m_world)
    {
        entity->OnAddedToWorld(m_world);
    }

    return entity;
}

Handle<Entity> EntityManager::AddTypedEntity(const Class* cls)
{
    HYP_SCOPE;

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    Assert(cls != nullptr, "Class must not be null");
    Assert(cls->IsDerivedFrom(Entity::StaticClass()), "Class must be a subclass of Entity");

    BoxedValue boxed;
    if (!cls->CreateInstance(boxed))
    {
        HYP_LOG(Entity, Error, "Failed to create instance of class {}", cls->GetName());

        return Handle<Entity>::empty;
    }

    Handle<Entity> entity = std::move(boxed.Get<Handle<Entity>>());

    if (!entity.IsValid())
    {
        HYP_LOG(Entity, Error, "Failed to create instance of class {}: data does not contain a valid Entity handle", cls->GetName());

        return Handle<Entity>::empty;
    }

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    m_entities.Add(entity);

    entity->m_entityManager = this;
    entity->SetScene(m_scene);

    InitObject(entity);

    if (entity->m_entityInitInfo.receivesUpdate)
    {
        AddTag<EntityTag::ReceivesUpdate>(entity);
    }

    // Create tag to track class of the entity.

    AddTag<EntityTag::EntityType>(entity);

    while (cls != nullptr && cls != Entity::StaticClass())
    {
        EntityTag entityTypeTag = MakeEntityTypeTag(cls->GetTypeId());
        AssertDebug(uint64(entityTypeTag) & uint64(EntityTag::EntityType));

        const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(entityTypeTag);
        AssertDebug(componentInterface);

        AddTag(entity, entityTypeTag);

        cls = cls->GetParent();
    }

    if (entity->m_entityInitInfo.initialTags.Any())
    {
        AddTags(entity, entity->m_entityInitInfo.initialTags);
    }

    entity->OnAddedToScene(m_scene);

    if (m_world)
    {
        entity->OnAddedToWorld(m_world);
    }

    return entity;
}

void EntityManager::AddExistingEntity_Internal(const Handle<Entity>& entity)
{
    HYP_SCOPE;

    if (!entity.IsValid())
    {
        return;
    }

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    // Get the current EntityManager for the entity, if it exists
    EntityManager* otherEntityManager = entity->GetEntityManager();

    if (otherEntityManager)
    {
        if (otherEntityManager == this)
        {
            // Entity is already in this EntityManager, no need to add it again
            return;
        }

        // Move the Entity from the other EntityManager to this one.
        otherEntityManager->MoveEntity(entity, HandleFromThis());

        return;
    }

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    m_entities.Add(entity);

    entity->m_entityManager = this;
    entity->SetScene(m_scene);

    InitObject(entity);

    AddTag<EntityTag::EntityType>(entity);

    const Class* cls = entity->InstanceClass();

    while (cls != nullptr && cls != Entity::StaticClass())
    {
        EntityTag entityTypeTag = MakeEntityTypeTag(cls->GetTypeId());
        AssertDebug(uint64(entityTypeTag) & uint64(EntityTag::EntityType));

        const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(entityTypeTag);
        AssertDebug(componentInterface);

        AddTag(entity, entityTypeTag);

        cls = cls->GetParent();
    }

    if (entity->m_entityInitInfo.receivesUpdate)
    {
        AddTag<EntityTag::ReceivesUpdate>(entity);
    }

    if (entity->m_entityInitInfo.initialTags.Any())
    {
        AddTags(entity, entity->m_entityInitInfo.initialTags);
    }

    entity->OnAddedToScene(m_scene);

    if (m_world)
    {
        entity->OnAddedToWorld(m_world);
    }
}

/// Called from Entity destructor or from a task enqueued during Entity destructor.
/// Does not operate on the Entity pointer as it would be invalid at this point,
/// so NotifySystemsOfEntityRemoved() is not called (it's expected that this is a non-world EntityManager so it wouldn't be called anyway).
bool EntityManager::RemoveEntity(Entity* entity)
{
    HYP_SCOPE;

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    Assert(m_world == nullptr, "RemoveEntity() can only be called on non-world EntityManagers. Use MoveEntity() to move entities out of a world EntityManager on its owner thread.");

    if (!entity)
    {
        return false;
    }

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    const ObjId<Entity> entityId = entity->Id();

    // Components generically stored as BoxedValue by TypeId - to add to other EntityManager
    TypeMap<BoxedValue> components;

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    EntityData* entityData = m_entities.TryGetEntityData(entityId);
    Assert(entityData != nullptr, "Entity does not exist");

    for (auto componentInfoPairIt = entityData->components.Begin(); componentInfoPairIt != entityData->components.End();)
    {
        const TypeId componentTypeId = componentInfoPairIt->first;
        const ComponentId componentId = componentInfoPairIt->second;

        auto componentContainerIt = m_containers.Find(componentTypeId);
        Assert(componentContainerIt != m_containers.End(), "Component container does not exist");
        Assert(componentContainerIt->second->HasComponent(componentId), "Component does not exist in component container");

        AnyRef componentRef = componentContainerIt->second->TryGetComponent(componentId);
        Assert(componentRef.HasValue(), "Component of type '{}' with id {} does not exist in component container", *GetComponentTypeName(componentTypeId), componentId);

        BoxedValue component;
        if (!componentContainerIt->second->RemoveComponent(componentId, component))
        {
            HYP_FAIL("Failed to get component of type '{}' as BoxedValue when moving between EntityManagers", *GetComponentTypeName(componentTypeId));
        }

        components.Set(componentTypeId, std::move(component));

        // Update iterator, erase the component from the entity's component map
        componentInfoPairIt = entityData->components.Erase(componentInfoPairIt);
    }

    {
        for (KeyValuePair<TypeId, BoxedValue>& pair : components)
        {
            const TypeId componentTypeId = pair.first;

            // Update our entity sets to reflect the change
            auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

            if (componentEntitySetsIt != m_componentEntitySets.End())
            {
                for (EntitySetId entitySetId : componentEntitySetsIt->second)
                {
                    EntitySetBase& entitySet = *m_entitySets.At(entitySetId);

                    entitySet.RemoveEntity(entity);
                }
            }
        }
    }

    m_entities.Remove(entityId);

    return true;
}

void EntityManager::MoveEntity(const Handle<Entity>& entity, const Handle<EntityManager>& other)
{
    HYP_SCOPE;

    Assert(entity.IsValid());
    AssertDebug(entity->GetEntityManager() == this);

    Assert(other.IsValid());

    if (this == other.Get())
    {
        return;
    }

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    // Components generically stored as BoxedValue by TypeId - to add to other EntityManager
    Array<BoxedValue> components;

    { // Remove components and entity from this and store them to be added to the other EntityManager
        HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

        EntityData* entityData = m_entities.TryGetEntityData(entity.Id());
        Assert(entityData != nullptr, "Entity does not exist");

        if (m_world)
        {
            entity->OnRemovedFromWorld(m_world);
        }

        entity->OnRemovedFromScene(m_scene);

        NotifySystemsOfEntityRemoved(entity, entityData->components);

        for (auto componentInfoPairIt = entityData->components.Begin(); componentInfoPairIt != entityData->components.End();)
        {
            const TypeId componentTypeId = componentInfoPairIt->first;
            const ComponentId componentId = componentInfoPairIt->second;

            auto componentContainerIt = m_containers.Find(componentTypeId);
            Assert(componentContainerIt != m_containers.End(), "Component container does not exist");
            Assert(componentContainerIt->second->HasComponent(componentId), "Component does not exist in component container");

            AnyRef componentRef = componentContainerIt->second->TryGetComponent(componentId);
            Assert(componentRef.HasValue(), "Component of type '{}' with id {} does not exist in component container", *GetComponentTypeName(componentTypeId), componentId);

            // Notify the entity that the component is being removed
            // - needed to ensure proper lifecycle. every OnComponentRemoved() call must be matched with an OnComponentAdded() call and vice versa
            EntityTag tag;
            if (IsEntityTagComponent(componentTypeId, tag))
            {
                // Remove the tag from the entity
                entity->OnTagRemoved(tag);
            }
            else
            {
                entity->OnComponentRemoved(componentRef);
            }

            BoxedValue component;
            if (!componentContainerIt->second->RemoveComponent(componentId, component))
            {
                HYP_FAIL("Failed to get component of type '{}' as BoxedValue when moving between EntityManagers", *GetComponentTypeName(componentTypeId));
            }

            components.PushBack(std::move(component));

            // Update iterator, erase the component from the entity's component map
            componentInfoPairIt = entityData->components.Erase(componentInfoPairIt);
        }

        {
            for (const BoxedValue& component : components)
            {
                const TypeId componentTypeId = component.GetTypeId();
                EnsureValidComponentType(componentTypeId);

                // Update our entity sets to reflect the change
                auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

                if (componentEntitySetsIt != m_componentEntitySets.End())
                {
                    for (EntitySetId entitySetId : componentEntitySetsIt->second)
                    {
                        EntitySetBase& entitySet = *m_entitySets.At(entitySetId);

                        entitySet.OnEntityUpdated(entity);
                    }
                }
            }
        }

        entity->m_entityManager = nullptr;

        m_entities.Remove(entity);
    }

    // Add the entity and its components to the other EntityManager
    auto addToOtherEntityManager = [other = other, entity = entity, components = std::move(components)]() mutable
    {
        Assert(!other->IsLocked() && IsOnThread(other->m_ownerThreadId));

        // Sanity check to prevent infinite recursion from AddExistingEntity calling MoveEntity again if there is already an EntityManager set
        AssertDebug(entity->GetEntityManager() == nullptr);

        HYP_MT_CHECK_RW(other->m_entitiesDataRaceDetector);

        other->m_entities.Add(entity);

        entity->m_entityManager = other;
        entity->SetScene(other->m_scene);

        InitObject(entity);

        EntityData* entityData = other->m_entities.TryGetEntityData(entity.Id());
        Assert(entityData != nullptr, "Entity with id {} does not exist", entity.Id());

        ComponentMap componentIds;

        for (BoxedValue& component : components)
        {
            const TypeId componentTypeId = component.GetTypeId();
            EnsureValidComponentType(componentTypeId);

            // Update the EntityData
            auto componentIt = entityData->FindComponent(componentTypeId);

            if (componentIt != entityData->components.End())
            {
                if (IsEntityTagComponent(componentTypeId))
                {
                    // Duplicate of the same tag, don't worry about it

                    return;
                }

                HYP_FAIL("Cannot add duplicate component of type '{}'", *GetComponentTypeName(componentTypeId));
            }

            ComponentContainerBase* container = other->TryGetContainer(componentTypeId);
            Assert(container != nullptr, "Component container does not exist for component of type '{}'", *GetComponentTypeName(componentTypeId));

            const ComponentId componentId = container->AddComponent(std::move(component));

            componentIds.Set(componentTypeId, componentId);

            entityData->components.Set(componentTypeId, componentId);

            AnyRef componentRef = container->TryGetComponent(componentId);
            Assert(componentRef.HasValue(), "Failed to get component of type '{}' with id {} from component container", *GetComponentTypeName(componentTypeId), componentId);

            EntityTag tag;
            if (IsEntityTagComponent(componentTypeId, tag))
            {
                entity->OnTagAdded(tag);
            }
            else
            {
                // Note: Call before notifying systems as they are able to remove components!
                entity->OnComponentAdded(componentRef);
            }
        }

        {
            // Update entity sets
            for (const KeyValuePair<TypeId, ComponentId>& it : componentIds)
            {
                auto componentEntitySetsIt = other->m_componentEntitySets.Find(it.first);

                if (componentEntitySetsIt != other->m_componentEntitySets.End())
                {
                    for (EntitySetId entitySetId : componentEntitySetsIt->second)
                    {
                        EntitySetBase& entitySet = *other->m_entitySets.At(entitySetId);

                        entitySet.OnEntityUpdated(entity);
                    }
                }
            }

            componentIds = entityData->components;
        }

        entity->OnAddedToScene(other->m_scene);

        if (other->m_world)
        {
            entity->OnAddedToWorld(other->m_world);
        }

        // Notify systems that entity is being added to them
        other->NotifySystemsOfEntityAdded(entity, componentIds);
    };

    if (IsOnThread(other->GetOwnerThreadId()))
    {
        addToOtherEntityManager();
    }
    else
    {
        Task<void> task = GetThreadById(other->GetOwnerThreadId())->GetScheduler().Enqueue(std::move(addToOtherEntityManager));
        task.Await();
    }
}

void EntityManager::AddComponent(Entity* entity, const BoxedValue& componentData)
{
    AssertDebug(!componentData.IsNull());

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    Assert(entity, "Invalid entity");

    Handle<Entity> entityHandle = MakeStrongRef(entity);
    Assert(entityHandle.IsValid());

    EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
    Assert(entityData != nullptr, "Entity with id {} does not exist", entity->Id());

    const TypeId componentTypeId = componentData.GetTypeId();
    EnsureValidComponentType(componentTypeId);

    ComponentMap componentIds;

    // Update the EntityData
    auto componentIt = entityData->FindComponent(componentTypeId);

    if (componentIt != entityData->components.End())
    {
        if (IsEntityTagComponent(componentTypeId))
        {
            // Duplicate of the same tag, don't worry about it

            return;
        }

        HYP_FAIL("Cannot add duplicate component of type '{}'", *GetComponentTypeName(componentTypeId));
    }

    ComponentContainerBase* container = TryGetContainer(componentTypeId);
    Assert(container != nullptr, "Component container does not exist for component of type '{}'", *GetComponentTypeName(componentTypeId));

    const ComponentId componentId = container->AddComponent(componentData);

    entityData->components.Set(componentTypeId, componentId);

    {
        // Update entity sets
        auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

        if (componentEntitySetsIt != m_componentEntitySets.End())
        {
            for (EntitySetId entitySetId : componentEntitySetsIt->second)
            {
                EntitySetBase& entitySet = *m_entitySets.At(entitySetId);

                entitySet.OnEntityUpdated(entity);
            }
        }

        componentIds = entityData->components;
    }

    AnyRef componentRef = container->TryGetComponent(componentId);
    Assert(componentRef.HasValue(), "Failed to get component of type '{}' with id {} from component container", *GetComponentTypeName(componentTypeId), componentId);

    // Note: Call before notifying systems as they are able to remove components!

    EntityTag tag;
    if (IsEntityTagComponent(componentTypeId, tag))
    {
        entity->OnTagAdded(tag);
    }
    else
    {
        // Note: Call before notifying systems as they are able to remove components!
        entity->OnComponentAdded(componentRef);
    }

    // Notify systems that entity is being added to them
    NotifySystemsOfEntityAdded(entityHandle, componentIds);
}

void EntityManager::AddComponent(Entity* entity, BoxedValue&& componentData)
{
    AssertDebug(!componentData.IsNull());

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    Assert(entity, "Invalid entity");

    Handle<Entity> entityHandle = MakeStrongRef(entity);
    Assert(entityHandle.IsValid());

    EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
    Assert(entityData != nullptr, "Entity with id {} does not exist", entity->Id());

    const TypeId componentTypeId = componentData.GetTypeId();
    EnsureValidComponentType(componentTypeId);

    ComponentMap componentIds;

    // Update the EntityData
    auto componentIt = entityData->FindComponent(componentTypeId);

    if (componentIt != entityData->components.End())
    {
        if (IsEntityTagComponent(componentTypeId))
        {
            // Duplicate of the same tag, don't worry about it

            return;
        }

        HYP_FAIL("Cannot add duplicate component of type '{}'", *GetComponentTypeName(componentTypeId));
    }

    ComponentContainerBase* container = TryGetContainer(componentTypeId);
    Assert(container != nullptr, "Component container does not exist for component of type '{}'", *GetComponentTypeName(componentTypeId));

    const ComponentId componentId = container->AddComponent(std::move(componentData));

    entityData->components.Set(componentTypeId, componentId);

    {
        // Update entity sets
        auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

        if (componentEntitySetsIt != m_componentEntitySets.End())
        {
            for (EntitySetId entitySetId : componentEntitySetsIt->second)
            {
                EntitySetBase& entitySet = *m_entitySets.At(entitySetId);

                entitySet.OnEntityUpdated(entity);
            }
        }

        componentIds = entityData->components;
    }

    AnyRef componentRef = container->TryGetComponent(componentId);
    Assert(componentRef.HasValue(), "Failed to get component of type '{}' with id {} from component container", *GetComponentTypeName(componentTypeId), componentId);

    EntityTag tag;
    if (IsEntityTagComponent(componentTypeId, tag))
    {
        entity->OnTagAdded(tag);
    }
    else
    {
        // Note: Call before notifying systems as they are able to remove components!
        entity->OnComponentAdded(componentRef);
    }

    // Notify systems that entity is being added to them
    NotifySystemsOfEntityAdded(entityHandle, componentIds);
}

bool EntityManager::RemoveComponent(TypeId componentTypeId, Entity* entity)
{
    HYP_SCOPE;
    EnsureValidComponentType(componentTypeId);

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    if (!entity)
    {
        return false;
    }

    HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);

    EntityData* entityData = m_entities.TryGetEntityData(entity->Id());

    if (!entityData)
    {
        return false;
    }

    auto componentIt = entityData->FindComponent(componentTypeId);
    if (componentIt == entityData->components.End())
    {
        return false;
    }

    const ComponentId componentId = componentIt->second;

    // Notify systems that entity is being removed from them
    ComponentMap removedComponents;
    removedComponents.Set(componentTypeId, componentId);

    NotifySystemsOfEntityRemoved(entity, removedComponents);

    ComponentContainerBase* container = TryGetContainer(componentTypeId);

    if (!container)
    {
        return false;
    }

    AnyRef componentRef = container->TryGetComponent(componentId);

    if (!componentRef.HasValue())
    {
        HYP_LOG(Entity, Error, "Failed to get component of type '{}' with Id {} for entity #{}", *GetComponentTypeName(componentTypeId), componentId, entity->Id());

        return false;
    }

    EntityTag tag;
    if (IsEntityTagComponent(componentTypeId, tag))
    {
        entity->OnTagRemoved(tag);
    }
    else
    {
        entity->OnComponentRemoved(componentRef);
    }

    if (!container->RemoveComponent(componentId))
    {
        return false;
    }

    entityData->components.Erase(componentIt);

    auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

    if (componentEntitySetsIt != m_componentEntitySets.End())
    {
        for (EntitySetId entitySetId : componentEntitySetsIt->second)
        {
            EntitySetBase& entitySet = *m_entitySets.At(entitySetId);

            entitySet.OnEntityUpdated(entity);
        }
    }

    return true;
}

bool EntityManager::HasTag(const Entity* entity, EntityTag tag) const
{
    HYP_SCOPE;

    Assert(IsLocked() || IsOnThread(m_ownerThreadId));

    if (!entity)
    {
        return false;
    }

    if (IsEntityTypeTag(tag))
    {
        const EntityData* entityData = m_entities.TryGetEntityData(entity->Id());

        if (!entityData)
        {
            return false;
        }

        return GetTypeIdFromEntityTag(tag) == entityData->entityWeak.GetTypeId();
    }

    const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!componentInterface)
    {
        HYP_LOG(Entity, Error, "No TagComponent registered for EntityTag {}", tag);

        return false;
    }

    const TypeInfo& componentTypeInfo = componentInterface->GetTypeInfo();

    return HasComponent(componentTypeInfo.id, entity);
}

void EntityManager::AddTag(Entity* entity, EntityTag tag)
{
    HYP_SCOPE;

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    if (!entity)
    {
        return;
    }

    Handle<Entity> entityHandle = MakeStrongRef(entity);
    Assert(entityHandle.IsValid());

    const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!componentInterface)
    {
        HYP_LOG(Entity, Error, "No TagComponent registered for EntityTag {}", tag);

        return;
    }

    const TypeInfo& componentTypeInfo = componentInterface->GetTypeInfo();

    if (HasComponent(componentTypeInfo.id, entity))
    {
        return;
    }

    ComponentContainerBase* container = TryGetContainer(componentTypeInfo.id);
    Assert(container != nullptr, "Component container does not exist for component type {}", componentTypeInfo.name);

    BoxedValue component;

    if (!componentInterface->CreateInstance(component))
    {
        HYP_LOG(Entity, Error, "Failed to create TagComponent for EntityTag {}", tag);

        return;
    }

    AddComponent(entity, std::move(component));
}

bool EntityManager::RemoveTag(Entity* entity, EntityTag tag)
{
    HYP_SCOPE;

    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    if (!entity)
    {
        return false;
    }

    const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!componentInterface)
    {
        HYP_LOG(Entity, Error, "No TagComponent registered for EntityTag {}", tag);

        return false;
    }

    const TypeInfo& componentTypeInfo = componentInterface->GetTypeInfo();

    return RemoveComponent(componentTypeInfo.id, entity);
}

void EntityManager::NotifySystemsOfEntityAdded(const Handle<Entity>& entity, const ComponentMap& componentIds)
{
    HYP_SCOPE;

    if (!entity.IsValid())
    {
        return;
    }

    // If the EntityManager is initialized, notify systems of the entity being added
    // otherwise, the systems will be notified when the EntityManager is initialized
    if (!IsInitCalled() || m_world == nullptr)
    {
        return;
    }

    for (SystemExecutionGroup* group : m_systemExecutionGroups)
    {
        for (auto& systemIt : group->GetSystems())
        {
            if (systemIt.second->ActsOnComponents(componentIds.Keys(), true))
            {
                { // critical section
                    Mutex::Guard guard(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(systemIt.second);

                    // Check if the system already has this entity initialized
                    if (systemEntityIt != m_systemEntityMap.End() && (systemEntityIt->second.Find(entity.Get()) != systemEntityIt->second.End()))
                    {
                        continue;
                    }

                    m_systemEntityMap[systemIt.second].Insert(entity);
                }

                systemIt.second->OnEntityAdded(entity);
            }
        }
    }
}

HYP_DISABLE_OPTIMIZATION;
void EntityManager::NotifySystemsOfEntityRemoved(Entity* entity, const ComponentMap& componentIds)
{
    HYP_SCOPE;

    if (!entity)
    {
        return;
    }

    if (!IsInitCalled() || m_world == nullptr)
    {
        return;
    }

    WeakHandle<Entity> entityWeak = MakeWeakRef(entity);

    for (SystemExecutionGroup* group : m_systemExecutionGroups)
    {
        for (auto& systemIt : group->GetSystems())
        {
            if (systemIt.second->ActsOnComponents(componentIds.Keys(), true))
            {
                { // critical section
                    Mutex::Guard guard(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(systemIt.second);

                    if (systemEntityIt == m_systemEntityMap.End())
                    {
                        continue;
                    }

                    auto entityIt = systemEntityIt->second.Find(entity);

                    if (entityIt == systemEntityIt->second.End())
                    {
                        continue;
                    }

                    systemEntityIt->second.Erase(entityIt);
                }

                systemIt.second->OnEntityRemoved(entity);
            }
        }
    }
}

void EntityManager::UpdateEntities(float delta)
{
    HYP_SCOPE;
    AssertOnThread(m_ownerThreadId);

    AssertDebug(GetWorld() != nullptr);

    for (auto [entity, _] : GetEntitySet<TagComponent<EntityTag::ReceivesUpdate>>().GetScopedView(DataAccessFlags::ACCESS_RW))
    {
        AssertDebug(entity->GetEntityManager() == this);
        AssertDebug(entity->GetWorld() == GetWorld());

        entity->Update(delta);
    }
}

void EntityManager::AddPendingEntitySets()
{
    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    Mutex::Guard guard(m_pendingEntitySetsMtx);

    for (auto& kvp : m_pendingEntitySets)
    {
        const EntitySetId entitySetId = kvp.first;
        UniquePtr<EntitySetBase>& entitySetPtr = kvp.second;

        AssertDebug(!m_entitySets.Contains(entitySetId));

        for (TypeId componentTypeId : entitySetPtr->GetComponentTypeIds())
        {
            auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

            if (componentEntitySetsIt == m_componentEntitySets.End())
            {
                auto componentEntitySetsInsertResult = m_componentEntitySets.Set(componentTypeId, {});

                componentEntitySetsIt = componentEntitySetsInsertResult.first;
            }

            componentEntitySetsIt->second.Insert(entitySetId);
        }

        m_entitySets.Insert(entitySetId, std::move(entitySetPtr));
    }

    m_pendingEntitySets.Clear();
}

bool EntityManager::IsEntityInitializedForSystem(SystemBase* system, const Entity* entity) const
{
    HYP_SCOPE;

    if (!system)
    {
        return false;
    }

    Mutex::Guard guard(m_systemEntityMapMutex);

    const auto it = m_systemEntityMap.Find(system);

    if (it == m_systemEntityMap.End())
    {
        return false;
    }

    return it->second.FindAs(entity) != it->second.End();
}

#pragma endregion EntityManager

} // namespace Hyperion
