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

#include <core/object/HypObject.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/object/Handle.hpp>
#include <core/object/ObjId.hpp>

#include <scene/Entity.hpp>
#include <scene/EntitySet.hpp>
#include <scene/EntityContainer.hpp>
#include <scene/ComponentContainer.hpp>
#include <scene/System.hpp>
#include <scene/EntityTag.hpp>

namespace hyperion {

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

enum class EntityManagerFlags : uint32
{
    NONE = 0x0,
    PARALLEL_SYSTEM_EXECUTION = 0x1,

    DEFAULT = PARALLEL_SYSTEM_EXECUTION
};

HYP_MAKE_ENUM_FLAGS(EntityManagerFlags)

class World;
class Scene;
struct HypData;
class Node;

static constexpr uint32 g_moveEntityWriteFlag = 0x1u;
static constexpr uint32 g_moveEntityReadMask = ~0u << 1;

/*! \brief A group of Systems that are able to be processed concurrently, as they do not share any dependencies.
 */
class HYP_API SystemExecutionGroup
{
public:
    SystemExecutionGroup(bool requiresGameThread = false, bool allowUpdate = true);
    SystemExecutionGroup(const SystemExecutionGroup&) = delete;
    SystemExecutionGroup& operator=(const SystemExecutionGroup&) = delete;
    SystemExecutionGroup(SystemExecutionGroup&&) noexcept = default;
    SystemExecutionGroup& operator=(SystemExecutionGroup&&) noexcept = default;
    ~SystemExecutionGroup();

    HYP_FORCE_INLINE bool RequiresGameThread() const
    {
        return m_requiresGameThread;
    }

    HYP_FORCE_INLINE bool AllowUpdate() const
    {
        return m_allowUpdate;
    }

    HYP_FORCE_INLINE TypeMap<Handle<SystemBase>>& GetSystems()
    {
        return m_systems;
    }

    HYP_FORCE_INLINE const TypeMap<Handle<SystemBase>>& GetSystems() const
    {
        return m_systems;
    }

    HYP_FORCE_INLINE TaskBatch* GetTaskBatch() const
    {
        return m_taskBatch.Get();
    }

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE const PerformanceClock& GetPerformanceClock() const
    {
        return m_performanceClock;
    }

    HYP_FORCE_INLINE const FlatMap<SystemBase*, PerformanceClock>& GetPerformanceClocks() const
    {
        return m_performanceClocks;
    }
#endif

