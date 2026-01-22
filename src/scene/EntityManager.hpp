/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/FlatSet.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>
#include <core/containers/TypeMap.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/functional/Proc.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/ForEach.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>
#include <core/reflection/ObjId.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <scene/Entity.hpp>
#include <scene/EntitySet.hpp>
#include <scene/EntityContainer.hpp>
#include <scene/ComponentContainer.hpp>
#include <scene/System.hpp>
#include <scene/EntityTag.hpp>
#include <scene/SystemExecutionGroup.hpp>

namespace Hyperion {

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

enum class EntityManagerFlags : uint32
{
    NONE = 0x0,

    DEFAULT = NONE
};

HYP_MAKE_ENUM_FLAGS(EntityManagerFlags)

class World;
class Scene;
struct BoxedValue;
class Node;

using ComponentMap = TypeMap<ComponentId>;

/*! \brief The EntityManager is responsible for managing Entities and their components within a single Scene. */
HYP_CLASS()
class HYP_API EntityManager final : public ObjectBase
{
    HYP_OBJECT_BODY(EntityManager);

    friend class EntityToEntityManagerMap;

    // Allow Entity destructor to call RemoveEntity().
    friend class Entity;

    friend class World;

public:
    EntityManager(const ThreadId& ownerThreadId, Scene* scene, EnumFlags<EntityManagerFlags> flags = EntityManagerFlags::DEFAULT);
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;
    EntityManager(EntityManager&&) noexcept = delete;
    EntityManager& operator=(EntityManager&&) noexcept = delete;
    ~EntityManager();

    template <class Component>
    static bool IsValidComponentType()
    {
        return IsValidComponentType(TypeId::ForType<Component>());
    }

    static bool IsValidComponentType(TypeId componentTypeId);

    template <class Component>
    static bool IsEntityTagComponent()
    {
        return IsEntityTagComponent(TypeId::ForType<Component>());
    }

    static bool IsEntityTagComponent(TypeId componentTypeId);
    static bool IsEntityTagComponent(TypeId componentTypeId, EntityTag& outTag);

    template <class Component>
    static ANSIStringView GetComponentTypeName()
    {
        return GetComponentTypeName(TypeId::ForType<Component>());
    }

    static ANSIStringView GetComponentTypeName(TypeId componentTypeId);

    /*! \brief Gets the thread mask of the thread that owns this EntityManager.
     *
     *  \return The thread mask.
     */
    HYP_FORCE_INLINE const ThreadId& GetOwnerThreadId() const
    {
        return m_ownerThreadId;
    }

    /*! \brief Sets the thread mask of the thread that owns this EntityManager.
     *  \internal This is used by the Scene to set the thread mask of the Scene's thread. It should not be called from user code. */
    HYP_FORCE_INLINE void SetOwnerThreadId(const ThreadId& ownerThreadId)
    {
        m_ownerThreadId = ownerThreadId;
    }

    /*! \brief Gets the World that this EntityManager is associated with.
     *
     *  \return Pointer to the World.
     */
    HYP_METHOD()
    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

    void SetWorld(World* world);

    /*! \brief Gets the Scene that this EntityManager is associated with.
     *
     *  \return Pointer to the Scene.
     */
    HYP_METHOD()
    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_scene;
    }

    HYP_FORCE_INLINE EnumFlags<EntityManagerFlags> GetEntityManagerFlags() const
    {
        return m_flags;
    }

    HYP_FORCE_INLINE EntityContainer& GetEntities()
    {
        return m_entities;
    }

    HYP_FORCE_INLINE const EntityContainer& GetEntities() const
    {
        return m_entities;
    }

    /*! \brief The EntityManager so that other threads cannot mutate the entity sets or create new ones */
    HYP_FORCE_INLINE bool IsLocked() const
    {
        return m_isLocked;
    }

    HYP_FORCE_INLINE void Lock()
    {
        AssertOnThread(m_ownerThreadId);

        m_isLocked = true;
    }

    HYP_FORCE_INLINE void Unlock()
    {
        AssertOnThread(m_ownerThreadId);

        m_isLocked = false;
    }

    /*! \brief Adds a new entity to the EntityManager.
     *  \note Must be called from the owner thread.
     *
     *  \return The Entity that was added. */
    HYP_FORCE_INLINE Handle<Entity> AddEntity()
    {
        return AddBasicEntity();
    }

