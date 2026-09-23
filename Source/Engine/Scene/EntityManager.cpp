/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/ComponentInterface.hpp>

#include <Scene/Entity.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Scene/Components/VisibilityStateComponent.hpp>

#include <Core/Threading/TaskSystem.hpp>

#include <Core/Utilities/Format.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/TypeInfo.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <EntityManager.generated.inl>

namespace Hyperion {

// if the number of systems in a group is less than this value, they will be executed sequentially
// static constexpr double SystemExecutionGroupLagSpikeThreshold = 50.0;

// #define HYP_SYSTEMS_LAG_SPIKE_DETECTION
// #define HYP_SYSTEM_LOG_PERFORMANCE

static bool IsComponentDataCurrent(const ComponentInterface& componentInterface, const BoxedValue& componentData)
{
    if (!componentInterface.IsRuntimeComponent() || componentData.GetTypeInfo() == &componentInterface.GetTypeInfo())
    {
        return true;
    }

    HYP_LOG(Entity, Error, "A '{}' component read before its type was redefined can't be added; it no longer matches the component's layout",
        componentInterface.GetTypeInfo().name);

    return false;
}

#pragma region EntityManager

bool EntityManager::IsValidComponentType(TypeId componentTypeId)
{
    return ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId) != nullptr;
}

bool EntityManager::IsEntityTagComponent(TypeId componentTypeId)
{
    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

    if (!componentInterface)
    {
        return false;
    }

    return componentInterface->IsEntityTag();
}

bool EntityManager::IsEntityTagComponent(TypeId componentTypeId, EntityTag& outTag)
{
    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

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
    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);

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
      m_isLocked(false),
      m_isInitialized(false),
      m_isShuttingDown(false)
{
    Assert(scene != nullptr);

    if (scene->GetSceneFlags() & SceneFlags::DETACHED)
    {
        m_flags |= EntityManagerFlags::DETACHED_SCENE;
    }

    // add initial component containers - types registered at runtime after this point get theirs from GetOrCreateContainer()
    for (const ComponentInterface* componentInterface : ComponentInterfaceRegistry::GetInstance().GetComponentInterfaces())
    {
        Assert(componentInterface != nullptr);

        m_containers.Set(componentInterface->GetTypeId(), MakeUniqueWithAllocator<ComponentContainer, SceneAllocator>(*componentInterface));
    }
}

EntityManager::~EntityManager()
{
    Shutdown();
}

