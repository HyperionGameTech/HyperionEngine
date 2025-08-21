/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/EntityManager.hpp>
#include <scene/ComponentInterface.hpp>

#include <scene/Entity.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <core/object/Handle.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/utilities/Format.hpp>

#include <core/object/HypClassRegistry.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/object/HypClassUtils.hpp>
#include <core/object/HypData.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

// if the number of systems in a group is less than this value, they will be executed sequentially
static constexpr double g_systemExecutionGroupLagSpikeThreshold = 50.0;
static constexpr uint32 g_entityManagerCommandQueueWarningSize = 8192;

#define HYP_SYSTEMS_PARALLEL_EXECUTION
// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION
#define HYP_SYSTEM_LOG_PERFORMANCE

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

    return componentInterface->GetTypeName();
}

EntityManager::EntityManager(const ThreadId& ownerThreadId, Scene* scene, EnumFlags<EntityManagerFlags> flags)
    : m_ownerThreadId(ownerThreadId),
      m_world(scene != nullptr ? scene->GetWorld() : nullptr),
      m_scene(scene),
      m_flags(flags),
      m_rootSynchronousExecutionGroup(nullptr)
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

        m_containers.Set(componentInterface->GetTypeId(), std::move(componentContainer));
    }
}

EntityManager::~EntityManager()
{
}