    /*! \brief Adds a new entity to the EntityManager.
     *  \note Must be called from the owner thread.
     *
     *  \tparam T The type of the Entity to add. Must be a subclass of Entity.
     *
     *  \param [in] args The arguments to pass to the Entity constructor.
     *
     *  \return The Entity that was added. */
    template <class T, class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE Handle<T> AddEntity(Args&&... args)
    {
        static_assert(std::is_base_of_v<Entity, T>, "T must be a subclass of Entity");

        Handle<T> entity = MakeHandle<T>(std::forward<Args>(args)...);
        Assert(entity.IsValid(), "Failed to create instance of Entity subclass {}", TypeNameWithoutNamespace<T>().Data());

        AddExistingEntity(entity);

        return entity;
    }

    /*! \brief Adds an existing entity to the EntityManager. */
    HYP_METHOD()
    HYP_FORCE_INLINE void AddExistingEntity(const Handle<Entity>& entity)
    {
        AddExistingEntity_Internal(entity);
    }

    HYP_METHOD()
    Handle<Entity> AddTypedEntity(const Class* cls);

    /*! \brief Moves an entity from one EntityManager to another.
     *  This is useful for moving entities between scenes.
     *  All components will be moved to the other EntityManager.
     *
     *  \param[in] entity The Entity to move.
     *  \param[in] other The EntityManager to move the entity to.
     */
    void MoveEntity(const Handle<Entity>& entity, const Handle<EntityManager>& other);

    HYP_FORCE_INLINE bool HasEntity(ObjId<Entity> id) const
    {
        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        return id.IsValid() && m_entities.HasEntity(id);
    }

    void AddTag(Entity* entity, EntityTag tag);
    bool RemoveTag(Entity* entity, EntityTag tag);
    bool HasTag(const Entity* entity, EntityTag tag) const;

    template <EntityTag Tag>
    HYP_FORCE_INLINE bool HasTag(const Entity* entity) const
    {
        return HasComponent<TagComponent<Tag>>(entity);
    }

    template <EntityTag Tag>
    HYP_FORCE_INLINE void AddTag(Entity* entity)
    {
        if (HasTag<Tag>(entity))
        {
            return;
        }

        AddTag(entity, Tag);
    }

    template <EntityTag... Tag>
    HYP_FORCE_INLINE void AddTags(Entity* entity)
    {
        (AddTag<Tag>(entity), ...);
    }

    HYP_FORCE_INLINE void AddTags(Entity* entity, Span<const EntityTag> tags)
    {
        for (EntityTag tag : tags)
        {
            if (tag == EntityTag::None || uint32(tag) >= uint32(EntityTag::EntityType))
            {
                continue;
            }

            AddTag(entity, tag);
        }
    }

    template <EntityTag Tag>
    HYP_FORCE_INLINE bool RemoveTag(Entity* entity)
    {
        if (!HasTag<Tag>(entity))
        {
            return false;
        }

        return RemoveComponent<TagComponent<Tag>>(entity);
    }