    /*! \brief Checks if the SystemExecutionGroup is valid for the given System.
     *
     *  \param[in] systemPtr The System to check.
     *
     *  \return True if the SystemExecutionGroup is valid for the given System, false otherwise.
     */
    bool IsValidForSystem(const SystemBase* systemPtr) const
    {
        Assert(systemPtr != nullptr);

        // If the system does not allow update calls, and we don't as well, return true as there will be no overlap.
        if (!AllowUpdate())
        {
            return !systemPtr->AllowUpdate();
        }

        // If the system requires to execute on game thread and the SystemExecutionGroup does not, it is not valid
        // and if the system does not require to execute on game thread and the SystemExecutionGroup does, it is not valid (it could be better parallelized)
        if (systemPtr->RequiresGameThread() != RequiresGameThread())
        {
            return false;
        }

        const Array<TypeId>& componentTypeIds = systemPtr->GetComponentTypeIds();

        for (const auto& it : m_systems)
        {
            const SystemBase* otherSystem = it.second.Get();

            for (TypeId componentTypeId : componentTypeIds)
            {
                const ComponentInfo& componentInfo = systemPtr->GetComponentInfo(componentTypeId);

                if (componentInfo.rwFlags & COMPONENT_RW_FLAGS_WRITE)
                {
                    if (otherSystem->HasComponentTypeId(componentTypeId, true))
                    {
                        return false;
                    }
                }
                else
                {
                    // This System is read-only for this component, so it can be processed with other Systems
                    if (otherSystem->HasComponentTypeId(componentTypeId, false))
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    /*! \brief Checks if the SystemExecutionGroup has a System of the given type.
     *
     *  \tparam SystemType The type of the System to check for.
     *
     *  \return True if the SystemExecutionGroup has a System of the given type, false otherwise.
     */
    template <class SystemType>
    HYP_FORCE_INLINE bool HasSystem() const
    {
        static const TypeId typeId = TypeId::ForType<SystemType>();

        return m_systems.Find(typeId) != m_systems.End();
    }

    /*! \brief Adds a System to the SystemExecutionGroup.
     *
     *  \param[in] system The System to add.
     */
    Handle<SystemBase> AddSystem(const Handle<SystemBase>& system)
    {
        Assert(system.IsValid());
        Assert(IsValidForSystem(system.Get()), "System is not valid for this SystemExecutionGroup");

        auto it = m_systems.Find(system->GetTypeId());
        Assert(it == m_systems.End(), "System already exists");

        auto insertResult = m_systems.Set(system->GetTypeId(), system);

        return insertResult.first->second;
    }

    template <class SystemType>
    Handle<SystemType> GetSystem() const
    {
        static const TypeId typeId = TypeId::ForType<SystemType>();

        const auto it = m_systems.Find(typeId);

        if (it == m_systems.End() || !it->second.IsValid())
        {
            return Handle<SystemType>::empty;
        }

        if (!IsA<SystemType>(*it->second))
        {
            HYP_BREAKPOINT;
            return Handle<SystemType>::empty;
        }

        return Handle<SystemType>(it->second);
    }

    /*! \brief Removes a System from the SystemExecutionGroup.
     *
     *  \tparam SystemType The type of the System to remove.
     *
     *  \return True if the System was removed, false otherwise.
     */
    template <class SystemType>
    bool RemoveSystem()
    {
        static const TypeId typeId = TypeId::ForType<SystemType>();

        return m_systems.Erase(typeId);
    }

    /*! \brief Start processing all Systems in the SystemExecutionGroup.
     *
     *  \param[in] delta The delta time value
     */
    void StartProcessing(float delta);

    /*! \brief Waits on all processing tasks to complete */
    void FinishProcessing(bool executeBlocking = false);

private:
    bool m_requiresGameThread;
    bool m_allowUpdate;

    TypeMap<Handle<SystemBase>> m_systems;
    UniquePtr<TaskBatch> m_taskBatch;

#ifdef HYP_DEBUG_MODE
    PerformanceClock m_performanceClock;
    FlatMap<SystemBase*, PerformanceClock> m_performanceClocks;
#endif
};

class EntityManager;
/*! \brief The EntityManager is responsible for managing Entities, their components, and Systems within a Scene. */
HYP_CLASS()
class HYP_API EntityManager final : public HypObjectBase
{
    HYP_OBJECT_BODY(EntityManager);

    friend class EntityToEntityManagerMap;

    // Allow Entity destructor to call RemoveEntity().
    friend class Entity;

public:
    static constexpr ComponentId invalidComponentId = 0;

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

    HYP_FORCE_INLINE EntityContainer& GetEntities()
    {
        return m_entities;
    }

    HYP_FORCE_INLINE const EntityContainer& GetEntities() const
    {
        return m_entities;
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

        Handle<T> entity = CreateObject<T>(std::forward<Args>(args)...);
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
    Handle<Entity> AddTypedEntity(const HypClass* hypClass);

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
        Threads::AssertOnThread(m_ownerThreadId);


        return id.IsValid() && m_entities.HasEntity(id);
    }

    void AddTag(Entity* entity, EntityTag tag);
    bool RemoveTag(Entity* entity, EntityTag tag);
    bool HasTag(const Entity* entity, EntityTag tag) const;

    template <EntityTag Tag>
    HYP_FORCE_INLINE bool HasTag(const Entity* entity) const
    {
        return HasComponent<EntityTagComponent<Tag>>(entity);
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
            if (tag == EntityTag::NONE || uint32(tag) >= uint32(EntityTag::TYPE_ID))
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

        return RemoveComponent<EntityTagComponent<Tag>>(entity);
    }

    HYP_FORCE_INLINE Array<EntityTag> GetSavableTags(const Entity* entity) const
    {
        Array<EntityTag> tags;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::SAVABLE_MAX) - 2>(), tags);

        return tags;
    }

    HYP_FORCE_INLINE uint32 GetSavableTagsMask(const Entity* entity) const
    {
        uint32 mask = 0;
        GetTagsHelper(entity, std::make_integer_sequence<uint32, uint32(EntityTag::SAVABLE_MAX) - 2>(), mask);

        return mask;
    }

    template <class Component>
    bool HasComponent(const Entity* entity) const
    {
        EnsureValidComponentType<Component>();

        // Threads::AssertOnThread(m_ownerThreadId);

        HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);

        return entity && m_entities.GetEntityData(entity->Id()).HasComponent<Component>();
    }

    bool HasComponent(TypeId componentTypeId, const Entity* entity) const
    {
        EnsureValidComponentType(componentTypeId);

        // Threads::AssertOnThread(m_ownerThreadId);

        HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);

        return entity && m_entities.GetEntityData(entity->Id()).HasComponent(componentTypeId);
    }

