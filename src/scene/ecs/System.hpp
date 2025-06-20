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

#include <core/object/HypObject.hpp>

#include <core/Defines.hpp>

#include <scene/ecs/ComponentContainer.hpp>

#include <GameCounter.hpp>

namespace hyperion {

class EntityManager;
class Scene;

enum class SceneFlags : uint32;

class SystemComponentDescriptors : HashSet<ComponentInfo, &ComponentInfo::type_id>
{
public:
    template <class... ComponentDescriptors>
    SystemComponentDescriptors(ComponentDescriptors&&... component_descriptors)
        : HashSet({ std::forward<ComponentDescriptors>(component_descriptors)... })
    {
        AssertThrowMsg(Size() == sizeof...(ComponentDescriptors), "Duplicate component descriptors found");
    }

    SystemComponentDescriptors(Span<ComponentInfo> component_infos)
    {
        for (ComponentInfo& component_info : component_infos)
        {
            Insert(component_info);
        }
    }

    using HashSet::ToArray;

    HYP_DEF_STL_BEGIN_END(HashSet::Begin(), HashSet::End())
};

HYP_CLASS(Abstract)
class HYP_API SystemBase : public HypObject<SystemBase>
{
    HYP_OBJECT_BODY(SystemBase);

public:
    friend class EntityManager;
    friend class SystemExecutionGroup;

    virtual ~SystemBase() override = default;

    Name GetName() const;
    TypeID GetTypeID() const;

    virtual bool ShouldCreateForScene(Scene* scene) const
    {
        return true;
    }

    /*! \brief Returns true if this System can be executed in parallel, false otherwise.
     *  If false, the System will be executed on the game thread.
     *
     *  \return True if this System can be executed in parallel, false otherwise.
     */
    virtual bool AllowParallelExecution() const
    {
        return true;
    }

    /*! \brief Returns true if this System requires the game thread to execute, false otherwise.
     *  \details Use this to ensure that the System can access game thread-only resources or perform operations
     *  that must be done on the game thread, such as modifying the Scene or World state.
     *  If true, the System will be executed on the game thread.
     *
     *  \return True if this System requires the game thread to execute, false otherwise.
     */
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

    virtual void OnEntityAdded(Entity* entity)
    {
    }

    virtual void OnEntityRemoved(Entity* entity)
    {
    }

    virtual void Shutdown()
    {
    }

    virtual void Process(float delta) = 0;

protected:
    SystemBase()
        : m_entity_manager(nullptr)
    {
    }

    SystemBase(EntityManager& entity_manager)
        : m_entity_manager(&entity_manager)
    {
    }

    HYP_FORCE_INLINE EntityManager& GetEntityManager() const
    {
        AssertDebug(m_entity_manager != nullptr);
        return *m_entity_manager;
    }

    /*! \brief Set a callback to be called once after the System has processed.
     *  The callback will be called on the EntityManager's owner thread and will not be parallelized, ensuring proper synchronization.
     *  After the callback has been called, it will be reset.
     *
     *  \param fn The callback to call after the System has processed. */
    template <class Func>
    void AfterProcess(Func&& fn)
    {
        m_after_process_procs.EmplaceBack(std::forward<Func>(fn));
    }

    virtual void Init() override
    {
        SetReady(true);
    }

    virtual SystemComponentDescriptors GetComponentDescriptors() const = 0;

    Scene* GetScene() const;
    World* GetWorld() const;

    EntityManager* m_entity_manager;
    DelegateHandlerSet m_delegate_handlers;

private:
    void InitComponentInfos_Internal()
    {
        m_component_type_ids.Clear();
        m_component_infos.Clear();

        Array<ComponentInfo> component_descriptors_array = GetComponentDescriptors().ToArray();
        m_component_type_ids.Reserve(component_descriptors_array.Size());
        m_component_infos.Reserve(component_descriptors_array.Size());

        for (const ComponentInfo& component_info : component_descriptors_array)
        {
            m_component_type_ids.PushBack(component_info.type_id);
            m_component_infos.PushBack(component_info);
        }
    }

    HashSet<WeakHandle<Entity>> m_initialized_entities;

    Array<TypeID> m_component_type_ids;
    Array<ComponentInfo> m_component_infos;

    Array<Proc<void()>> m_after_process_procs;
};

} // namespace hyperion

#endif
