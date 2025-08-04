/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/TypeId.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/AnyRef.hpp>
#include <core/memory/MemoryPool.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/object/HypData.hpp>

#include <core/object/ObjId.hpp>
#include <core/Util.hpp>

namespace hyperion {

class Entity;

using ComponentId = uint32;
using ComponentTypeId = uint32;
using ComponentRWFlags = uint32;

enum ComponentRWFlagBits : ComponentRWFlags
{
    COMPONENT_RW_FLAGS_NONE = 0,
    COMPONENT_RW_FLAGS_READ = 0x1,
    COMPONENT_RW_FLAGS_WRITE = 0x2,
    COMPONENT_RW_FLAGS_READ_WRITE = COMPONENT_RW_FLAGS_READ | COMPONENT_RW_FLAGS_WRITE
};

template <class T, ComponentRWFlags RWFlags = COMPONENT_RW_FLAGS_READ_WRITE, bool ReceivesEvents = true>
struct ComponentDescriptor
{
    using Type = T;

    constexpr static ComponentRWFlags rwFlags = RWFlags;
    constexpr static bool receivesEvents = ReceivesEvents;
};

HYP_STRUCT(Size = 12)
struct ComponentInfo
{
    HYP_FIELD()
    TypeId typeId;

    HYP_FIELD()
    ComponentRWFlags rwFlags;

    HYP_FIELD()
    bool receivesEvents;

    ComponentInfo()
        : typeId(TypeId::Void()),
          rwFlags(COMPONENT_RW_FLAGS_NONE),
          receivesEvents(false)
    {
    }

    ComponentInfo(TypeId typeId, ComponentRWFlags rwFlags = COMPONENT_RW_FLAGS_NONE, bool receivesEvents = false)
        : typeId(typeId),
          rwFlags(rwFlags),
          receivesEvents(receivesEvents)
    {
    }

    template <class ComponentDescriptorType>
    ComponentInfo(ComponentDescriptorType)
        : typeId(TypeId::ForType<typename NormalizedType<ComponentDescriptorType>::Type>()),
          rwFlags(NormalizedType<ComponentDescriptorType>::rwFlags),
          receivesEvents(NormalizedType<ComponentDescriptorType>::receivesEvents)
    {
    }
};

class ComponentContainerFactoryBase;

class HYP_API ComponentContainerBase
{
public:
    ComponentContainerBase(ComponentContainerFactoryBase* factory)
        : m_factory(factory)
    {
    }

    ComponentContainerBase(const ComponentContainerBase&) = delete;
    ComponentContainerBase& operator=(const ComponentContainerBase&) = delete;
    ComponentContainerBase(ComponentContainerBase&&) noexcept = delete;
    ComponentContainerBase& operator=(ComponentContainerBase&&) noexcept = delete;

    virtual ~ComponentContainerBase() = default;

    HYP_FORCE_INLINE ComponentContainerFactoryBase* GetFactory() const
    {
        return m_factory;
    }

#ifdef HYP_ENABLE_MT_CHECK
    HYP_FORCE_INLINE DataRaceDetector& GetDataRaceDetector()
    {
        return m_dataRaceDetector;
    }

    HYP_FORCE_INLINE const DataRaceDetector& GetDataRaceDetector() const
    {
        return m_dataRaceDetector;
    }
#endif

    /*! \brief Gets the type Id of the component type that this component container holds.
     *
     *  \return The type Id of the component type.
     */
    virtual TypeId GetComponentTypeId() const = 0;

    /*! \brief Tries to get the component with the given Id from the component container.
     *
     *  \param id The Id of the component to get.
     *
     *  \return A pointer to the component if the component container has a component with the given Id, nullptr otherwise.
     */
    virtual AnyRef TryGetComponent(ComponentId id) = 0;

