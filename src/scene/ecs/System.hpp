/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_SYSTEM_HPP
#define HYPERION_ECS_SYSTEM_HPP

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/TypeID.hpp>
#include <core/utilities/Tuple.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/functional/Delegate.hpp>

#include <core/Defines.hpp>

#include <scene/ecs/ComponentContainer.hpp>

#include <GameCounter.hpp>

namespace hyperion {

class EntityManager;
class Scene;

enum class SceneFlags : uint32;

class HYP_API SystemBase
{
public:
    friend class EntityManager;
    friend class SystemExecutionGroup;

    virtual ~SystemBase() = default;

    virtual ANSIStringView GetName() const = 0;

    virtual TypeID GetTypeID() const = 0;

    virtual bool ShouldCreateForScene(Scene* scene) const
    {
        return true;
    }

    virtual bool AllowParallelExecution() const
    {
        return true;
    }

    virtual bool RequiresGameThread() const
    {
        return false;
    }

    virtual bool AllowUpdate() const
    {
        return true;
    }

    /*! \brief Returns the TypeIDs of the components this System operates on.
     *  To be used by the EntityManager in order to properly order the Systems based on their dependencies.
     *
     *  \return The TypeIDs of the components this System operates on.
     */
    HYP_FORCE_INLINE const Array<TypeID>& GetComponentTypeIDs() const
    {
        return m_component_type_ids;
    }

    /*! \brief Returns the ComponentInfo objects of the components this System operates on.
     *
     *  \return The ComponentInfos of the components this System operates on.
     */
    HYP_FORCE_INLINE const Array<ComponentInfo>& GetComponentInfos() const
    {
        return m_component_infos;
    }

    /*! \brief Returns true if all given TypeIDs are operated on by this System, false otherwise.
     *
     *  \param component_type_ids The TypeIDs of the components to check.
     *  \param receive_events_context If true, this function will skip components that do not receive events for this System.
     *
     *  \return True if all given TypeIDs are operated on by this System, false otherwise.
     */
    bool ActsOnComponents(const Array<TypeID>& component_type_ids, bool receive_events_context) const
    {
        for (const TypeID component_type_id : m_component_type_ids)
        {
            const ComponentInfo& component_info = GetComponentInfo(component_type_id);

            // skip observe-only components
            if (!(component_info.rw_flags & COMPONENT_RW_FLAGS_READ) && !(component_info.rw_flags & COMPONENT_RW_FLAGS_WRITE))
            {
                continue;
            }

            if (receive_events_context && !component_info.receives_events)
            {
                continue;
            }

            if (!component_type_ids.Contains(component_type_id))
            {
                return false;
            }
        }

        return true;
    }

    /*! \brief Returns true if this System operates on the component with the given TypeID, false otherwise.
     *
     *  \param component_type_id The TypeID of the component to check.
     *  \param include_read_only If true, this function will return true even if the component is read-only.
     *  Otherwise, read-only components will be ignored.
     *
     *  \return True if this System operates on the component with the given TypeID, false otherwise.
     */
    bool HasComponentTypeID(TypeID component_type_id, bool include_read_only = true) const
    {
        const bool has_component_type_id = m_component_type_ids.Contains(component_type_id);

        if (!has_component_type_id)
        {
            return false;
        }

        if (include_read_only)
        {
            return true;
        }

        const ComponentInfo& component_info = GetComponentInfo(component_type_id);

        return !!(component_info.rw_flags & COMPONENT_RW_FLAGS_WRITE);
    }

    /*! \brief Returns the ComponentInfo of the component with the given TypeID.
     *
     *  \param component_type_id The TypeID of the component to check.
     *
     *  \return The ComponentInfo of the component with the given TypeID.
     */
    const ComponentInfo& GetComponentInfo(TypeID component_type_id) const
    {
        const auto it = m_component_type_ids.Find(component_type_id);
        AssertThrowMsg(it != m_component_type_ids.End(), "Component type ID not found");

        const SizeType index = m_component_type_ids.IndexOf(it);
        AssertThrowMsg(index != SizeType(-1), "Component type ID not found");

        return m_component_infos[index];
    }

    virtual void OnEntityAdded(const Handle<Entity>& entity)
    {
    }

    virtual void OnEntityRemoved(ID<Entity> entity)
    {
    }

    virtual void Process(GameCounter::TickUnit delta) = 0;

protected:
    SystemBase(EntityManager& entity_manager, Array<TypeID>&& component_type_ids, Array<ComponentInfo>&& component_infos)
        : m_entity_manager(entity_manager),
          m_component_type_ids(std::move(component_type_ids)),
          m_component_infos(std::move(component_infos))
    {
        AssertThrowMsg(m_component_type_ids.Size() == m_component_infos.Size(), "Component type ID count and component infos count mismatch");
    }

    HYP_FORCE_INLINE EntityManager& GetEntityManager()
    {
        return m_entity_manager;
    }

    /*! \brief Set a Proc<void()> to be called once after the System has processed.
     *  The Proc<void()> will be called on the EntityManager's owner thread and will not be parallelized, ensuring proper synchronization.
     *  After the Proc<void()> has been called, it will be reset.
     *
     *  \param proc The Proc<void()> to call after the System has processed.
     */
    void AfterProcess(Proc<void()>&& proc)
    {
        m_after_process_procs.PushBack(std::move(proc));
    }

    Scene* GetScene() const;
    World* GetWorld() const;

    Delegate<void, World* /* new */, World* /* previous */> OnWorldChanged;

    EntityManager& m_entity_manager;

    DelegateHandlerSet m_delegate_handlers;

private:
    void SetWorld(World* world);

    HashSet<WeakHandle<Entity>> m_initialized_entities;

    Array<TypeID> m_component_type_ids;
    Array<ComponentInfo> m_component_infos;

    Array<Proc<void()>, DynamicAllocator> m_after_process_procs;
};

/*! \brief A System is a class that operates on a set of components.
 *
 *  \tparam ComponentDescriptors ComponentDescriptor structs for the Components this System operates on.
 */
template <class Derived, class... ComponentDescriptors>
class System : public SystemBase
{
public:
    using ComponentDescriptorTypes = Tuple<ComponentDescriptors...>;

    System(EntityManager& entity_manager)
        : SystemBase(
              entity_manager,
              { TypeID::ForType<typename ComponentDescriptors::Type>()... },
              { ComponentInfo(ComponentDescriptors())... })
    {
    }

    System(const System& other) = delete;
    System& operator=(const System& other) = delete;
    System(System&& other) noexcept = delete;
    System& operator=(System&& other) noexcept = delete;
    virtual ~System() override = default;

    virtual ANSIStringView GetName() const override final
    {
        return ANSIStringView(TypeNameHelper<Derived, true>::value);
    }

    virtual TypeID GetTypeID() const override final
    {
        return TypeID::ForType<Derived>();
    }

    virtual void Process(GameCounter::TickUnit delta) override = 0;
};

} // namespace hyperion

#endif
