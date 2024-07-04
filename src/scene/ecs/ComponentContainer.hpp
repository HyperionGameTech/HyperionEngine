/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_COMPONENT_CONTAINER_HPP
#define HYPERION_ECS_COMPONENT_CONTAINER_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/utilities/Optional.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/memory/AnyRef.hpp>
#include <core/utilities/TypeID.hpp>
#include <core/ID.hpp>
#include <core/Util.hpp>

namespace hyperion {

class Entity;

using ComponentID = uint32;
using ComponentRWFlags = uint32;

enum ComponentRWFlagBits : ComponentRWFlags
{
    COMPONENT_RW_FLAGS_NONE         = 0,
    COMPONENT_RW_FLAGS_READ         = 0x1,
    COMPONENT_RW_FLAGS_WRITE        = 0x2,
    COMPONENT_RW_FLAGS_READ_WRITE   = COMPONENT_RW_FLAGS_READ | COMPONENT_RW_FLAGS_WRITE
};

template <class T, ComponentRWFlags _rw_flags = COMPONENT_RW_FLAGS_READ_WRITE, bool _receives_events = true>
struct ComponentDescriptor
{
    using Type = T;

    constexpr static ComponentRWFlags rw_flags = _rw_flags;
    constexpr static bool receives_events = _receives_events;
};

struct ComponentInfo
{
    TypeID              type_id;
    ComponentRWFlags    rw_flags;
    bool                receives_events;

    template <class ComponentDescriptorType>
    ComponentInfo(ComponentDescriptorType &&)
        : type_id(TypeID::ForType<typename ComponentDescriptorType::Type>()),
          rw_flags(ComponentDescriptorType::rw_flags),
          receives_events(ComponentDescriptorType::receives_events)
    {
    }
};

class ComponentContainerFactoryBase;

class HYP_API ComponentContainerBase
{
public:
    ComponentContainerBase(ComponentContainerFactoryBase *factory)
        : m_factory(factory)
    {
    }

    ComponentContainerBase(const ComponentContainerBase &)                  = delete;
    ComponentContainerBase &operator=(const ComponentContainerBase &)       = delete;
    ComponentContainerBase(ComponentContainerBase &&) noexcept              = delete;
    ComponentContainerBase &operator=(ComponentContainerBase &&) noexcept   = delete;

    virtual ~ComponentContainerBase()                                       = default;

    ComponentContainerFactoryBase *GetFactory() const
        { return m_factory; }

    /*! \brief Tries to get the component with the given ID from the component container.
     *
     *  \param id The ID of the component to get.
     *
     *  \return A pointer to the component if the component container has a component with the given ID, nullptr otherwise.
    */
    virtual AnyRef TryGetComponent(ComponentID id) = 0;

    /*! \brief Tries to get the component with the given ID from the component container.
     *
     *  \param id The ID of the component to get.
     *
     *  \return A pointer to the component if the component container has a component with the given ID, nullptr otherwise.
    */
    virtual ConstAnyRef TryGetComponent(ComponentID id) const = 0;

    /*! \brief Checks if the component container has a component with the given ID.
     *
     *  \param id The ID of the component to check.
     *
     *  \return True if the component container has a component with the given ID, false otherwise.
     */
    virtual bool HasComponent(ComponentID id) const = 0;

    /*! \brief Adds a component to the component container, using type erasure.
     *
     *  \param component A UniquePtr<void>, pointing to an object of type Component.
     *
     *  \return The ID of the added component.
     */
    virtual ComponentID AddComponent(UniquePtr<void> &&component) = 0;

    /*! \brief Removes the component with the given ID from the component container.
     *
     *  \param id The ID of the component to remove.
     *
     *  \return True if the component was removed, false otherwise.
     */
    virtual bool RemoveComponent(ComponentID id) = 0;