    /*! \brief Tries to get the component with the given Id from the component container.
     *
     *  \param id The Id of the component to get.
     *
     *  \return A pointer to the component if the component container has a component with the given Id, nullptr otherwise.
     */
    virtual ConstAnyRef TryGetComponent(ComponentId id) const = 0;

    /*! \brief Tries to get the component with the given Id from the component container.
     *
     *  \param id The Id of the component to get.
     *  \param outHypData The value to store a reference to the component in
     *
     *  \return True if the component was found, false otherwise
     */
    virtual bool TryGetComponent(ComponentId id, HypData& outHypData) = 0;

    /*! \brief Checks if the component container has a component with the given Id.
     *
     *  \param id The Id of the component to check.
     *
     *  \return True if the component container has a component with the given Id, false otherwise.
     */
    virtual bool HasComponent(ComponentId id) const = 0;

    /*! \brief Adds a component to the component container, using HypData to store the component data generically.
     *
     *  \param componentData The HypData containing the component data to add.
     *
     *  \return The Id of the added component.
     */
    virtual ComponentId AddComponent(HypData&& componentData) = 0;

    /*! \brief Removes the component with the given Id from the component container.
     *
     *  \param id The Id of the component to remove.
     *
     *  \return True if the component was removed, false otherwise.
     */
    virtual bool RemoveComponent(ComponentId id) = 0;

    /*! \brief Removes the component with the given Id from the component container and stores the component object in HypData
     *
     *  \param id The Id of the component to remove.
     *  \param outHypData Out reference to store the component data in
     *
     *  \return True if the component was removed, false otherwise.
     */
    virtual bool RemoveComponent(ComponentId id, HypData& outHypData) = 0;
    
    /*! \brief Sets an existing component to a new value. The component must already exist,
     *  and the component data must be the same type as this container's held object type
     *
     *  \param id The ID of the component to assign
     *  \param componentData The HypData containing the component data to set.
     *
     *  \returns True on success, false if component was not found
     */
    virtual bool SetComponent(ComponentId id, HypData&& componentData) = 0;

    /*! \brief Moves the component with the given Id from this component container to the given component container.
     *       The component container must be of the same type as this component container, otherwise an assertion will be thrown.
     *
     *  \param id The Id of the component to move.
     *  \param other The component container to move the component to.
     *
     *  \return An optional containing the Id of the component in the given component container if the component was moved, an empty optional otherwise.
     */
    virtual Optional<ComponentId> MoveComponent(ComponentId id, ComponentContainerBase& other) = 0;

protected:
    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);

private:
    ComponentContainerFactoryBase* m_factory;
};

class ComponentContainerFactoryBase
{
public:
    virtual ~ComponentContainerFactoryBase() = default;

    virtual UniquePtr<ComponentContainerBase> Create() const = 0;
};

template <class Component>
class ComponentContainer final : public ComponentContainerBase
{
    static class Factory : public ComponentContainerFactoryBase
    {
    public:
        Factory(UniquePtr<ComponentContainerBase> (*create)())
            : m_create(create)
        {
        }

        virtual ~Factory() override = default;

        virtual UniquePtr<ComponentContainerBase> Create() const override
        {
            return m_create();
        }

    private:
        UniquePtr<ComponentContainerBase> (*m_create)();
    } s_factory;

public:
    static Factory* GetFactory()
    {
        return &s_factory;
    }

    ComponentContainer()
        : ComponentContainerBase(&s_factory)
    {
    }

    ComponentContainer(const ComponentContainer&) = delete;
    ComponentContainer& operator=(const ComponentContainer&) = delete;
    ComponentContainer(ComponentContainer&&) noexcept = delete;
    ComponentContainer& operator=(ComponentContainer&&) noexcept = delete;
    virtual ~ComponentContainer() override = default;

    virtual TypeId GetComponentTypeId() const override
    {
        static const TypeId typeId = TypeId::ForType<Component>();

        return typeId;
    }