void EntityManager::NotifySystemOfExistingEntities(SystemBase* system)
{
    Assert(m_world != nullptr, "EntityManager must be associated with a World before initializing systems.");

    Assert(system != nullptr);
            
    FatArray<TypeId, InlineAllocator<16>> keys;

    for (auto& subtypeData : m_entities.GetSubtypeData())
    {
        for (auto entitiesIt = subtypeData.data.Begin(); entitiesIt != subtypeData.data.End(); ++entitiesIt)
        {
            EntityData& entityData = *entitiesIt;

            Entity* entity = entityData.entityWeak.GetUnsafe();
            Assert(entity != nullptr);

            const ComponentMap& componentIds = entityData.components;

            keys.Resize(0);
            keys.Reserve(componentIds.Size());

            for (const auto& it : componentIds)
            {
                keys.PushBack(it.first);
            }

            if (system->ActsOnComponents(keys.ToSpan(), true))
            {
                { // critical section
                    TUniqueLock lock(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(system);

                    // Check if the system already has this entity initialized
                    if (systemEntityIt != m_systemEntityMap.End() && (systemEntityIt->second.Find(entity) != systemEntityIt->second.End()))
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
    Assert(m_world != nullptr, "EntityManager must be associated with a World before shutting down systems.");

    Assert(system != nullptr);

    for (auto& subtypeData : m_entities.GetSubtypeData())
    {
        for (auto entitiesIt = subtypeData.data.Begin(); entitiesIt != subtypeData.data.End(); ++entitiesIt)
        {
            EntityData& entityData = *entitiesIt;

            Entity* entity = entityData.entityWeak.GetUnsafe();
            Assert(entity != nullptr);

            if (IsEntityInitializedForSystem(system, entity))
            {
                { // critical section
                    TUniqueLock lock(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(system);

                    if (systemEntityIt == m_systemEntityMap.End())
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

void EntityManager::Initialize()
{
    if (m_isInitialized)
    {
        return;
    }

    AssertOnThread(m_ownerThreadId);

    m_isInitialized = true;
    m_isShuttingDown = false;

    if (m_world != nullptr)
    {
        Array<SystemBase*> systems;

        for (SystemExecutionGroup* group : m_world->GetSystemExecutionGroups())
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

        for (SystemBase* system : systems)
        {
            // Initialize the system
            InitObject(system);
            NotifySystemOfExistingEntities(system);
        }
    }
}

void EntityManager::Shutdown()
{
    // Entities can be added (AddEntity/AddTypedEntity/AddExistingEntity) before Initialize()
    // is ever called, so we need to clear 'em even if we're not initialized, ourselves.
    if (!m_isInitialized)
    {
        ClearEntities_Internal();

        return;
    }

    m_isShuttingDown = true;

#if 0
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

            if (m_scene->GetSceneFlags() & SceneFlags::HAS_OCTREE)
            {
                auto removeFromOctreeResult = m_scene->GetOctree().Remove(entity, /* allowRebuild */ false);
                if (removeFromOctreeResult.HasError())
                {
                    HYP_LOG(Entity, Warning, "Failed to remove Entity {} from Scene {}'s octree: {}",
                        entity->GetName(),
                        m_scene->GetName(),
                        removeFromOctreeResult.GetError().GetMessage());
                }
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
                EntityTag tag = EntityTag::None;
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
        subtypeData.data.Clear();
    }
#endif

    if (m_world != nullptr)
    {
        Set<Handle<Entity>> allEntities;

        for (SystemExecutionGroup* group : m_world->GetSystemExecutionGroups())
        {
            for (auto& systemIt : group->GetSystems())
            {
                SystemBase* system = systemIt.second;
                Assert(system != nullptr);

                // Drain all remaining entities registered with this system in the system entity map
                Set<Entity*> entities;

                {
                    TUniqueLock lock(m_systemEntityMapMutex);

                    auto systemEntityIt = m_systemEntityMap.Find(system);

                    if (systemEntityIt != m_systemEntityMap.End())
                    {
                        for (Entity* entity : systemEntityIt->second)
                        {
                            if (entities.Insert(entity).second)
                            {
                                if (allEntities.FindAs(entity->Id()) == allEntities.End())
                                {
                                    allEntities.Add(MakeStrongRef(entity));
                                }
                            }
                        }

                        systemEntityIt->second.Clear();
                    }
                }

                for (Entity* entity : entities)
                {
                    if (entity->GetEntityManager() != this)
                    {
                        HYP_LOG(Entity, Warning, "Entity {} tracked by system {} has a mismatched EntityManager - skipping OnEntityRemoved", entity->GetName(), system->InstanceClass()->GetName());

                        continue;
                    }

                    system->OnEntityRemoved(entity);
                }

                system->Shutdown();
            }
        }

        for (const Handle<Entity>& entity : allEntities)
        {
            if (entity->GetEntityManager() != this)
            {
                HYP_LOG(Entity, Warning, "Entity {} in Shutdown() no longer owned by this EntityManager - skipping", entity->GetName());

                continue;
            }

            entity->OnRemovedFromWorld(m_world);

            if (m_scene->GetSceneFlags() & SceneFlags::HAS_OCTREE)
            {
                VisibilityStateComponent* visibilityStateComponent = TryGetComponent<VisibilityStateComponent>(entity);

                if (!visibilityStateComponent || visibilityStateComponent->octantId != OctantId::Invalid())
                {
                    auto removeFromOctreeResult = m_scene->GetOctree().Remove(entity, /* allowRebuild */ false);
                    if (removeFromOctreeResult.HasError())
                    {
                        HYP_LOG(Entity, Warning, "Failed to remove Entity {} from Scene {}'s octree: {}",
                                entity->GetName(),
                                m_scene->GetName(),
                                removeFromOctreeResult.GetError().GetMessage());
                    }
                }
            }

            entity->OnRemovedFromScene(m_scene);

            entity->SetEntityManagerRaw_Internal(nullptr);
            entity->SetSceneRaw_Internal(nullptr);
        }
    }

    ClearEntities_Internal();

    m_isInitialized = false;
    m_isShuttingDown = false;
}

void EntityManager::ClearEntities_Internal()
{
    for (auto& subtypeData : m_entities.GetSubtypeData())
    {
        for (EntityData& entityData : subtypeData.data)
        {
            // The Entity may have already been destructed at this point; this happens when
            // Shutdown() detached it (nulling its m_entityManager) and then dropped the last
            // strong reference to it, so ~Entity() could not remove its EntityData.
            // The weak handle keeps the ObjectHeader alive, so Expired() is safe to call.
            if (entityData.entityWeak.Expired())
            {
                continue;
            }

            Entity* entity = entityData.entityWeak.GetUnsafe();
            Assert(entity != nullptr);

            entity->OnRemovedFromWorld(m_world);

            entity->SetEntityManagerRaw_Internal(nullptr);
            entity->SetSceneRaw_Internal(nullptr);
        }

        subtypeData.data.Clear();
    }

    TUniqueLock lock(m_systemEntityMapMutex);
    m_systemEntityMap.Clear();
}

void EntityManager::SetWorld(World* world)
{
    AssertOnThread(m_ownerThreadId);

    if (world == m_world)
    {
        return;
    }

    // If EntityManager is initialized we need to notify all of our systems that the world has changed.
    Array<SystemBase*> systems;

    // Call OnRemovedFromWorld() now for all entities in the EntityManager if previous world is not null
    if (m_world)
    {
        for (SystemExecutionGroup* group : m_world->GetSystemExecutionGroups())
        {
            for (auto& systemIt : group->GetSystems())
            {
                SystemBase* system = systemIt.second;
                Assert(system != nullptr && !systems.Contains(system));

                systems.PushBack(system);
            }
        }

        for (SystemBase* system : systems)
        {
            NotifySystemOfAllEntitiesRemoved(system);
        }

        for (auto& subtypeData : m_entities.GetSubtypeData())
        {
            for (EntityData& entityData : subtypeData.data)
            {
                Entity* entity = entityData.entityWeak.GetUnsafe();
                Assert(entity != nullptr);

                // @NOTE: Entities remain registered in this EntityManager while the Scene has no World.
                // 
                // We do NOT unset the Entity's EntityManager pointer here - otherwise the Entity can no
                // longer be removed from this EntityManager

                entity->OnRemovedFromWorld(m_world);
            }
        }
    }

    m_world = world;

    if (m_world != nullptr)
    {
        systems.Clear();

        for (SystemExecutionGroup* group : m_world->GetSystemExecutionGroups())
        {
            for (auto& systemIt : group->GetSystems())
            {
                SystemBase* system = systemIt.second;
                Assert(system != nullptr);

                systems.PushBack(system);
            }
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

        // notify systems of entity added for the new world
        for (SystemBase* system : systems)
        {
            NotifySystemOfExistingEntities(system);
        }
    }
}

void EntityManager::Lock()
{
    AssertDebug(!IsDetachedScene(), "Cannot lock/unlock EntityManager for detached scene");

    if (IsDetachedScene())
    {
        return;
    }

    AssertOnThread(m_ownerThreadId);

    m_isLocked = true;
}

void EntityManager::Unlock()
{
    AssertDebug(!IsDetachedScene(), "Cannot lock/unlock EntityManager for detached scene");

    if (IsDetachedScene())
    {
        return;
    }

    AssertOnThread(m_ownerThreadId);

    m_isLocked = false;
}

Handle<Entity> EntityManager::AddBasicEntity()
{
    Assert(!IsLocked() && (IsOnThread(m_ownerThreadId) || IsDetachedScene()));

    TLockGuard<AtomicFlag> lock;

    if (IsDetachedScene())
    {
        lock.Reset(m_detachedSceneLocked);
    }

    Handle<Entity> entity = MakeHandle<Entity>();

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    m_entities.Add(entity);

    entity->SetEntityManagerRaw_Internal(this);
    entity->SetScene(m_scene);

    InitObject(entity);

    // Use basic TypeId tag for the entity, as the type is just Entity
    AddTag<EntityTag::EntityTypeSentinel>(entity);

    if (entity->m_entityInitInfo.receivesUpdate)
    {
        AddTag<EntityTag::ReceivesUpdate>(entity);
    }

    if (entity->m_entityInitInfo.initialTags.Any())
    {
        AddTags(entity, entity->m_entityInitInfo.initialTags);
    }

    lock.Reset();

    entity->OnAddedToScene(m_scene);

    if (m_world)
    {
        entity->OnAddedToWorld(m_world);
    }

    return entity;
}

Handle<Entity> EntityManager::AddTypedEntity(const Class* cls)
{
    Assert(!IsLocked() && (IsOnThread(m_ownerThreadId) || IsDetachedScene()));

    TLockGuard<AtomicFlag> lock;

    if (IsDetachedScene())
    {
        lock.Reset(m_detachedSceneLocked);
    }

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

    entity->SetEntityManagerRaw_Internal(this);
    entity->SetScene(m_scene);

    InitObject(entity);

    if (entity->m_entityInitInfo.receivesUpdate)
    {
        AddTag<EntityTag::ReceivesUpdate>(entity);
    }

    // Create tag to track class of the entity.

    AddTag<EntityTag::EntityTypeSentinel>(entity);

    while (cls != nullptr && cls != Entity::StaticClass())
    {
        EntityTag entityTypeTag = MakeEntityTypeTag(cls->GetTypeId());
        AssertDebug((uint64(entityTypeTag) & uint64(EntityTag::EntityTypeSentinel)) != 0);

        const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(entityTypeTag);
        AssertDebug(componentInterface);

        AddTag(entity, entityTypeTag);

        cls = cls->GetParent();
    }

    if (entity->m_entityInitInfo.initialTags.Any())
    {
        AddTags(entity, entity->m_entityInitInfo.initialTags);
    }

    lock.Reset();

    entity->OnAddedToScene(m_scene);

    if (m_world)
    {
        entity->OnAddedToWorld(m_world);
    }

    return entity;
}

void EntityManager::AddExistingEntity_Internal(const Handle<Entity>& entity)
{
    if (!entity.IsValid())
    {
        return;
    }

    Assert(!IsLocked() && (IsOnThread(m_ownerThreadId) || IsDetachedScene()));

    TLockGuard<AtomicFlag> lock;

    if (IsDetachedScene())
    {
        lock.Reset(m_detachedSceneLocked);
    }

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

    entity->SetEntityManagerRaw_Internal(this);

    lock.Reset();

    entity->SetScene(m_scene);

    if (IsDetachedScene())
    {
        lock.Reset(m_detachedSceneLocked);
    }

    InitObject(entity);

    AddTag<EntityTag::EntityTypeSentinel>(entity);

    const Class* cls = entity->InstanceClass();

    while (cls != nullptr && cls != Entity::StaticClass())
    {
        EntityTag entityTypeTag = MakeEntityTypeTag(cls->GetTypeId());
        AssertDebug((uint64(entityTypeTag) & uint64(EntityTag::EntityTypeSentinel)) != 0);

        const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(entityTypeTag);
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

    lock.Reset();

    entity->OnAddedToScene(m_scene);

    if (m_world)
    {
        entity->OnAddedToWorld(m_world);
    }
}

/// Called from Entity destructor or from a task enqueued during Entity destructor.
/// Does not operate on the Entity pointer as it would be invalid at this point
bool EntityManager::RemoveEntity(Entity* entity, bool calledFromEntityDestructor)
{
    Assert(!IsLocked() && (IsOnThread(m_ownerThreadId) || IsDetachedScene()));

    if (!entity)
    {
        return false;
    }

    TLockGuard<AtomicFlag> lock;

    if (IsDetachedScene())
    {
        lock.Reset(m_detachedSceneLocked);
    }

    HYP_MT_CHECK_RW(m_entitiesDataRaceDetector);

    const ObjId<Entity> entityId = entity->Id();

    struct RemovedComponent
    {
        TypeId typeId;
        ComponentContainer* container;
        ComponentId id;
    };

    // Components are destroyed once the entity is fully removed, matching when the boxed copies used to be destroyed
    FatArray<RemovedComponent, InlineAllocator<16>> removedComponents;

    EntityData* entityData = m_entities.TryGetEntityData(entityId);
    Assert(entityData != nullptr, "Entity does not exist");

    if (!calledFromEntityDestructor)
    {
        NotifySystemsOfEntityRemoved(entity, entityData->components);
    }
    else if (m_scene != nullptr && (m_scene->GetSceneFlags() & SceneFlags::HAS_OCTREE))
    {
        m_scene->GetOctree().Remove(entity);
    }


    for (auto componentInfoPairIt = entityData->components.Begin(); componentInfoPairIt != entityData->components.End();)
    {
        const TypeId componentTypeId = componentInfoPairIt->first;
        const ComponentId componentId = componentInfoPairIt->second;

        auto componentContainerIt = m_containers.Find(componentTypeId);
        Assert(componentContainerIt != m_containers.End(), "Component container does not exist");
        Assert(componentContainerIt->second->HasComponent(componentId), "Component does not exist in component container");

        AnyRef componentRef = componentContainerIt->second->TryGetComponent(componentId);
        Assert(componentRef.HasValue(), "Component of type '{}' with id {} does not exist in component container", *GetComponentTypeName(componentTypeId), componentId);

        if (!calledFromEntityDestructor)
        {
            // Notify the entity that the component is being removed
            // - needed to ensure proper lifecycle. every OnComponentRemoved() call must be matched with an OnComponentAdded() call and vice versa
            if (componentContainerIt->second->GetComponentInterface().IsEntityTag())
            {
                // Remove the tag from the entity
                entity->OnTagRemoved(componentContainerIt->second->GetComponentInterface().GetEntityTag());
            }
            else
            {
                entity->OnComponentRemoved(componentRef);
            }
        }

        removedComponents.PushBack(RemovedComponent { componentTypeId, componentContainerIt->second.Get(), componentId });

        // Update iterator, erase the component from the entity's component map
        componentInfoPairIt = entityData->components.Erase(componentInfoPairIt);
    }

    for (const RemovedComponent& removedComponent : removedComponents)
    {
        // Update our entity sets to reflect the change
        auto componentEntitySetsIt = m_componentEntitySets.Find(removedComponent.typeId);

        if (componentEntitySetsIt != m_componentEntitySets.End())
        {
            for (EntitySetId entitySetId : componentEntitySetsIt->second)
            {
                EntitySetBase& entitySet = *m_entitySets.At(entitySetId);

                entitySet.RemoveEntity(entity);
            }
        }
    }

    if (!calledFromEntityDestructor)
    {
        entity->SetEntityManagerRaw_Internal(nullptr);
    }

    m_entities.Remove(entityId);

    for (const RemovedComponent& removedComponent : removedComponents)
    {
        if (!removedComponent.container->RemoveComponent(removedComponent.id))
        {
            HYP_FAIL("Failed to remove component of type '{}' with id {} from its component container", *GetComponentTypeName(removedComponent.typeId), removedComponent.id);
        }
    }

    return true;
}

void EntityManager::MoveEntity(const Handle<Entity>& entity, const Handle<EntityManager>& other)
{
    Assert(entity.IsValid());
    AssertDebug(entity->GetEntityManager() == this);

    Assert(other.IsValid());

    if (this == other.Get())
    {
        return;
    }

    Assert(!IsLocked() && (IsOnThread(m_ownerThreadId) || IsDetachedScene()));

    TLockGuard<AtomicFlag> lock;

    if (IsDetachedScene())
    {
        lock.Reset(m_detachedSceneLocked);
    }

    struct MovedComponent
    {
        const ComponentInterface* componentInterface;
        BoxedValue value;
    };

    // Components generically stored as BoxedValue - to add to other EntityManager
    Array<MovedComponent> components;

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

            // the other EntityManager only accepts a redefined runtime component in its current layout
            EnsureCurrentComponentLayout(*componentContainerIt->second);

            AnyRef componentRef = componentContainerIt->second->TryGetComponent(componentId);
            Assert(componentRef.HasValue(), "Component of type '{}' with id {} does not exist in component container", *GetComponentTypeName(componentTypeId), componentId);

            const ComponentInterface& componentInterface = componentContainerIt->second->GetComponentInterface();

            // Notify the entity that the component is being removed
            // - needed to ensure proper lifecycle. every OnComponentRemoved() call must be matched with an OnComponentAdded() call and vice versa
            if (componentInterface.IsEntityTag())
            {
                entity->OnTagRemoved(componentInterface.GetEntityTag(), /* refreshDependentTags */ false);
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

            components.PushBack(MovedComponent { &componentInterface, std::move(component) });

            // Update iterator, erase the component from the entity's component map
            componentInfoPairIt = entityData->components.Erase(componentInfoPairIt);
        }

        {
            for (const MovedComponent& component : components)
            {
                const TypeId componentTypeId = component.componentInterface->GetTypeId();

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

        entity->SetEntityManagerRaw_Internal(nullptr);

        m_entities.Remove(entity);
    }

    lock.Reset();

    // Add the entity and its components to the other EntityManager
    auto addToOtherEntityManager = [other = other, entity = entity, components = std::move(components)]() mutable
    {
        Assert(!other->IsLocked() && (IsOnThread(other->m_ownerThreadId) || other->IsDetachedScene()));

        TLockGuard<AtomicFlag> lock;

        if (other->IsDetachedScene())
        {
            lock.Reset(other->m_detachedSceneLocked);
        }

        // Sanity check to prevent infinite recursion from AddExistingEntity calling MoveEntity again if there is already an EntityManager set
        AssertDebug(entity->GetEntityManager() == nullptr);

        HYP_MT_CHECK_RW(other->m_entitiesDataRaceDetector);

        other->m_entities.Add(entity);

        entity->SetEntityManagerRaw_Internal(other.Get());

        lock.Reset();

        entity->SetScene(other->m_scene);

        if (other->IsDetachedScene())
        {
            lock.Reset(other->m_detachedSceneLocked);
        }

        InitObject(entity);

        EntityData* entityData = other->m_entities.TryGetEntityData(entity.Id());
        Assert(entityData != nullptr, "Entity with id {} does not exist", entity.Id());

        ComponentMap componentIds;

        for (MovedComponent& component : components)
        {
            const ComponentInterface& componentInterface = *component.componentInterface;
            const TypeId componentTypeId = componentInterface.GetTypeId();

            // Update the EntityData
            auto componentIt = entityData->FindComponent(componentTypeId);

            if (componentIt != entityData->components.End())
            {
                if (componentInterface.IsEntityTag())
                {
                    // Duplicate of the same tag, don't worry about it

                    continue;
                }

                HYP_LOG(Entity, Error, "Cannot add duplicate component of type '{}'", *GetComponentTypeName(componentTypeId));
                continue;
            }

            ComponentContainer* container = other->GetOrCreateContainer(componentInterface);

            if (!container)
            {
                HYP_LOG(Entity, Error, "No component container available for component of type '{}'; component dropped while moving entity", *GetComponentTypeName(componentTypeId));
                continue;
            }

            const ComponentId componentId = container->AddComponent(std::move(component.value));

            componentIds.Set(componentTypeId, componentId);

            entityData->components.Set(componentTypeId, componentId);

            AnyRef componentRef = container->TryGetComponent(componentId);
            Assert(componentRef.HasValue(), "Failed to get component of type '{}' with id {} from component container", *GetComponentTypeName(componentTypeId), componentId);

            if (componentInterface.IsEntityTag())
            {
                entity->OnTagAdded(componentInterface.GetEntityTag());
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

        if (entity->HasUnresolvedComponents())
        {
            other->MarkUnresolvedComponents();
        }

        // Notify systems that entity is being added to them
        other->NotifySystemsOfEntityAdded(entity, componentIds);

        // Have to reset lock before calling OnAddedToWorld or OnAddedToScene, the Entity
        // may add new Entities which would cause a deadlock for detached scenes if this wasn't unlocked.
        lock.Reset();

        entity->OnAddedToScene(other->m_scene);

        if (other->m_world)
        {
            entity->OnAddedToWorld(other->m_world);
        }
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

void EntityManager::ResolveUnresolvedComponents()
{
    if (!m_hasUnresolvedComponents || IsLocked() || !IsOnThread(m_ownerThreadId))
    {
        return;
    }

    const uint32 registrationGeneration = ComponentInterfaceRegistry::GetInstance().GetRuntimeRegistrationGeneration();

    if (registrationGeneration == m_resolvedRegistrationGeneration)
    {
        return;
    }

    m_resolvedRegistrationGeneration = registrationGeneration;

    // collected first: resolving adds components, which must not happen while walking entity storage
    Array<Handle<Entity>> pendingEntities;

    ForEachEntity([&pendingEntities](Entity* entity)
        {
            if (entity->HasUnresolvedComponents())
            {
                pendingEntities.PushBack(MakeStrongRef(entity));
            }
        });

    bool anyRemaining = false;

    for (const Handle<Entity>& entity : pendingEntities)
    {
        anyRemaining |= entity->ResolveUnresolvedComponents();
    }

    m_hasUnresolvedComponents = anyRemaining;
}

void EntityManager::SyncRuntimeComponentTypes()
{
    if (!IsOnThread(m_ownerThreadId))
    {
        return;
    }

    MigrateStaleComponentContainers();
    ResolveUnresolvedComponents();
}

void EntityManager::MigrateStaleComponentContainers()
{
    const ComponentInterfaceRegistry& registry = ComponentInterfaceRegistry::GetInstance();
    const uint32 registrationGeneration = registry.GetRuntimeRegistrationGeneration();

    if (registrationGeneration == m_migratedRegistrationGeneration)
    {
        return;
    }

    // containers are migrated in place, so m_containers itself doesn't change and pointers to the containers stay valid
    for (auto& it : m_containers)
    {
        EnsureCurrentComponentLayout(*it.second);
    }

    m_migratedRegistrationGeneration = registrationGeneration;
}

bool EntityManager::EnsureCurrentComponentLayout(ComponentContainer& container)
{
    const ComponentInterface& containerInterface = container.GetComponentInterface();

    if (!containerInterface.IsRuntimeComponent())
    {
        return true;
    }

    const ComponentInterface* currentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(container.GetComponentTypeId());

    if (!currentInterface || currentInterface == &containerInterface)
    {
        return true;
    }

    // no system reads runtime component containers, so the owner thread may migrate even while locked
    if (!IsOnThread(m_ownerThreadId))
    {
        return false;
    }

    container.MigrateTo(*currentInterface);

    return true;
}

ComponentContainer* EntityManager::GetOrCreateContainer(const ComponentInterface& componentInterface)
{
    const TypeId componentTypeId = componentInterface.GetTypeId();

    {
        TSharedLock lock(m_componentContainersMtx);

        auto it = m_containers.Find(componentTypeId);

        if (it != m_containers.End() && &it->second->GetComponentInterface() == &componentInterface)
        {
            return it->second.Get();
        }
    }

    Assert(!IsLocked() && (IsOnThread(m_ownerThreadId) || IsDetachedScene()), "Component containers can only be created on the owner thread while the EntityManager is unlocked");

    TUniqueLock lock(m_componentContainersMtx);

    auto it = m_containers.Find(componentTypeId);

    if (it != m_containers.End())
    {
        if (&it->second->GetComponentInterface() == &componentInterface)
        {
            return it->second.Get();
        }

        // the container predates a redefinition of its type
        // it can only move forward to the current registration
        if (ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId) != &componentInterface)
        {
            HYP_LOG(Entity, Error, "Component type '{}' was redefined; a component of its previous definition can't be added", componentInterface.GetTypeInfo().name);

            return nullptr;
        }

        it->second->MigrateTo(componentInterface);

        return it->second.Get();
    }

    it = m_containers.Set(componentTypeId, MakeUniqueWithAllocator<ComponentContainer, SceneAllocator>(componentInterface)).first;

    return it->second.Get();
}

void EntityManager::AddComponent_Internal(Entity* entity, const ComponentInterface& componentInterface, ComponentConstructMode constructMode, void* source)
{
    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    Assert(entity, "Invalid entity");

    Handle<Entity> entityHandle = MakeStrongRef(entity);
    Assert(entityHandle.IsValid());

    EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
    Assert(entityData != nullptr, "Entity with id {} does not exist", entity->Id());

    const TypeId componentTypeId = componentInterface.GetTypeId();

    ComponentMap componentIds;

    // Update the EntityData
    auto componentIt = entityData->FindComponent(componentTypeId);

    if (componentIt != entityData->components.End())
    {
        if (componentInterface.IsEntityTag())
        {
            // Duplicate of the same tag, don't worry about it

            return;
        }

        HYP_LOG(Entity, Error, "Cannot add duplicate component of type '{}' to entity {}", *GetComponentTypeName(componentTypeId), entity->GetName());

        return;
    }

    ComponentContainer* container = GetOrCreateContainer(componentInterface);
    Assert(container != nullptr, "Component container does not exist for component of type '{}'", *GetComponentTypeName(componentTypeId));

    ComponentId componentId = Invalid<ComponentId>;

    switch (constructMode)
    {
    case ComponentConstructMode::DEFAULT:
        componentId = container->AddDefaultComponent();
        break;
    case ComponentConstructMode::COPY:
        componentId = container->AddComponentCopy(source);
        break;
    case ComponentConstructMode::MOVE:
        componentId = container->AddComponentMove(source);
        break;
    default:
        HYP_UNREACHABLE();
    }

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

    if (componentInterface.IsEntityTag())
    {
        entity->OnTagAdded(componentInterface.GetEntityTag());
    }
    else
    {
        // Note: Call before notifying systems as they are able to remove components!
        entity->OnComponentAdded(componentRef);
    }

    // Notify systems that entity is being added to them
    NotifySystemsOfEntityAdded(entityHandle, componentIds);
}

void EntityManager::AddComponent(Entity* entity, const BoxedValue& componentData)
{
    AssertDebug(!componentData.IsNull());

    const TypeId componentTypeId = componentData.GetTypeId();
    EnsureValidComponentType(componentTypeId);

    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);
    Assert(componentInterface != nullptr, "Component container does not exist for component of type TypeId({})", componentTypeId.Value());

    if (!IsComponentDataCurrent(*componentInterface, componentData))
    {
        return;
    }

    AddComponent_Internal(entity, *componentInterface, ComponentConstructMode::COPY, componentData.ToRef().GetPointer());
}

void EntityManager::AddComponent(Entity* entity, BoxedValue&& componentData)
{
    AssertDebug(!componentData.IsNull());

    const TypeId componentTypeId = componentData.GetTypeId();
    EnsureValidComponentType(componentTypeId);

    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);
    Assert(componentInterface != nullptr, "Component container does not exist for component of type TypeId({})", componentTypeId.Value());

    if (!IsComponentDataCurrent(*componentInterface, componentData))
    {
        return;
    }

    AddComponent_Internal(entity, *componentInterface, ComponentConstructMode::MOVE, componentData.ToRef().GetPointer());
}

void EntityManager::AddDefaultComponent(Entity* entity, TypeId componentTypeId)
{
    EnsureValidComponentType(componentTypeId);

    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetComponentInterface(componentTypeId);
    Assert(componentInterface != nullptr, "Component container does not exist for component of type TypeId({})", componentTypeId.Value());

    AddComponent_Internal(entity, *componentInterface, ComponentConstructMode::DEFAULT, nullptr);
}

bool EntityManager::RemoveComponent(TypeId componentTypeId, Entity* entity)
{
    EnsureValidComponentType(componentTypeId);

    Assert(!IsLocked() && (IsOnThread(m_ownerThreadId) || IsDetachedSceneLocked()));

    if (!entity)
    {
        return false;
    }

    // Systems and the Entity's own callbacks below may drop the last reference to it
    Handle<Entity> entityHandle = MakeStrongRef(entity);

    HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);

    EntityData* entityData = m_entities.TryGetEntityData(entity->Id());

    if (!entityData)
    {
        return false;
    }

    const ComponentId componentId = entityData->GetComponentId(componentTypeId);

    if (componentId == Invalid<ComponentId>)
    {
        return false;
    }

    auto* container = TryGetContainer(componentTypeId);

    if (!container)
    {
        return false;
    }

    ComponentMap removedComponents;
    removedComponents.Set(componentTypeId, componentId);

    NotifySystemsOfEntityRemoved(entity, removedComponents);

    // A system may have removed the component itself from OnEntityRemoved() in which case doing everything below would be redundant
    entityData = m_entities.TryGetEntityData(entity->Id());

    if (!entityData || entityData->GetComponentId(componentTypeId) != componentId)
    {
        return true;
    }

    // removal callbacks get the component as its current type
    EnsureCurrentComponentLayout(*container);

    AnyRef componentRef = container->TryGetComponent(componentId);

    if (!componentRef.HasValue())
    {
        HYP_LOG(Entity, Error, "Failed to get component of type '{}' with Id {} for entity #{}", *GetComponentTypeName(componentTypeId), componentId, entity->Id());

        return false;
    }

    EntityTag tag = EntityTag::None;
    if (IsEntityTagComponent(componentTypeId, tag))
    {
        entity->OnTagRemoved(tag);
    }
    else
    {
        entity->OnComponentRemoved(componentRef);
    }

    entityData = m_entities.TryGetEntityData(entity->Id());

    if (!entityData)
    {
        return true;
    }

    auto componentIt = entityData->FindComponent(componentTypeId);

    if (componentIt == entityData->components.End() || componentIt->second != componentId)
    {
        return true;
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

    if (!container->RemoveComponent(componentId))
    {
        HYP_FAIL("Component of type '{}' with Id {} was present in the Entity's component map but not found in the ComponentContainer", *GetComponentTypeName(componentTypeId), componentId);
    }

    return true;
}

bool EntityManager::HasTag(const Entity* entity, EntityTag tag) const
{
    Assert(!IsLocked() && (IsOnThread(m_ownerThreadId) || IsDetachedSceneLocked()));

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

    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!componentInterface)
    {
        HYP_LOG(Entity, Error, "No TagComponent registered for EntityTag {}", tag.value);

        return false;
    }

    const TypeInfo& componentTypeInfo = componentInterface->GetTypeInfo();

    return HasComponent(componentTypeInfo.id, entity);
}

void EntityManager::AddTag(Entity* entity, EntityTag tag)
{
    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    if (!entity)
    {
        return;
    }

    Handle<Entity> entityHandle = MakeStrongRef(entity);
    Assert(entityHandle.IsValid());

    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!componentInterface)
    {
        HYP_LOG(Entity, Error, "No TagComponent registered for EntityTag {}", tag.value);

        return;
    }

    if (HasComponent(componentInterface->GetTypeId(), entity))
    {
        return;
    }

    AddComponent_Internal(entity, *componentInterface, ComponentConstructMode::DEFAULT, nullptr);
}

bool EntityManager::RemoveTag(Entity* entity, EntityTag tag)
{
    Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

    if (!entity)
    {
        return false;
    }

    Assert(!IsLocked() && (IsOnThread(m_ownerThreadId) || IsDetachedSceneLocked()));

    const ComponentInterface* componentInterface = ComponentInterfaceRegistry::GetInstance().GetEntityTagComponentInterface(tag);

    if (!componentInterface)
    {
        HYP_LOG(Entity, Error, "No TagComponent registered for EntityTag {}", tag.value);

        return false;
    }

    const TypeInfo& componentTypeInfo = componentInterface->GetTypeInfo();

    return RemoveComponent(componentTypeInfo.id, entity);
}

void EntityManager::NotifySystemsOfEntityAdded(const Handle<Entity>& entity, const ComponentMap& componentIds)
{
    if (!entity.IsValid())
    {
        return;
    }

    // If the EntityManager is initialized, notify systems of the entity being added
    // otherwise, the systems will be notified when the EntityManager is initialized
    if (!m_world)
    {
        return;
    }

    // don't call OnEntityAdded() when shutting down, some systems in OnEntityRemoved() will add tags,
    // (eg MeshSystem adding UpdateRenderProxy)
    // we don't want to revive anything here.
    if (m_isShuttingDown)
    {
        return;
    }
            
    FatArray<TypeId, InlineAllocator<16>> keys;

    for (SystemExecutionGroup* group : m_world->GetSystemExecutionGroups())
    {
        for (auto& systemIt : group->GetSystems())
        {
            keys.Resize(0);
            keys.Reserve(componentIds.Size());

            for (const auto& it : componentIds)
            {
                keys.PushBack(it.first);
            }

            if (systemIt.second->ActsOnComponents(keys.ToSpan(), true))
            {
                { // critical section
                    TUniqueLock lock(m_systemEntityMapMutex);

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

void EntityManager::NotifySystemsOfEntityRemoved(Entity* entity, const ComponentMap& componentIds)
{
    if (!entity)
    {
        return;
    }

    if (!m_world)
    {
        return;
    }

    WeakHandle<Entity> entityWeak = MakeWeakRef(entity);
    
    FatArray<TypeId, InlineAllocator<16>> keys;

    for (SystemExecutionGroup* group : m_world->GetSystemExecutionGroups())
    {
        for (auto& systemIt : group->GetSystems())
        {
            keys.Resize(0);
            keys.Reserve(componentIds.Size());

            for (const auto& it : componentIds)
            {
                keys.PushBack(it.first);
            }

            bool systemAffected = false;

            for (const TypeId key : keys)
            {
                if (!systemIt.second->HasComponentTypeId(key, /* includeReadOnly */ true))
                {
                    continue;
                }

                const ComponentInfo& componentInfo = systemIt.second->GetComponentInfo(key);

                // writable components, plus read-only ones that gate OnEntityAdded()
                const bool isWritable = !!(componentInfo.access & ComponentAccess::WRITE);
                const bool isEventComponent = !!(componentInfo.access & ComponentAccess::READ_WRITE) && componentInfo.receivesEvents;

                if (isWritable || isEventComponent)
                {
                    systemAffected = true;

                    break;
                }
            }

            if (systemAffected)
            {
                {
                    TUniqueLock lock(m_systemEntityMapMutex);

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
    Assert(!IsLocked() && IsOnThread(m_ownerThreadId) && !IsDetachedScene());

    TUniqueLock lock(m_pendingEntitySetsMtx);

    for (auto& kvp : m_pendingEntitySets)
    {
        const EntitySetId entitySetId = kvp.first;
        UniquePtr<EntitySetBase, SceneAllocator>& entitySetPtr = kvp.second;

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
    if (!system)
    {
        return false;
    }

    TSharedLock lock(m_systemEntityMapMutex);

    const auto it = m_systemEntityMap.Find(system);

    if (it == m_systemEntityMap.End())
    {
        return false;
    }

    return it->second.Find(const_cast<Entity*>(entity)) != it->second.End();
}

void EntityManager::UntrackEntityFromAllSystems(Entity* entity)
{
    if (m_world == nullptr)
    {
        return;
    }

    TUniqueLock lock(m_systemEntityMapMutex);

    for (auto& systemEntityPair : m_systemEntityMap)
    {
        systemEntityPair.second.Erase(entity);
    }
}

#pragma endregion EntityManager

} // namespace Hyperion