    /*! \brief Moves the component with the given ID from this component container to the given component container.
     *       The component container must be of the same type as this component container, otherwise an assertion will be thrown.
     *
     *  \param id The ID of the component to move.
     *  \param other The component container to move the component to.
     *
     *  \return An optional containing the ID of the component in the given component container if the component was moved, an empty optional otherwise.
     */
    virtual Optional<ComponentID> MoveComponent(ComponentID id, ComponentContainerBase &other) = 0;

private:
    ComponentContainerFactoryBase *m_factory;
};

class ComponentContainerFactoryBase
{
public:
    virtual ~ComponentContainerFactoryBase() = default;

    virtual UniquePtr<ComponentContainerBase> Create() const = 0;
};

template <class Component>
class ComponentContainer : public ComponentContainerBase
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
            { return m_create(); }

    private:
        UniquePtr<ComponentContainerBase> (*m_create)();
    } factory;

public:
    static Factory *GetFactory()
        { return &factory; }

    ComponentContainer()
        : ComponentContainerBase(&factory)
    {
    }

    ComponentContainer(const ComponentContainer &)                  = delete;
    ComponentContainer &operator=(const ComponentContainer &)       = delete;
    ComponentContainer(ComponentContainer &&) noexcept              = delete;
    ComponentContainer &operator=(ComponentContainer &&) noexcept   = delete;
    virtual ~ComponentContainer() override                          = default;

    virtual bool HasComponent(ComponentID id) const override
        { return m_components.Contains(id); }

    virtual AnyRef TryGetComponent(ComponentID id) override
    {
        auto it = m_components.Find(id);

        if (it == m_components.End()) {
            return AnyRef::Empty();
        }

        return AnyRef(&it->second);
    }

    virtual ConstAnyRef TryGetComponent(ComponentID id) const override
    {
        auto it = m_components.Find(id);

        if (it == m_components.End()) {
            return ConstAnyRef::Empty();
        }

        return ConstAnyRef(&it->second);
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    Component &GetComponent(ComponentID id)
    {
        AssertThrowMsg(HasComponent(id), "Component of type `%s` with ID %u does not exist", TypeNameWithoutNamespace<Component>().Data(), id);
        
        return m_components.At(id);
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    const Component &GetComponent(ComponentID id) const
    {
        AssertThrowMsg(HasComponent(id), "Component of type `%s` with ID %u does not exist", TypeNameWithoutNamespace<Component>().Data(), id);
        
        return m_components.At(id);
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    ComponentID AddComponent(Component &&component)
    {
        ComponentID id = ++m_component_id_counter;

        m_components.Set(id, std::move(component));

        return id;
    }

    virtual ComponentID AddComponent(UniquePtr<void> &&component) override
    {
        AssertThrowMsg(component, "Component is null");
        AssertThrowMsg(component.Is<Component>(), "Component is not of the correct type");

        return AddComponent(std::move(*static_cast<Component *>(component.Get())));
    }

    virtual bool RemoveComponent(ComponentID id) override
    {
        auto it = m_components.Find(id);

        if (it != m_components.End()) {
            m_components.Erase(it);

            return true;
        }

        return false;
    }

    virtual Optional<ComponentID> MoveComponent(ComponentID id, ComponentContainerBase &other) override
    {
        AssertThrowMsg(dynamic_cast<ComponentContainer<Component> *>(&other), "Component container is not of the same type");

        auto it = m_components.Find(id);

        if (it != m_components.End()) {
            const ComponentID new_component_id = static_cast<ComponentContainer<Component> &>(other).AddComponent(std::move(it->second));

            m_components.Erase(it);

            return new_component_id;
        }

        return { };
    }

    HYP_NODISCARD HYP_FORCE_INLINE
    SizeType Size() const
        { return m_components.Size(); }

private:
    ComponentID                     m_component_id_counter = 0;
    FlatMap<ComponentID, Component> m_components;
};

template <class Component>
typename ComponentContainer<Component>::Factory ComponentContainer<Component>::factory {
    []() -> UniquePtr<ComponentContainerBase>
    {
        return UniquePtr<ComponentContainerBase>(new ComponentContainer<Component>());
    }
};

} // namespace hyperion

#endif