    virtual bool HasComponent(ComponentId id) const override
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_components.Contains(id);
    }

    virtual AnyRef TryGetComponent(ComponentId id) override
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it == m_components.End())
        {
            return AnyRef::Empty();
        }

        return AnyRef(&it->second);
    }

    virtual ConstAnyRef TryGetComponent(ComponentId id) const override
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it == m_components.End())
        {
            return ConstAnyRef::Empty();
        }

        return ConstAnyRef(&it->second);
    }

    virtual bool TryGetComponent(ComponentId id, HypData& outHypData) override
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it == m_components.End())
        {
            return false;
        }

        outHypData = HypData(&it->second);

        return true;
    }

    HYP_FORCE_INLINE Component& GetComponent(ComponentId id)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        Assert(HasComponent(id), "Component of type `{}` with ID {} does not exist", TypeNameWithoutNamespace<Component>().Data(), id);

        return m_components.At(id);
    }

    HYP_FORCE_INLINE const Component& GetComponent(ComponentId id) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        Assert(HasComponent(id), "Component of type `{}` with ID {} does not exist", TypeNameWithoutNamespace<Component>().Data(), id);

        return m_components.At(id);
    }

    HYP_FORCE_INLINE Pair<ComponentId, Component&> AddComponent(const Component& component)
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        ComponentId id = ++m_componentIdCounter;

        auto insertResult = m_components.Set(id, component);

        return Pair<ComponentId, Component&> { id, insertResult.first->second };
    }

    HYP_FORCE_INLINE Pair<ComponentId, Component&> AddComponent(Component&& component)
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        ComponentId id = ++m_componentIdCounter;

        auto insertResult = m_components.Set(id, std::move(component));

        return Pair<ComponentId, Component&> { id, insertResult.first->second };
    }

    virtual ComponentId AddComponent(HypData&& componentData) override
    {
        Assert(componentData.IsValid(), "Cannot add an invalid component");
        Assert(componentData.Is<Component>(), "Component is not of the correct type");

        return AddComponent(std::move(componentData.Get<Component>())).first;
    }

    virtual bool RemoveComponent(ComponentId id) override
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it != m_components.End())
        {
            m_components.Erase(it);

            return true;
        }

        return false;
    }

    virtual bool RemoveComponent(ComponentId id, HypData& outHypData) override
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it != m_components.End())
        {
            outHypData = HypData(std::move(it->second));

            m_components.Erase(it);

            return true;
        }

        return false;
    }
    
    virtual bool SetComponent(ComponentId id, HypData&& componentData) override
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);
        
        Assert(componentData.IsValid() && componentData.GetTypeId() == GetComponentTypeId());

        auto it = m_components.Find(id);

        if (it != m_components.End())
        {
            m_components.Set(id, std::move(componentData.Get<Component>()));

            return true;
        }

        return false;
    }

    virtual Optional<ComponentId> MoveComponent(ComponentId id, ComponentContainerBase& other) override
    {
        Assert(other.GetComponentTypeId() == GetComponentTypeId(), "Component container is not of the same type");

        HYP_MT_CHECK_RW(m_dataRaceDetector);

        auto it = m_components.Find(id);

        if (it != m_components.End())
        {
            Component& component = it->second;

            const ComponentId newComponentId = static_cast<ComponentContainer<Component>&>(other).AddComponent(std::move(component)).first;

            m_components.Erase(it);

            return newComponentId;
        }

        return {};
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_components.Size();
    }

private:
    ComponentId m_componentIdCounter = 0;

    /// TODO: Change to MemoryPool and use Component* rather than ComponentId
    HashMap<ComponentId, Component> m_components;
    // MemoryPool<Component> m_componentPool;
};

template <class Component>
typename ComponentContainer<Component>::Factory ComponentContainer<Component>::s_factory {
    []() -> UniquePtr<ComponentContainerBase>
    {
        return MakeUnique<ComponentContainer<Component>>();
    }
};

} // namespace hyperion
