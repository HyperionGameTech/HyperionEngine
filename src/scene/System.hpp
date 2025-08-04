/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/TypeId.hpp>
#include <core/utilities/Tuple.hpp>
#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/functional/Delegate.hpp>

#include <core/object/HypObject.hpp>

#include <core/Defines.hpp>

#include <scene/ComponentContainer.hpp>

#include <GameCounter.hpp>

namespace hyperion {

class EntityManager;
class Scene;
class World;

enum class SceneFlags : uint32;

class SystemComponentDescriptors : HashSet<ComponentInfo, &ComponentInfo::typeId>
{
public:
    template <class... ComponentDescriptors>
    SystemComponentDescriptors(ComponentDescriptors&&... componentDescriptors)
        : HashSet({ std::forward<ComponentDescriptors>(componentDescriptors)... })
    {
        Assert(Size() == sizeof...(ComponentDescriptors), "Duplicate component descriptors found");
    }

    SystemComponentDescriptors(Span<ComponentInfo> componentInfos)
    {
        for (ComponentInfo& componentInfo : componentInfos)
        {
            Insert(componentInfo);
        }
    }

    using HashSet::ToArray;

    HYP_DEF_STL_BEGIN_END(HashSet::Begin(), HashSet::End())
};

HYP_CLASS(Abstract)
class HYP_API SystemBase : public HypObjectBase
{
    HYP_OBJECT_BODY(SystemBase);

public:
    friend class EntityManager;
    friend class SystemExecutionGroup;

    virtual ~SystemBase() override = default;

    Name GetName() const;

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
            if (!(componentInfo.rwFlags & COMPONENT_RW_FLAGS_READ) && !(componentInfo.rwFlags & COMPONENT_RW_FLAGS_WRITE))
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

        return !!(componentInfo.rwFlags & COMPONENT_RW_FLAGS_WRITE);
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

    virtual void Process(float delta) = 0;

protected:
    SystemBase()
        : m_entityManager(nullptr)
    {
    }

    SystemBase(EntityManager& entityManager)
        : m_entityManager(&entityManager)
    {
    }

    HYP_FORCE_INLINE EntityManager& GetEntityManager() const
    {
        AssertDebug(m_entityManager != nullptr);
        return *m_entityManager;
    }

    /*! \brief Set a callback to be called once after the System has processed.
     *  The callback will be called on the EntityManager's owner thread and will not be parallelized, ensuring proper synchronization.
     *  After the callback has been called, it will be reset.
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

    Scene* GetScene() const;
    World* GetWorld() const;

    EntityManager* m_entityManager;
    DelegateHandlerSet m_delegateHandlers;

private:
    void InitComponentInfos_Internal()
    {
        m_componentTypeIds.Clear();
        m_componentInfos.Clear();

        Array<ComponentInfo> componentDescriptorsArray = GetComponentDescriptors().ToArray();
        m_componentTypeIds.Reserve(componentDescriptorsArray.Size());
        m_componentInfos.Reserve(componentDescriptorsArray.Size());

        for (const ComponentInfo& componentInfo : componentDescriptorsArray)
        {
            m_componentTypeIds.PushBack(componentInfo.typeId);
            m_componentInfos.PushBack(componentInfo);
        }
    }

    HashSet<WeakHandle<Entity>> m_initializedEntities;

    Array<TypeId> m_componentTypeIds;
    Array<ComponentInfo> m_componentInfos;

    Array<Proc<void()>> m_afterProcessProcs;
};

} // namespace hyperion

