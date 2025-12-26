/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/functional/Delegate.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <scene/ComponentContainer.hpp>
#include <scene/EntitySetHelpers.hpp>

namespace Hyperion {

class EntityManager;
class Scene;
class World;

class SystemComponentDescriptors : HashSet<ComponentInfo, &ComponentInfo::typeId>
{
public:
    EntitySetId entitySetId; // can be 0 if inited dynamically (from Span<ComponentInfo>)

    template <class... ComponentDescriptors>
    SystemComponentDescriptors(ComponentDescriptors&&... componentDescriptors)
        : HashSet({ std::forward<ComponentDescriptors>(componentDescriptors)... }),
          entitySetId(GetEntitySetId<std::conditional_t<bool(ComponentDescriptors::Access& ComponentAccess::READ_WRITE), typename ComponentDescriptors::Type, VoidComponentType>...>())
    {
        Assert(Size() == sizeof...(ComponentDescriptors), "Duplicate component descriptors found");
    }

    explicit SystemComponentDescriptors(Span<ComponentInfo> componentInfos)
    {
        for (ComponentInfo& componentInfo : componentInfos)
        {
            Insert(componentInfo);
        }
    }

    using HashSet::ToArray;

    HYP_DEF_STL_BEGIN_END(HashSet::Begin(), HashSet::End())
};

/*! \brief A system is attached to a World and batch processes entities with specific components each tick.
 *  Systems are grouped into SystemExecutionGroups which define how and when they are executed (e.g., on the sim thread, in parallel, etc.). */
HYP_CLASS(Abstract)
class HYP_API SystemBase : public ObjectBase
{
    HYP_OBJECT_BODY(SystemBase);

public:
    friend class EntityManager;
    friend class SystemExecutionGroup;
    friend class World;

    virtual ~SystemBase() override = default;

    Name GetName() const;

    virtual bool ShouldProcessScene(Scene* scene) const
    {
        return true;
    }

    /*! \brief Returns true if this System can be executed in parallel, false otherwise.
     *  If false, the System will be executed on the sim thread.
     *
     *  \return True if this System can be executed in parallel, false otherwise.
     */
    virtual bool AllowParallelExecution() const
    {
        return true;
    }

    /*! \brief Returns true if this System requires the sim thread to execute, false otherwise.
     *  \details Use this to ensure that the System can access sim thread-only resources or perform operations
     *  that must be done on the sim thread, such as modifying the Scene or World state.
     *  If true, the System will be executed on the sim thread.
     *
     *  \return True if this System requires the sim thread to execute, false otherwise.
     */
    virtual bool RequiresSimThread() const
    {
        return false;
    }

    virtual bool AllowUpdate() const
    {
        return true;
    }

    /*! \brief Returns the TypeIds of the components this System operates on.
     *  To be used by the EntityManager in order to properly order the Systems based on their dependencies.
     *
     *  \return The TypeIds of the components this System operates on.
     */
    HYP_FORCE_INLINE const Array<TypeId>& GetComponentTypeIds() const
    {
        return m_componentTypeIds;
    }

    /*! \brief Returns the ComponentInfo objects of the components this System operates on.
     *
     *  \return The ComponentInfos of the components this System operates on.
     */
    HYP_FORCE_INLINE const Array<ComponentInfo>& GetComponentInfos() const
    {
        return m_componentInfos;
    }

    /*! \brief Returns true if all given TypeIds are operated on by this System, false otherwise.
     *
     *  \param componentTypeIds The TypeIds of the components to check.
     *  \param receiveEventsContext If true, this function will skip components that do not receive events for this System.
     *
     *  \return True if all given TypeIds are operated on by this System, false otherwise.
     */
    bool ActsOnComponents(const Array<TypeId>& componentTypeIds, bool receiveEventsContext) const
    {
        for (const TypeId componentTypeId : m_componentTypeIds)
        {
            const ComponentInfo& componentInfo = GetComponentInfo(componentTypeId);

            // skip observe-only components
            if (!(componentInfo.access & ComponentAccess::READ_WRITE))
            {
                continue;
            }

            if (receiveEventsContext && !componentInfo.receivesEvents)
            {
                continue;
            }

            if (!componentTypeIds.Contains(componentTypeId))
            {
                return false;
            }
        }

        return true;
    }

    /*! \brief Returns true if this System operates on the component with the given TypeId, false otherwise.
     *
     *  \param componentTypeId The TypeId of the component to check.
     *  \param includeReadOnly If true, this function will return true even if the component is read-only.
     *  Otherwise, read-only components will be ignored.
     *
     *  \return True if this System operates on the component with the given TypeId, false otherwise.
     */
    bool HasComponentTypeId(TypeId componentTypeId, bool includeReadOnly = true) const
    {
        const bool hasComponentTypeId = m_componentTypeIds.Contains(componentTypeId);

        if (!hasComponentTypeId)
        {
            return false;
        }

        if (includeReadOnly)
        {
            return true;
        }

        const ComponentInfo& componentInfo = GetComponentInfo(componentTypeId);

        return !!(componentInfo.access & ComponentAccess::WRITE);
    }

    /*! \brief Returns the ComponentInfo of the component with the given TypeId.
     *
     *  \param componentTypeId The TypeId of the component to check.
     *
     *  \return The ComponentInfo of the component with the given TypeId.
     */
    const ComponentInfo& GetComponentInfo(TypeId componentTypeId) const
    {
        const auto it = m_componentTypeIds.Find(componentTypeId);
        Assert(it != m_componentTypeIds.End(), "Component type Id not found");

        const SizeType index = m_componentTypeIds.IndexOf(it);
        Assert(index != SizeType(-1), "Component type Id not found");

        return m_componentInfos[index];
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

    virtual void Process(float delta, Span<Handle<Scene>> scenes) = 0;

    HYP_FORCE_INLINE World* GetWorld() const
    {
        return m_world;
    }

protected:
    SystemBase()
    {
    }

    /*! \brief Set a callback to be called once after the System has processed.
     *  The callback will be called on the sim thread after the System has finished processing, so it is safe to add/remove components within the callback.
     *
     *  \param fn The callback to call after the System has processed. */
    template <class Func>
    void AfterProcess(Func&& fn)
    {
        m_afterProcessProcs.EmplaceBack(std::forward<Func>(fn));
    }

    virtual void Init() override
    {
        SetReady(true);
    }

    virtual SystemComponentDescriptors GetComponentDescriptors() const = 0;

    World* m_world = nullptr;
    DelegateHandlerSet m_delegateHandlers;

private:
    void InitComponentInfos_Internal();

    HashSet<WeakHandle<Entity>> m_initializedEntities;

    Array<TypeId> m_componentTypeIds;
    Array<ComponentInfo> m_componentInfos;

    Array<Proc<void()>> m_afterProcessProcs;
};

} // namespace Hyperion