    HYP_FORCE_INLINE Array<EntityTag> GetSavableTags(const Entity* entity) const
    {
        Array<EntityTag> tags;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::MaxPersistent) - 2>(), tags);

        return tags;
    }

    HYP_FORCE_INLINE uint32 GetSavableTagsMask(const Entity* entity) const
    {
        uint32 mask = 0;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::MaxPersistent) - 2>(), mask);

        return mask;
    }

    template <class Component>
    bool HasComponent(const Entity* entity) const
    {
        EnsureValidComponentType<Component>();

        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);

        return entity && m_entities.GetEntityData(entity->Id()).HasComponent<Component>();
    }

    bool HasComponent(TypeId componentTypeId, const Entity* entity) const
    {
        EnsureValidComponentType(componentTypeId);

        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);

        return entity && m_entities.GetEntityData(entity->Id()).HasComponent(componentTypeId);
    }

    template <class Component>
    HYP_FORCE_INLINE Component& GetComponent(const Entity* entity)
    {
        EnsureValidComponentType<Component>();

        Assert(entity, "Invalid entity");

        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);
        HYP_MT_CHECK_READ(m_containersDataRaceDetector);

        EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
        Assert(entityData != nullptr, "Entity does not exist");

        const Optional<ComponentId> componentIdOpt = entityData->TryGetComponentId<Component>();
        Assert(componentIdOpt.HasValue(), "Entity does not have component of type {}", TypeNameWithoutNamespace<Component>().Data());

        static const TypeId s_componentTypeId = TypeId::ForType<Component>();

        auto componentContainerIt = m_containers.Find(s_componentTypeId);
        Assert(componentContainerIt != m_containers.End(), "Component container does not exist");

        HYP_MT_CHECK_READ(componentContainerIt->second->GetDataRaceDetector());

        return static_cast<ComponentContainer<Component>&>(*componentContainerIt->second).GetComponent(*componentIdOpt);
    }

    template <class Component>
    HYP_FORCE_INLINE const Component& GetComponent(const Entity* entity) const
    {
        return const_cast<EntityManager*>(this)->GetComponent<Component>(entity);
    }

    template <class Component>
    Component* TryGetComponent(const Entity* entity)
    {
        EnsureValidComponentType<Component>();

        if (!entity)
        {
            return nullptr;
        }

        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);
        HYP_MT_CHECK_READ(m_containersDataRaceDetector);

        EntityData* entityData = m_entities.TryGetEntityData(entity->Id());

        if (!entityData)
        {
            return nullptr;
        }

        if (!entityData->HasComponent<Component>())
        {
            return nullptr;
        }

        static const TypeId s_componentTypeId = TypeId::ForType<Component>();

        const Optional<ComponentId> componentIdOpt = entityData->TryGetComponentId<Component>();

        if (!componentIdOpt)
        {
            return nullptr;
        }

        auto componentContainerIt = m_containers.Find(s_componentTypeId);
        if (componentContainerIt == m_containers.End())
        {
            return nullptr;
        }

        HYP_MT_CHECK_READ(componentContainerIt->second->GetDataRaceDetector());

        return &static_cast<ComponentContainer<Component>&>(*componentContainerIt->second).GetComponent(*componentIdOpt);
    }

    template <class Component>
    HYP_FORCE_INLINE const Component* TryGetComponent(const Entity* entity) const
    {
        return const_cast<EntityManager*>(this)->TryGetComponent<Component>(entity);
    }

    /*! \brief Gets a component using the dynamic type Id.
     *
     *  \param[in] componentTypeId The type Id of the component to get.
     *  \param[in] entity The Entity to get the component from.
     *
     *  \return Pointer to the component as a void pointer, or nullptr if the entity does not have the component.
     */
    AnyRef TryGetComponent(TypeId componentTypeId, const Entity* entity)
    {
        EnsureValidComponentType(componentTypeId);

        if (!entity)
        {
            return AnyRef::Empty();
        }

        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);
        HYP_MT_CHECK_READ(m_containersDataRaceDetector);

        EntityData* entityData = m_entities.TryGetEntityData(entity->Id());

        if (!entityData)
        {
            return AnyRef::Empty();
        }

        const Optional<ComponentId> componentIdOpt = entityData->TryGetComponentId(componentTypeId);

        if (!componentIdOpt)
        {
            return AnyRef::Empty();
        }

        auto componentContainerIt = m_containers.Find(componentTypeId);
        Assert(componentContainerIt != m_containers.End(), "Component container does not exist");

        return componentContainerIt->second->TryGetComponent(*componentIdOpt);
    }

    /*! \brief Gets a component using the dynamic type Id.
     *
     *  \param[in] componentTypeId The type Id of the component to get.
     *  \param[in] entity The entity to get the component from.
     *
     *  \return Pointer to the component as a void pointer, or nullptr if the entity does not have the component.
     */
    HYP_FORCE_INLINE ConstAnyRef TryGetComponent(TypeId componentTypeId, const Entity* entity) const
    {
        return const_cast<EntityManager*>(this)->TryGetComponent(componentTypeId, entity);
    }

    template <class... Components>
    HYP_FORCE_INLINE Tuple<Components*...> TryGetComponents(const Entity* entity)
    {
        return Tuple<Components*...>(TryGetComponent<Components>(entity)...);
    }

    template <class... Components>
    HYP_FORCE_INLINE Tuple<const Components*...> TryGetComponents(const Entity* entity) const
    {
        return Tuple<const Components*...>(TryGetComponent<Components>(entity)...);
    }

    template <class... Components>
    HYP_FORCE_INLINE Tuple<Components&...> GetComponents(const Entity* entity)
    {
        return Tie(GetComponent<Components>(entity)...);
    }

    template <class... Components>
    HYP_FORCE_INLINE Tuple<const Components&...> GetComponents(const Entity* entity) const
    {
        return Tie(GetComponent<Components>(entity)...);
    }

    /*! \brief Get a map of all component types to respective component IDs for a given Entity.
     *  \param entity The Entity to get the components from
     *  \returns An Optional object holding a reference to the typemap if it exists, otherwise an empty optional. */
    HYP_FORCE_INLINE Optional<const ComponentMap&> GetAllComponents(const Entity* entity) const
    {
        if (!entity)
        {
            return {};
        }

        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        const EntityData* entityData = m_entities.TryGetEntityData(entity->Id());

        if (!entityData)
        {
            return {};
        }

        return entityData->components;
    }

    void AddComponent(Entity* entity, const BoxedValue& componentData);
    void AddComponent(Entity* entity, BoxedValue&& componentData);

    bool RemoveComponent(TypeId componentTypeId, Entity* entity);

    template <class Component, class U = Component>
    Component& AddComponent(Entity* entity, U&& component)
    {
        EnsureValidComponentType<Component>();

        Assert(entity, "Invalid entity");
        Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

        Handle<Entity> entityHandle = MakeStrongRef(entity);
        Assert(entityHandle.IsValid());

        EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
        Assert(entityData != nullptr);

        Component* componentPtr = nullptr;
        ComponentMap componentIds;

        auto componentIt = entityData->FindComponent<Component>();
        /// \todo : Replace the component if it already exists
        Assert(componentIt == entityData->components.End(), "Entity already has component of type {}", TypeNameWithoutNamespace<Component>().Data());

        static const TypeId componentTypeId = TypeId::ForType<Component>();

        const Pair<ComponentId, Component&> componentInsertResult = GetContainer<Component>().AddComponent(std::move(component));

        entityData->components.Set<Component>(componentInsertResult.first);

        { // Lock the entity sets mutex
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

        componentPtr = &componentInsertResult.second;
        componentIds = entityData->components;

        // Note: Call OnComponentAdded on the entity before notifying systems, as systems may remove the component
        EntityTag tag;
        if (IsEntityTagComponent(componentTypeId, tag))
        {
            // If the component is an TagComponent, add the tag to the entity
            entity->OnTagAdded(tag);
        }
        else
        {
            // Notify the entity that a component was added
            entity->OnComponentAdded(AnyRef(componentPtr));
        }

        // Notify systems that entity is being added to them
        NotifySystemsOfEntityAdded(entityHandle, componentIds);

        return *componentPtr;
    }

    template <class Component>
    bool RemoveComponent(Entity* entity)
    {
        EnsureValidComponentType<Component>();

        if (!entity)
        {
            return false;
        }

        Handle<Entity> entityHandle = MakeStrongRef(entity);
        Assert(entityHandle.IsValid());

        Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

        ComponentMap removedComponents;

        EntityData* entityData = m_entities.TryGetEntityData(entity->Id());

        if (!entityData)
        {
            return false;
        }

        auto componentIt = entityData->FindComponent<Component>();
        if (componentIt == entityData->components.End())
        {
            return false;
        }

        const TypeId componentTypeId = componentIt->first;
        const ComponentId componentId = componentIt->second;

        // Notify systems that entity is being removed from them
        removedComponents.Set(componentTypeId, componentId);

        BoxedValue componentHypData;

        if (!GetContainer<Component>().RemoveComponent(componentId, componentHypData))
        {
            return false;
        }

        entityData->components.Erase(componentIt);

        {
            auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

            if (componentEntitySetsIt != m_componentEntitySets.End())
            {
                for (EntitySetId entitySetId : componentEntitySetsIt->second)
                {
                    EntitySetBase& entitySet = *m_entitySets.At(entitySetId);

                    entitySet.OnEntityUpdated(entityHandle);
                }
            }
        }

        NotifySystemsOfEntityRemoved(entity, removedComponents);

        EntityTag tag;
        if (IsEntityTagComponent(componentTypeId, tag))
        {
            // If the component is an TagComponent, remove the tag from the entity
            entity->OnTagRemoved(tag);
        }
        else
        {
            // Notify the entity that a component was removed
            entity->OnComponentRemoved(componentHypData.ToRef());
        }

        componentHypData.Reset();

        return true;
    }

    /*! \brief Gets an entity set with the specified components, creating it if it doesn't exist.
     *  This method is thread-safe, and can be used within Systems running in task threads.
     *
     *  \tparam Components The components that the entities in this set have.
     *
     *  \param[in] entities The entity container to use.
     *  \param[in] components The component containers to use.
     *
     *  \return Reference to the entity set.
     */
    template <class... Components>
    EntitySet<Components...>& GetEntitySet()
    {
        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        const EntitySetId entitySetId = GetEntitySetId<Components...>();

        auto entitySetsIt = m_entitySets.Find(entitySetId);

        if (entitySetsIt == m_entitySets.End())
        {
            if (IsLocked())
            {
                return GetOrCreatePendingEntitySet<Components...>();
            }

            auto entitySetsInsertResult = m_entitySets.Set(
                entitySetId,
                MakeUnique<EntitySet<Components...>>(m_entities, GetContainer<Components>()...));

            Assert(entitySetsInsertResult.second); // Make sure the element was inserted (it shouldn't already exist)

            entitySetsIt = entitySetsInsertResult.first;

            if constexpr (sizeof...(Components) > 0)
            {
                // Make sure the element exists in m_componentEntitySets
                for (TypeId componentTypeId : FixedArray<TypeId, sizeof...(Components)> { TypeId::ForType<Components>()... })
                {
                    auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

                    if (componentEntitySetsIt == m_componentEntitySets.End())
                    {
                        auto componentEntitySetsInsertResult = m_componentEntitySets.Set(componentTypeId, {});

                        componentEntitySetsIt = componentEntitySetsInsertResult.first;
                    }

                    componentEntitySetsIt->second.Insert(entitySetId);
                }
            }
        }

        return static_cast<EntitySet<Components...>&>(*entitySetsIt->second);
    }

    template <class... Components>
    EntitySet<Components...>* TryGetEntitySet()
    {
        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        const EntitySetId entitySetId = GetEntitySetId<Components...>();

        auto entitySetsIt = m_entitySets.Find(entitySetId);

        if (entitySetsIt == m_entitySets.End())
        {
            if (IsLocked())
            {
                return static_cast<EntitySet<Components...>*>(TryGetPendingEntitySet(entitySetId));
            }

            return nullptr;
        }

        return static_cast<EntitySet<Components...>*>(entitySetsIt->second.Get());
    }

    EntitySetBase* TryGetEntitySet(EntitySetId entitySetId)
    {
        Assert(IsLocked() || IsOnThread(m_ownerThreadId));

        auto entitySetsIt = m_entitySets.Find(entitySetId);

        if (entitySetsIt == m_entitySets.End())
        {
            if (IsLocked())
            {
                return TryGetPendingEntitySet(entitySetId);
            }

            return nullptr;
        }

        return entitySetsIt->second.Get();
    }

    template <class Callback>
    void ForEachEntity(Callback&& callback) const
    {
        Assert(!IsLocked() && IsOnThread(m_ownerThreadId));

        for (auto& subtypeData : m_entities.GetSubtypeData())
        {
            for (auto entitiesIt = subtypeData.data.Begin(); entitiesIt != subtypeData.data.End(); ++entitiesIt)
            {
                EntityData& entityData = *entitiesIt;

                Entity* entity = entityData.entityWeak.GetUnsafe();
                Assert(entity != nullptr);

                callback(entity);
            }
        }
    }

    void Shutdown();

    void UpdateEntities(float delta);

    void AddPendingEntitySets();

    template <class Component>
    ComponentContainer<Component>& GetContainer()
    {
        EnsureValidComponentType<Component>();

        Mutex::Guard guard(m_componentContainersMtx);

        auto it = m_containers.Find<Component>();

        if (it == m_containers.End())
        {
            it = m_containers.Set<Component>(MakeUnique<ComponentContainer<Component>>()).first;
        }

        return static_cast<ComponentContainer<Component>&>(*it->second);
    }

    ComponentContainerBase* TryGetContainer(TypeId componentTypeId)
    {
        EnsureValidComponentType(componentTypeId);

        Mutex::Guard guard(m_componentContainersMtx);

        auto it = m_containers.Find(componentTypeId);

        if (it == m_containers.End())
        {
            return nullptr;
        }

        return it->second.Get();
    }

private:
    void Init() override;

    HYP_METHOD()
    Handle<Entity> AddBasicEntity();

    void AddExistingEntity_Internal(const Handle<Entity>& entity);

    template <class Component>
    static void EnsureValidComponentType()
    {
        AssertDebug(IsValidComponentType<Component>());
    }

    static void EnsureValidComponentType(TypeId componentTypeId)
    {
        AssertDebug(IsValidComponentType(componentTypeId), "Invalid component type: TypeId({})", componentTypeId.Value());
    }

    template <uint32... Indices>
    HYP_FORCE_INLINE void GetTagsHelper(const Entity* entity, std::integer_sequence<uint32, Indices...>, Array<EntityTag>& outTags) const
    {
        ((HasTag<EntityTag(Indices + 1)>(entity) ? (void)(outTags.PushBack(EntityTag(Indices + 1))) : void()), ...);
    }

    template <uint32... Indices>
    HYP_FORCE_INLINE void GetTagsHelper(const Entity* entity, std::integer_sequence<uint32, Indices...>, uint32& outMask) const
    {
        ((HasTag<EntityTag(Indices + 1)>(entity) ? (void)(outMask |= (1u << uint32(Indices))) : void()), ...);
    }

    void NotifySystemOfExistingEntities(SystemBase* system);
    void NotifySystemOfAllEntitiesRemoved(SystemBase* system);
    void NotifySystemsOfEntityAdded(const Handle<Entity>& entity, const ComponentMap& componentIds);
    void NotifySystemsOfEntityRemoved(Entity* entity, const ComponentMap& componentIds);

    /*! \brief Removes an entity from the EntityManager.

     *  \return True if the entity was removed, false otherwise.
     */
    bool RemoveEntity(Entity* entity);

    bool IsEntityInitializedForSystem(SystemBase* system, const Entity* entity) const;

    // Thread safe way to create new entity set if one doesn't exist
    // will look for an existing pending one to prevent dupes
    template <class... Components>
    EntitySet<Components...>& GetOrCreatePendingEntitySet()
    {
        Mutex::Guard guard(m_pendingEntitySetsMtx);

        const EntitySetId entitySetId = GetEntitySetId<Components...>();

        auto it = m_pendingEntitySets.Find(entitySetId);

        if (it == m_pendingEntitySets.End())
        {
            auto insertResult = m_pendingEntitySets.Insert(
                entitySetId,
                MakeUnique<EntitySet<Components...>>(m_entities, GetContainer<Components>()...));

            Assert(insertResult.second);

            it = insertResult.first;
        }

        return static_cast<EntitySet<Components...>&>(*it->second);
    }

    EntitySetBase* TryGetPendingEntitySet(EntitySetId entitySetId)
    {
        Mutex::Guard guard(m_pendingEntitySetsMtx);

        auto it = m_pendingEntitySets.Find(entitySetId);

        if (it != m_pendingEntitySets.End())
        {
            return it->second.Get();
        }

        return nullptr;
    }

    ThreadId m_ownerThreadId;
    World* m_world;
    Scene* m_scene;
    EnumFlags<EntityManagerFlags> m_flags;

    TypeMap<UniquePtr<ComponentContainerBase>> m_containers;
    DataRaceDetector m_containersDataRaceDetector;
    mutable Mutex m_componentContainersMtx;

    EntityContainer m_entities;
    DataRaceDetector m_entitiesDataRaceDetector;

    HashMap<EntitySetId, UniquePtr<EntitySetBase>> m_entitySets;

    TypeMap<HashSet<EntitySetId>> m_componentEntitySets;

    // thread safe map of entity sets not yet added to m_entitySets
    // that will be added upon synchronization
    HashMap<EntitySetId, UniquePtr<EntitySetBase>> m_pendingEntitySets;
    mutable Mutex m_pendingEntitySetsMtx;

    Array<SystemExecutionGroup*> m_systemExecutionGroups;

    HashMap<SystemBase*, HashSet<Entity*>> m_systemEntityMap;
    mutable Mutex m_systemEntityMapMutex;

    bool m_isLocked;
};

} // namespace Hyperion