void EntityManager::InitializeSystem(const Handle<SystemBase>& system)
{
    HYP_SCOPE;

    Assert(m_world != nullptr, "EntityManager must be associated with a World before initializing systems.");

    Assert(system.IsValid());

    InitObject(system);

    for (auto& subtypeData : m_entities.GetSubtypeData())
    {
        for (auto entitiesIt = subtypeData.data.Begin(); entitiesIt != subtypeData.data.End(); ++entitiesIt)
        {
            EntityData& entityData = *entitiesIt;

            Entity* entity = entityData.entityWeak.GetUnsafe();
            Assert(entity != nullptr);

            const TypeMap<ComponentId>& componentIds = entityData.components;

            if (system->ActsOnComponents(componentIds.Keys(), true))
            {
                { // critical section
                    Mutex::Guard guard(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(system.Get());

                    // Check if the system already has this entity initialized
                    if (systemEntityIt != m_systemEntityMap.End() && (systemEntityIt->second.FindAs(entity) != systemEntityIt->second.End()))
                    {
                        continue;
                    }

                    m_systemEntityMap[system.Get()].Insert(entity);
                }

                system->OnEntityAdded(entity);
            }
        }
    }
}

void EntityManager::ShutdownSystem(const Handle<SystemBase>& system)
{
    HYP_SCOPE;

    Assert(m_world != nullptr, "EntityManager must be associated with a World before shutting down systems.");

    Assert(system.IsValid());

    for (auto& subtypeData : m_entities.GetSubtypeData())
    {
        for (auto entitiesIt = subtypeData.data.Begin(); entitiesIt != subtypeData.data.End(); ++entitiesIt)
        {
            EntityData& entityData = *entitiesIt;

            Entity* entity = entityData.entityWeak.GetUnsafe();
            Assert(entity != nullptr);

            const TypeMap<ComponentId>& componentIds = entityData.components;

            if (system->ActsOnComponents(componentIds.Keys(), true) && IsEntityInitializedForSystem(system.Get(), entity))
            {
                { // critical section
                    Mutex::Guard guard(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(system.Get());

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
    Threads::AssertOnThread(m_ownerThreadId);

    // Notify all entities that they've been added to the world
    for (auto& subtypeData : m_entities.GetSubtypeData())
    {
        for (EntityData& entityData : subtypeData.data)
        {
            Entity* entity = entityData.entityWeak.GetUnsafe();
            Assert(entity != nullptr);

            entity->OnAddedToScene(m_scene);

            if (m_world)
            {
                entity->OnAddedToWorld(m_world);
            }
        }
    }

    Array<Handle<SystemBase>> systems;

    for (SystemExecutionGroup& group : m_systemExecutionGroups)
    {
        for (auto& systemIt : group.GetSystems())
        {
            const Handle<SystemBase>& system = systemIt.second;
            Assert(system.IsValid());

            systems.PushBack(system);
        }
    }

    for (const Handle<SystemBase>& system : systems)
    {
        // Must be called before InitObject() is called on Systems to ensure the system is initialized if
        // other systems end up adding/removing components that trigger OnEntityAdded() or OnEntityRemoved() calls.
        system->InitComponentInfos_Internal();
    }

    if (m_world != nullptr)
    {
        for (const Handle<SystemBase>& system : systems)
        {
            // Initialize the system
            InitializeSystem(system);
        }
    }

    SetReady(true);
}

void EntityManager::Shutdown()
{
    HYP_SCOPE;

    if (IsInitCalled())
    {
        // Notify all entities that they're being removed from the world
        for (auto& subtypeData : m_entities.GetSubtypeData())
        {
            for (EntityData& entityData : subtypeData.data)
            {
                Entity* entity = entityData.entityWeak.GetUnsafe();
                Assert(entity != nullptr);

                // call OnComponentRemoved() for all components of the entity
                HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

                NotifySystemsOfEntityRemoved(entity, entityData.components);

                for (auto componentInfoPairIt = entityData.components.Begin(); componentInfoPairIt != entityData.components.End();)
                {
                    const TypeId componentTypeId = componentInfoPairIt->first;
                    const ComponentId componentId = componentInfoPairIt->second;

                    auto componentContainerIt = m_containers.Find(componentTypeId);
                    Assert(componentContainerIt != m_containers.End(), "Component container does not exist");
                    Assert(componentContainerIt->second->HasComponent(componentId), "Component does not exist in component container");

                    AnyRef componentRef = componentContainerIt->second->TryGetComponent(componentId);
                    Assert(componentRef.HasValue(), "Component of type '%s' with id {} does not exist in component container", *GetComponentTypeName(componentTypeId), componentId);

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

                    HypData componentHypData;
                    if (!componentContainerIt->second->RemoveComponent(componentId, componentHypData))
                    {
                        HYP_FAIL("Failed to get component of type '%s' as HypData when removing it from entity '{}'",
                            *GetComponentTypeName(componentTypeId), entity->Id());
                    }

                    // Update iterator, erase the component from the entity's component map
                    componentInfoPairIt = entityData.components.Erase(componentInfoPairIt);
                }

                if (m_world)
                {
                    entity->OnRemovedFromWorld(m_world);
                }

                entity->OnRemovedFromScene(m_scene);
            }
        }

        if (m_world != nullptr)
        {
            Array<Handle<SystemBase>> systems;

            for (SystemExecutionGroup& group : m_systemExecutionGroups)
            {
                for (auto& systemIt : group.GetSystems())
                {
                    const Handle<SystemBase>& system = systemIt.second;
                    Assert(system.IsValid());

                    systems.PushBack(system);
                }
            }

            for (const Handle<SystemBase>& system : systems)
            {
                // Shutdown the system
                ShutdownSystem(system);
            }
        }
    }

    SetReady(false);
}

void EntityManager::SetWorld(World* world)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_ownerThreadId);

    if (world == m_world)
    {
        return;
    }

    // If EntityManager is initialized we need to notify all of our systems that the world has changed.
    if (IsInitCalled())
    {
        Array<Handle<SystemBase>> systems;

        for (SystemExecutionGroup& group : m_systemExecutionGroups)
        {
            for (auto& systemIt : group.GetSystems())
            {
                const Handle<SystemBase>& system = systemIt.second;
                Assert(system.IsValid());

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

            for (const Handle<SystemBase>& system : systems)
            {
                ShutdownSystem(system);
            }
        }

        m_world = world;

        if (m_world != nullptr)
        { // notify systems of entity added for the new world
            for (const Handle<SystemBase>& system : systems)
            {
                InitializeSystem(system);
            }

            for (auto& subtypeData : m_entities.GetSubtypeData())
            {
                for (EntityData& entityData : subtypeData.data)
                {
                    Entity* entity = entityData.entityWeak.GetUnsafe();
                    Assert(entity != nullptr);

                    HYP_LOG(Entity, Debug, "Calling OnAddedToWorld() for Entity {} (subclass idx: {})",
                        entity->Id(), m_entities.GetSubtypeData().IndexOf(&subtypeData));

                    entity->OnAddedToWorld(m_world);
                }
            }
        }

        return;
    }

    m_world = world;
}

Handle<Entity> EntityManager::AddBasicEntity()
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

    Handle<Entity> entity = CreateObject<Entity>();

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    m_entities.Add(entity);

    entity->m_entityManager = this;
    entity->SetScene(m_scene);

    InitObject(entity);

    // Use basic TypeId tag for the entity, as the type is just Entity
    AddTag<EntityTag::TYPE_ID>(entity);

    if (entity->m_entityInitInfo.receivesUpdate)
    {
        AddTag<EntityTag::RECEIVES_UPDATE>(entity);
    }

    if (entity->m_entityInitInfo.initialTags.Any())
    {
        AddTags(entity, entity->m_entityInitInfo.initialTags);
    }

    if (IsInitCalled())
    {
        entity->OnAddedToScene(m_scene);

        if (m_world)
        {
            entity->OnAddedToWorld(m_world);
        }
    }

    return entity;
}

Handle<Entity> EntityManager::AddTypedEntity(const HypClass* hypClass)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

    Assert(hypClass != nullptr, "HypClass must not be null");
    Assert(hypClass->IsDerivedFrom(Entity::Class()), "HypClass must be a subclass of Entity");

    HypData data;
    if (!hypClass->CreateInstance(data))
    {
        HYP_LOG(Entity, Error, "Failed to create instance of class {}", hypClass->GetName());

        return Handle<Entity>::empty;
    }

    Handle<Entity> entity = std::move(data.Get<Handle<Entity>>());

    if (!entity.IsValid())
    {
        HYP_LOG(Entity, Error, "Failed to create instance of class {}: data does not contain a valid Entity handle", hypClass->GetName());

        return Handle<Entity>::empty;
    }

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    m_entities.Add(entity);

    entity->m_entityManager = this;
    entity->SetScene(m_scene);

    InitObject(entity);

    if (entity->m_entityInitInfo.receivesUpdate)
    {
        AddTag<EntityTag::RECEIVES_UPDATE>(entity);
    }

    // Create tag to track class of the entity.

    AddTag<EntityTag::TYPE_ID>(entity);

    while (hypClass != nullptr && hypClass != Entity::Class())
    {
        EntityTag entityTypeTag = MakeEntityTypeTag(hypClass->GetTypeId());
        AssertDebug(uint64(entityTypeTag) & uint64(EntityTag::TYPE_ID));

        const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(entityTypeTag);
        AssertDebug(componentInterface);

        AddTag(entity, entityTypeTag);

        hypClass = hypClass->GetParent();
    }

    if (entity->m_entityInitInfo.initialTags.Any())
    {
        AddTags(entity, entity->m_entityInitInfo.initialTags);
    }

    if (IsInitCalled())
    {
        entity->OnAddedToScene(m_scene);

        if (m_world)
        {
            entity->OnAddedToWorld(m_world);
        }
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

    Threads::AssertOnThread(m_ownerThreadId);

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

    AddTag<EntityTag::TYPE_ID>(entity);

    const HypClass* hypClass = entity->InstanceClass();

    while (hypClass != nullptr && hypClass != Entity::Class())
    {
        EntityTag entityTypeTag = MakeEntityTypeTag(hypClass->GetTypeId());
        AssertDebug(uint64(entityTypeTag) & uint64(EntityTag::TYPE_ID));

        const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(entityTypeTag);
        AssertDebug(componentInterface);

        AddTag(entity, entityTypeTag);

        hypClass = hypClass->GetParent();
    }

    if (entity->m_entityInitInfo.receivesUpdate)
    {
        AddTag<EntityTag::RECEIVES_UPDATE>(entity);
    }

    if (entity->m_entityInitInfo.initialTags.Any())
    {
        AddTags(entity, entity->m_entityInitInfo.initialTags);
    }

    if (IsInitCalled())
    {
        entity->OnAddedToScene(m_scene);

        if (m_world)
        {
            entity->OnAddedToWorld(m_world);
        }
    }
}

/// Called from Entity destructor or from a task enqueued during Entity destructor.
/// Does not operate on the Entity pointer as it would be invalid at this point,
/// so NotifySystemsOfEntityRemoved() is not called (it's expected that this is a non-world EntityManager so it wouldn't be called anyway).
bool EntityManager::RemoveEntity(ObjId<Entity> entityId)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

    Assert(m_world == nullptr, "RemoveEntity() can only be called on non-world EntityManagers. Use MoveEntity() to move entities out of a world EntityManager on its owner thread.");

    if (!entityId.IsValid())
    {
        return false;
    }

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    // Components generically stored as HypData by TypeId - to add to other EntityManager
    TypeMap<HypData> componentHypDatas;

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

        HypData componentHypData;
        if (!componentContainerIt->second->RemoveComponent(componentId, componentHypData))
        {
            HYP_FAIL("Failed to get component of type '{}' as HypData when moving between EntityManagers", *GetComponentTypeName(componentTypeId));
        }

        componentHypDatas.Set(componentTypeId, std::move(componentHypData));

        // Update iterator, erase the component from the entity's component map
        componentInfoPairIt = entityData->components.Erase(componentInfoPairIt);
    }

    {
        Mutex::Guard entitySetsGuard(m_entitySetsMutex);

        for (KeyValuePair<TypeId, HypData>& it : componentHypDatas)
        {
            const TypeId componentTypeId = it.first;

            // Update our entity sets to reflect the change
            auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

            if (componentEntitySetsIt != m_componentEntitySets.End())
            {
                for (TypeId entitySetTypeId : componentEntitySetsIt->second)
                {
                    EntitySetBase& entitySet = *m_entitySets.At(entitySetTypeId);

                    entitySet.RemoveEntity(entityId);
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

    Threads::AssertOnThread(m_ownerThreadId);

    // Components generically stored as HypData by TypeId - to add to other EntityManager
    Array<HypData> componentHypDatas;

    { // Remove components and entity from this and store them to be added to the other EntityManager
        HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

        EntityData* entityData = m_entities.TryGetEntityData(entity.Id());
        Assert(entityData != nullptr, "Entity does not exist");

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

            HypData componentHypData;
            if (!componentContainerIt->second->RemoveComponent(componentId, componentHypData))
            {
                HYP_FAIL("Failed to get component of type '{}' as HypData when moving between EntityManagers", *GetComponentTypeName(componentTypeId));
            }

            componentHypDatas.PushBack(std::move(componentHypData));

            // Update iterator, erase the component from the entity's component map
            componentInfoPairIt = entityData->components.Erase(componentInfoPairIt);
        }

        if (IsInitCalled())
        {
            if (m_world)
            {
                entity->OnRemovedFromWorld(m_world);
            }

            entity->OnRemovedFromScene(m_scene);
        }

        {
            Mutex::Guard entitySetsGuard(m_entitySetsMutex);

            for (const HypData& componentData : componentHypDatas)
            {
                const TypeId componentTypeId = componentData.GetTypeId();
                EnsureValidComponentType(componentTypeId);

                // Update our entity sets to reflect the change
                auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

                if (componentEntitySetsIt != m_componentEntitySets.End())
                {
                    for (TypeId entitySetTypeId : componentEntitySetsIt->second)
                    {
                        EntitySetBase& entitySet = *m_entitySets.At(entitySetTypeId);

                        entitySet.OnEntityUpdated(entity);
                    }
                }
            }
        }

        entity->m_entityManager = nullptr;

        m_entities.Remove(entity);
    }

    // Add the entity and its components to the other EntityManager
    auto addToOtherEntityManager = [other = other, entity = entity, componentHypDatas = std::move(componentHypDatas)]() mutable
    {
        Threads::AssertOnThread(other->GetOwnerThreadId());

        // Sanity check to prevent infinite recursion from AddExistingEntity calling MoveEntity again if there is already an EntityManager set
        AssertDebug(entity->GetEntityManager() == nullptr);

        HYP_MT_CHECK_RW(other->m_entitiesDataRaceDetector);

        other->m_entities.Add(entity);

        entity->m_entityManager = other;
        entity->SetScene(other->m_scene);

        InitObject(entity);

        EntityData* entityData = other->m_entities.TryGetEntityData(entity.Id());
        Assert(entityData != nullptr, "Entity with id {} does not exist", entity.Id());

        TypeMap<ComponentId> componentIds;

        for (HypData& componentData : componentHypDatas)
        {
            const TypeId componentTypeId = componentData.GetTypeId();
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

            const ComponentId componentId = container->AddComponent(std::move(componentData));

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
            Mutex::Guard entitySetsGuard(other->m_entitySetsMutex);

            // Update entity sets
            for (const KeyValuePair<TypeId, ComponentId>& it : componentIds)
            {
                auto componentEntitySetsIt = other->m_componentEntitySets.Find(it.first);

                if (componentEntitySetsIt != other->m_componentEntitySets.End())
                {
                    for (TypeId entitySetTypeId : componentEntitySetsIt->second)
                    {
                        EntitySetBase& entitySet = *other->m_entitySets.At(entitySetTypeId);

                        entitySet.OnEntityUpdated(entity);
                    }
                }
            }

            componentIds = entityData->components;
        }

        if (other->IsInitCalled())
        {
            entity->OnAddedToScene(other->m_scene);

            if (other->m_world)
            {
                entity->OnAddedToWorld(other->m_world);
            }
        }

        // Notify systems that entity is being added to them
        other->NotifySystemsOfEntityAdded(entity, componentIds);
    };

    if (Threads::IsOnThread(other->GetOwnerThreadId()))
    {
        addToOtherEntityManager();
    }
    else
    {
        Task<void> task = Threads::GetThread(other->GetOwnerThreadId())->GetScheduler().Enqueue(std::move(addToOtherEntityManager));
        task.Await();
    }
}

void EntityManager::AddComponent(Entity* entity, const HypData& componentData)
{
    AssertDebug(!componentData.IsNull());

    Threads::AssertOnThread(m_ownerThreadId);

    Assert(entity, "Invalid entity");

    Handle<Entity> entityHandle = MakeStrongRef(entity);
    Assert(entityHandle.IsValid());

    EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
    Assert(entityData != nullptr, "Entity with id {} does not exist", entity->Id());

    const TypeId componentTypeId = componentData.GetTypeId();
    EnsureValidComponentType(componentTypeId);

    TypeMap<ComponentId> componentIds;

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
        Mutex::Guard entitySetsGuard(m_entitySetsMutex);

        // Update entity sets
        auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

        if (componentEntitySetsIt != m_componentEntitySets.End())
        {
            for (TypeId entitySetTypeId : componentEntitySetsIt->second)
            {
                EntitySetBase& entitySet = *m_entitySets.At(entitySetTypeId);

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

void EntityManager::AddComponent(Entity* entity, HypData&& componentData)
{
    AssertDebug(!componentData.IsNull());

    Threads::AssertOnThread(m_ownerThreadId);

    Assert(entity, "Invalid entity");

    Handle<Entity> entityHandle = MakeStrongRef(entity);
    Assert(entityHandle.IsValid());

    EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
    Assert(entityData != nullptr, "Entity with id {} does not exist", entity->Id());

    const TypeId componentTypeId = componentData.GetTypeId();
    EnsureValidComponentType(componentTypeId);

    TypeMap<ComponentId> componentIds;

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
        Mutex::Guard entitySetsGuard(m_entitySetsMutex);

        // Update entity sets
        auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

        if (componentEntitySetsIt != m_componentEntitySets.End())
        {
            for (TypeId entitySetTypeId : componentEntitySetsIt->second)
            {
                EntitySetBase& entitySet = *m_entitySets.At(entitySetTypeId);

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
    Threads::AssertOnThread(m_ownerThreadId);

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
    TypeMap<ComponentId> removedComponentIds;
    removedComponentIds.Set(componentTypeId, componentId);

    NotifySystemsOfEntityRemoved(entity, removedComponentIds);

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

    // Lock the entity sets mutex
    Mutex::Guard entitySetsGuard(m_entitySetsMutex);

    auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

    if (componentEntitySetsIt != m_componentEntitySets.End())
    {
        for (TypeId entitySetTypeId : componentEntitySetsIt->second)
        {
            EntitySetBase& entitySet = *m_entitySets.At(entitySetTypeId);

            entitySet.OnEntityUpdated(entity);
        }
    }

    return true;
}

bool EntityManager::HasTag(const Entity* entity, EntityTag tag) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

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
        HYP_LOG(Entity, Error, "No EntityTagComponent registered for EntityTag {}", tag);

        return false;
    }

    const TypeId componentTypeId = componentInterface->GetTypeId();

    return HasComponent(componentTypeId, entity);
}

void EntityManager::AddTag(Entity* entity, EntityTag tag)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_ownerThreadId);

    if (!entity)
    {
        return;
    }

    Handle<Entity> entityHandle = MakeStrongRef(entity);
    Assert(entityHandle.IsValid());

    const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!componentInterface)
    {
        HYP_LOG(Entity, Error, "No EntityTagComponent registered for EntityTag {}", tag);

        return;
    }

    const TypeId componentTypeId = componentInterface->GetTypeId();

    if (HasComponent(componentTypeId, entity))
    {
        return;
    }

    ComponentContainerBase* container = TryGetContainer(componentTypeId);
    Assert(container != nullptr, "Component container does not exist for component type {}", componentTypeId.Value());

    HypData componentHypData;

    if (!componentInterface->CreateInstance(componentHypData))
    {
        HYP_LOG(Entity, Error, "Failed to create EntityTagComponent for EntityTag {}", tag);

        return;
    }

    AddComponent(entity, std::move(componentHypData));
}

bool EntityManager::RemoveTag(Entity* entity, EntityTag tag)
{
    HYP_SCOPE;

    Threads::AssertOnThread(m_ownerThreadId);

    if (!entity)
    {
        return false;
    }

    const IComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!componentInterface)
    {
        HYP_LOG(Entity, Error, "No EntityTagComponent registered for EntityTag {}", tag);

        return false;
    }

    const TypeId componentTypeId = componentInterface->GetTypeId();

    return RemoveComponent(componentTypeId, entity);
}

void EntityManager::NotifySystemsOfEntityAdded(const Handle<Entity>& entity, const TypeMap<ComponentId>& componentIds)
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

    for (SystemExecutionGroup& group : m_systemExecutionGroups)
    {
        for (auto& systemIt : group.GetSystems())
        {
            if (systemIt.second->ActsOnComponents(componentIds.Keys(), true))
            {
                { // critical section
                    Mutex::Guard guard(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(systemIt.second.Get());

                    // Check if the system already has this entity initialized
                    if (systemEntityIt != m_systemEntityMap.End() && (systemEntityIt->second.Find(entity.Get()) != systemEntityIt->second.End()))
                    {
                        continue;
                    }

                    m_systemEntityMap[systemIt.second.Get()].Insert(entity);
                }

                systemIt.second->OnEntityAdded(entity);
            }
        }
    }
}

void EntityManager::NotifySystemsOfEntityRemoved(Entity* entity, const TypeMap<ComponentId>& componentIds)
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

    for (SystemExecutionGroup& group : m_systemExecutionGroups)
    {
        for (auto& systemIt : group.GetSystems())
        {
            if (systemIt.second->ActsOnComponents(componentIds.Keys(), true))
            {
                { // critical section
                    Mutex::Guard guard(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(systemIt.second.Get());

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

void EntityManager::BeginAsyncUpdate(float delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

    AssertDebug(GetWorld() != nullptr);

    for (auto [entity, _] : GetEntitySet<EntityTagComponent<EntityTag::RECEIVES_UPDATE>>().GetScopedView(DataAccessFlags::ACCESS_RW))
    {
        AssertDebug(entity->GetEntityManager() == this);
        AssertDebug(entity->GetWorld() == GetWorld());
        
        entity->Update(delta);
    }

    m_rootSynchronousExecutionGroup = nullptr;

    TaskBatch* rootTaskBatch = nullptr;
    TaskBatch* lastTaskBatch = nullptr;

    // Prepare task dependencies
    for (SizeType index = 0; index < m_systemExecutionGroups.Size(); index++)
    {
        SystemExecutionGroup& systemExecutionGroup = m_systemExecutionGroups[index];

        if (!systemExecutionGroup.AllowUpdate())
        {
            continue;
        }

        TaskBatch* currentTaskBatch = systemExecutionGroup.GetTaskBatch();
        AssertDebug(currentTaskBatch != nullptr);

        AssertDebug(currentTaskBatch->IsCompleted(), "TaskBatch for SystemExecutionGroup is not completed: {} tasks enqueued", currentTaskBatch->numEnqueued);

        currentTaskBatch->ResetState();

        // Add tasks to batches before kickoff
        systemExecutionGroup.StartProcessing(delta);

        if (systemExecutionGroup.RequiresGameThread())
        {
            if (m_rootSynchronousExecutionGroup != nullptr)
            {
                m_rootSynchronousExecutionGroup->GetTaskBatch()->nextBatch = currentTaskBatch;
            }
            else
            {
                m_rootSynchronousExecutionGroup = &systemExecutionGroup;
            }

            continue;
        }

        if (!rootTaskBatch)
        {
            rootTaskBatch = currentTaskBatch;
        }

        if (lastTaskBatch != nullptr)
        {
            if (currentTaskBatch->executors.Any())
            {
                lastTaskBatch->nextBatch = currentTaskBatch;
                lastTaskBatch = currentTaskBatch;
            }
        }
        else
        {
            lastTaskBatch = currentTaskBatch;
        }
    }

    // Kickoff first task
    if (rootTaskBatch != nullptr && (rootTaskBatch->executors.Any() || rootTaskBatch->nextBatch != nullptr))
    {
#ifdef HYP_SYSTEMS_PARALLEL_EXECUTION
        TaskSystem::GetInstance().EnqueueBatch(rootTaskBatch);
#endif
    }
}

void EntityManager::EndAsyncUpdate()
{
    HYP_SCOPE;
    Threads::AssertOnThread(m_ownerThreadId);

    for (SystemExecutionGroup& systemExecutionGroup : m_systemExecutionGroups)
    {
        if (!systemExecutionGroup.AllowUpdate() || systemExecutionGroup.RequiresGameThread())
        {
            continue;
        }

        systemExecutionGroup.FinishProcessing();
    }

    if (m_rootSynchronousExecutionGroup != nullptr)
    {
        m_rootSynchronousExecutionGroup->FinishProcessing(/* executeBlocking */ true);

        m_rootSynchronousExecutionGroup = nullptr;
    }

#if defined(HYP_DEBUG_MODE) && (defined(HYP_SYSTEM_LOG_PERFORMANCE) || defined(HYP_SYSTEMS_LAG_SPIKE_DETECTION))
    for (SystemExecutionGroup& systemExecutionGroup : m_systemExecutionGroups)
    {
        const PerformanceClock& performanceClock = systemExecutionGroup.GetPerformanceClock();
        const double elapsedTimeMs = performanceClock.Elapsed() / 1000.0;

#ifdef HYP_SYSTEMS_LAG_SPIKE_DETECTION
        if (elapsedTimeMs >= g_systemExecutionGroupLagSpikeThreshold)
        {
            HYP_LOG(Entity, Warning, "SystemExecutionGroup spike detected: {} ms", elapsedTimeMs);
        }
#endif
#ifdef HYP_SYSTEM_LOG_PERFORMANCE
        for (const auto& it : systemExecutionGroup.GetPerformanceClocks())
        {
            HYP_LOG(Entity, Debug, "\tSystem {} performance: {}", it.first->GetName(), it.second.Elapsed() / 1000.0);
        }
#endif
    }
#endif
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

void EntityManager::GetSystemClasses(Array<const HypClass*>& outClasses) const
{
    HYP_NOT_IMPLEMENTED();
}

#pragma endregion EntityManager

#pragma region SystemExecutionGroup

SystemExecutionGroup::SystemExecutionGroup(bool requiresGameThread, bool allowUpdate)
    : m_requiresGameThread(requiresGameThread),
      m_allowUpdate(allowUpdate),
      m_taskBatch(MakeUnique<TaskBatch>())
{
}

SystemExecutionGroup::~SystemExecutionGroup()
{
}

void SystemExecutionGroup::StartProcessing(float delta)
{
    HYP_SCOPE;

    AssertDebug(AllowUpdate());

#ifdef HYP_DEBUG_MODE
    m_performanceClock.Start();

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second.Get();

        m_performanceClocks.Set(system, PerformanceClock());
    }
#endif

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEM_LOG_PERFORMANCE)
    HYP_LOG(Entity, Debug, "Starting SystemExecutionGroup processing with {} systems", m_systems.Size());
#endif

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second.Get();

#if defined(HYP_DEBUG_MODE) && defined(HYP_SYSTEM_LOG_PERFORMANCE)
        HYP_LOG(Entity, Debug, "\t\tSystem: {}", system->GetName());
#endif

        m_taskBatch->AddTask([this, system, delta]
            {
                HYP_NAMED_SCOPE_FMT("Processing system {}", system->GetName());

#ifdef HYP_DEBUG_MODE
                PerformanceClock& performanceClock = m_performanceClocks[system];
                performanceClock.Start();
#endif

                system->Process(delta);

#ifdef HYP_DEBUG_MODE
                performanceClock.Stop();
#endif
            });
    }
}

void SystemExecutionGroup::FinishProcessing(bool executeBlocking)
{
    AssertDebug(AllowUpdate());

#ifdef HYP_SYSTEMS_PARALLEL_EXECUTION
    if (executeBlocking)
    {
        m_taskBatch->ExecuteBlocking(/* executeDependentBatches */ true);
    }
    else
    {
        m_taskBatch->AwaitCompletion();
    }
#else
    m_taskBatch->ExecuteBlocking(/* executeDependentBatches */ true);
#endif

    for (auto& it : m_systems)
    {
        SystemBase* system = it.second.Get();

        if (system->m_afterProcessProcs.Any())
        {
            for (auto& proc : system->m_afterProcessProcs)
            {
                proc();
            }

            system->m_afterProcessProcs.Clear();
        }
    }

#ifdef HYP_DEBUG_MODE
    m_performanceClock.Stop();
#endif
}

#pragma endregion SystemExecutionGroup

} // namespace hyperion