    template <class Component>
    HYP_FORCE_INLINE Component& GetComponent(const Entity* entity)
    {
        EnsureValidComponentType<Component>();

        Assert(entity, "Invalid entity");

        // Threads::AssertOnThread(m_ownerThreadId);

        HYP_MT_CHECK_READ(m_entitiesDataRaceDetector);
        HYP_MT_CHECK_READ(m_containersDataRaceDetector);

        EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
        Assert(entityData != nullptr, "Entity does not exist");

        const Optional<ComponentId> componentIdOpt = entityData->TryGetComponentId<Component>();
        Assert(componentIdOpt.HasValue(), "Entity does not have component of type {}", TypeNameWithoutNamespace<Component>().Data());

        static const TypeId componentTypeId = TypeId::ForType<Component>();

        auto componentContainerIt = m_containers.Find(componentTypeId);
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

        // Threads::AssertOnThread(m_ownerThreadId);

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

        static const TypeId componentTypeId = TypeId::ForType<Component>();

        const Optional<ComponentId> componentIdOpt = entityData->TryGetComponentId<Component>();

        if (!componentIdOpt)
        {
            return nullptr;
        }

        auto componentContainerIt = m_containers.Find(componentTypeId);
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

        // Threads::AssertOnThread(m_ownerThreadId);

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
    HYP_FORCE_INLINE Optional<const TypeMap<ComponentId>&> GetAllComponents(const Entity* entity) const
    {
        if (!entity)
        {
            return {};
        }

        Threads::AssertOnThread(m_ownerThreadId);

        const EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
        
        if (!entityData)
        {
            return {};
        }
        
        return entityData->components;
    }

    void AddComponent(Entity* entity, const HypData& componentData);
    void AddComponent(Entity* entity, HypData&& componentData);

    bool RemoveComponent(TypeId componentTypeId, Entity* entity);

    template <class Component, class U = Component>
    Component& AddComponent(Entity* entity, U&& component)
    {
        EnsureValidComponentType<Component>();

        Assert(entity, "Invalid entity");

        Threads::AssertOnThread(m_ownerThreadId);

        Handle<Entity> entityHandle = MakeStrongRef(entity);
        Assert(entityHandle.IsValid());

        EntityData* entityData = m_entities.TryGetEntityData(entity->Id());
        Assert(entityData != nullptr);

        Component* componentPtr = nullptr;
        TypeMap<ComponentId> componentIds;

        auto componentIt = entityData->FindComponent<Component>();
        // @TODO: Replace the component if it already exists
        Assert(componentIt == entityData->components.End(), "Entity already has component of type {}", TypeNameWithoutNamespace<Component>().Data());

        static const TypeId componentTypeId = TypeId::ForType<Component>();

        const Pair<ComponentId, Component&> componentInsertResult = GetContainer<Component>().AddComponent(std::move(component));

        entityData->components.Set<Component>(componentInsertResult.first);

        { // Lock the entity sets mutex
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
        }

        componentPtr = &componentInsertResult.second;
        componentIds = entityData->components;

        // Note: Call OnComponentAdded on the entity before notifying systems, as systems may remove the component
        EntityTag tag;
        if (IsEntityTagComponent(componentTypeId, tag))
        {
            // If the component is an EntityTagComponent, add the tag to the entity
            entity->OnTagAdded(tag);
        }
        else
        {
            // Notify the entity that a component was added
            entity->OnComponentAdded(componentPtr);
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

        Threads::AssertOnThread(m_ownerThreadId);

        TypeMap<ComponentId> removedComponentIds;

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
        removedComponentIds.Set(componentTypeId, componentId);

        HypData componentHypData;

        if (!GetContainer<Component>().RemoveComponent(componentId, componentHypData))
        {
            return false;
        }

        entityData->components.Erase(componentIt);

        {
            // Lock the entity sets mutex
            Mutex::Guard entitySetsGuard(m_entitySetsMutex);

            auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

            if (componentEntitySetsIt != m_componentEntitySets.End())
            {
                for (TypeId entitySetTypeId : componentEntitySetsIt->second)
                {
                    EntitySetBase& entitySet = *m_entitySets.At(entitySetTypeId);

                    entitySet.OnEntityUpdated(entityHandle);
                }
            }
        }

        NotifySystemsOfEntityRemoved(entity, removedComponentIds);

        EntityTag tag;
        if (IsEntityTagComponent(componentTypeId, tag))
        {
            // If the component is an EntityTagComponent, remove the tag from the entity
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
        Mutex::Guard guard(m_entitySetsMutex);

        static const TypeId entitySetTypeId = TypeId::ForType<EntitySet<Components...>>();

        auto entitySetsIt = m_entitySets.Find(entitySetTypeId);

        if (entitySetsIt == m_entitySets.End())
        {
            auto entitySetsInsertResult = m_entitySets.Set(
                entitySetTypeId,
                MakeUnique<EntitySet<Components...>>(m_entities, GetContainer<Components>()...));

            Assert(entitySetsInsertResult.second); // Make sure the element was inserted (it shouldn't already exist)

            entitySetsIt = entitySetsInsertResult.first;

            if constexpr (sizeof...(Components) > 0)
            {
                // Make sure the element exists in m_componentEntitySets
                for (TypeId componentTypeId : { TypeId::ForType<Components>()... })
                {
                    auto componentEntitySetsIt = m_componentEntitySets.Find(componentTypeId);

                    if (componentEntitySetsIt == m_componentEntitySets.End())
                    {
                        auto componentEntitySetsInsertResult = m_componentEntitySets.Set(componentTypeId, {});

                        componentEntitySetsIt = componentEntitySetsInsertResult.first;
                    }

                    componentEntitySetsIt->second.Insert(entitySetTypeId);
                }
            }
        }

        return static_cast<EntitySet<Components...>&>(*entitySetsIt->second);
    }

    HYP_METHOD()
    HYP_FORCE_INLINE Handle<SystemBase> AddSystem(const Handle<SystemBase>& system)
    {
        if (!system.IsValid())
        {
            return Handle<SystemBase>::empty;
        }

        return AddSystemToExecutionGroup(system);
    }

    template <class SystemType>
    Handle<SystemType> GetSystem() const
    {
        for (const SystemExecutionGroup& systemExecutionGroup : m_systemExecutionGroups)
        {
            if (Handle<SystemType> system = systemExecutionGroup.GetSystem<SystemType>())
            {
                return system;
            }
        }

        return Handle<SystemType>::empty;
    }

    HYP_METHOD()
    Handle<SystemBase> GetSystemByTypeId(TypeId systemTypeId) const
    {
        for (const SystemExecutionGroup& systemExecutionGroup : m_systemExecutionGroups)
        {
            for (const auto& it : systemExecutionGroup.GetSystems())
            {
                if (it.first == systemTypeId)
                {
                    return it.second;
                }
            }
        }

        return Handle<SystemBase>::empty;
    }

    template <class Callback>
    HYP_FORCE_INLINE void ForEachEntity(Callback&& callback) const
    {
        Threads::AssertOnThread(m_ownerThreadId);
        
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

    void BeginAsyncUpdate(float delta);
    void EndAsyncUpdate();

    template <class Component>
    HYP_FORCE_INLINE ComponentContainer<Component>& GetContainer()
    {
        EnsureValidComponentType<Component>();

        HYP_MT_CHECK_READ(m_containersDataRaceDetector);

        auto it = m_containers.Find<Component>();

        if (it == m_containers.End())
        {
            it = m_containers.Set<Component>(MakeUnique<ComponentContainer<Component>>()).first;
        }

        return static_cast<ComponentContainer<Component>&>(*it->second);
    }

    HYP_FORCE_INLINE ComponentContainerBase* TryGetContainer(TypeId componentTypeId)
    {
        EnsureValidComponentType(componentTypeId);

        HYP_MT_CHECK_READ(m_containersDataRaceDetector);

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
        AssertDebug(IsValidComponentType<Component>(), "Invalid component type: {}", TypeName<Component>().Data());
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

    Handle<SystemBase> AddSystemToExecutionGroup(const Handle<SystemBase>& system)
    {
        Assert(system.IsValid());
        Assert(system->m_entityManager == nullptr || system->m_entityManager == this);

        system->InitComponentInfos_Internal();

        bool wasAdded = false;

        if ((m_flags & EntityManagerFlags::PARALLEL_SYSTEM_EXECUTION) && system->AllowParallelExecution())
        {
            for (SystemExecutionGroup& systemExecutionGroup : m_systemExecutionGroups)
            {
                if (systemExecutionGroup.IsValidForSystem(system.Get()))
                {
                    if (systemExecutionGroup.AddSystem(system))
                    {
                        wasAdded = true;

                        break;
                    }
                }
            }
        }

        if (!wasAdded)
        {
            SystemExecutionGroup& systemExecutionGroup = m_systemExecutionGroups.EmplaceBack(system->RequiresGameThread(), system->AllowUpdate());

            if (systemExecutionGroup.AddSystem(system))
            {
                wasAdded = true;
            }
        }

        system->m_entityManager = this;

        // If the EntityManager is initialized, call Initialize() on the System.
        if (IsInitCalled() && wasAdded)
        {
            if (m_world != nullptr)
            {
                InitializeSystem(system);
            }
        }

        return system;
    }

    void InitializeSystem(const Handle<SystemBase>& system);
    void ShutdownSystem(const Handle<SystemBase>& system);

    void NotifySystemsOfEntityAdded(const Handle<Entity>& entity, const TypeMap<ComponentId>& componentIds);
    void NotifySystemsOfEntityRemoved(Entity* entity, const TypeMap<ComponentId>& componentIds);

    /*! \brief Removes an entity from the EntityManager.
     *
     *  \param[in] entityId The ID of the entity to remove.
     *
     *  \return True if the entity was removed, false otherwise.
     */
    bool RemoveEntity(ObjId<Entity> entityId);

    bool IsEntityInitializedForSystem(SystemBase* system, const Entity* entity) const;

    void GetSystemClasses(Array<const HypClass*>& outClasses) const;

    ThreadId m_ownerThreadId;
    World* m_world;
    Scene* m_scene;
    EnumFlags<EntityManagerFlags> m_flags;

    TypeMap<UniquePtr<ComponentContainerBase>> m_containers;
    DataRaceDetector m_containersDataRaceDetector;
    EntityContainer m_entities;
    DataRaceDetector m_entitiesDataRaceDetector;
    HashMap<TypeId, UniquePtr<EntitySetBase>> m_entitySets;
    mutable Mutex m_entitySetsMutex;
    TypeMap<HashSet<TypeId>> m_componentEntitySets;

    Array<SystemExecutionGroup> m_systemExecutionGroups;

    SystemExecutionGroup* m_rootSynchronousExecutionGroup;

    HashMap<SystemBase*, HashSet<Entity*>> m_systemEntityMap;
    mutable Mutex m_systemEntityMapMutex;
};

} // namespace hyperion

