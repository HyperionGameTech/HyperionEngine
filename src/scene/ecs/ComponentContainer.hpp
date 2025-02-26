/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_COMPONENT_CONTAINER_HPP
#define HYPERION_ECS_COMPONENT_CONTAINER_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FlatMap.hpp>

#include <core/utilities/Optional.hpp>
#include <core/utilities/TypeID.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <core/object/HypData.hpp>

#include <core/ID.hpp>
#include <core/IDGenerator.hpp>
#include <core/Util.hpp>

namespace hyperion {

class Entity;

using ComponentID = uint32;
using ComponentTypeID = uint32;
using ComponentRWFlags = uint32;

enum ComponentRWFlagBits : ComponentRWFlags
{
    COMPONENT_RW_FLAGS_NONE         = 0,
    COMPONENT_RW_FLAGS_READ         = 0x1,
    COMPONENT_RW_FLAGS_WRITE        = 0x2,
    COMPONENT_RW_FLAGS_READ_WRITE   = COMPONENT_RW_FLAGS_READ | COMPONENT_RW_FLAGS_WRITE
};

template <class T, ComponentRWFlags RWFlags = COMPONENT_RW_FLAGS_READ_WRITE, bool ReceivesEvents = true>
struct ComponentDescriptor
{
    using Type = T;

    constexpr static ComponentRWFlags rw_flags = RWFlags;
    constexpr static bool receives_events = ReceivesEvents;
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

struct ComponentTypeIDGeneratorBase
{
    static ComponentTypeID NextID()
    {
        static IDGenerator generator;

        return generator.NextID();
    }
};

template <class Component>
struct ComponentTypeIDGenerator final : ComponentTypeIDGeneratorBase
{
    static ComponentTypeID Next()
    {
        static ComponentTypeID id = ComponentTypeIDGeneratorBase::NextID();

        return id;
    }
};

template <class Component>
static ComponentTypeID GetComponentTypeID()
    { return ComponentTypeIDGenerator<Component>::Next(); }

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

    HYP_FORCE_INLINE ComponentContainerFactoryBase *GetFactory() const
        { return m_factory; }

#ifdef HYP_ENABLE_MT_CHECK
    HYP_FORCE_INLINE DataRaceDetector &GetDataRaceDetector()
        { return m_data_race_detector; }

    HYP_FORCE_INLINE const DataRaceDetector &GetDataRaceDetector() const
        { return m_data_race_detector; }
#endif

    /*! \brief Gets the type ID of the component type that this component container holds.
     *
     *  \return The type ID of the component type.
     */
    virtual TypeID GetComponentTypeID() const = 0;

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

    /*! \brief Tries to get the component with the given ID from the component container.
     *
     *  \param id The ID of the component to get.
     *  \param out_hyp_data The value to store a reference to the component in
     *
     *  \return True if the component was found, false otherwise
    */
    virtual bool TryGetComponent(ComponentID id, HypData &out_hyp_data) = 0;

    /*! \brief Checks if the component container has a component with the given ID.
     *
     *  \param id The ID of the component to check.
     *
     *  \return True if the component container has a component with the given ID, false otherwise.
     */
    virtual bool HasComponent(ComponentID id) const = 0;

    /*! \brief Adds a component to the component container, using type erasure.
     *
     *  \param ref A type-erased reference to an object of type Component.
     *
     *  \return The ID of the added component.
     */
    virtual ComponentID AddComponent(AnyRef ref) = 0;

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

protected:
    HYP_DECLARE_MT_CHECK(m_data_race_detector);

private:
    ComponentContainerFactoryBase   *m_factory;
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

    virtual TypeID GetComponentTypeID() const override
    {
        static const TypeID type_id = TypeID::ForType<Component>();

        return type_id;
    }

    virtual bool HasComponent(ComponentID id) const override
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_components.Contains(id);
    }

    virtual AnyRef TryGetComponent(ComponentID id) override
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_components.Find(id);

        if (it == m_components.End()) {
            return AnyRef::Empty();
        }

        return AnyRef(&it->second);
    }

    virtual ConstAnyRef TryGetComponent(ComponentID id) const override
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_components.Find(id);

        if (it == m_components.End()) {
            return ConstAnyRef::Empty();
        }

        return ConstAnyRef(&it->second);
    }

    virtual bool TryGetComponent(ComponentID id, HypData &out_hyp_data) override
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_components.Find(id);

        if (it == m_components.End()) {
            return false;
        }

        out_hyp_data = HypData(&it->second);

        return true;
    }

    HYP_FORCE_INLINE Component &GetComponent(ComponentID id)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        AssertThrowMsg(HasComponent(id), "Component of type `%s` with ID %u does not exist", TypeNameWithoutNamespace<Component>().Data(), id);
        
        return m_components.At(id);
    }

    HYP_FORCE_INLINE const Component &GetComponent(ComponentID id) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        AssertThrowMsg(HasComponent(id), "Component of type `%s` with ID %u does not exist", TypeNameWithoutNamespace<Component>().Data(), id);
        
        return m_components.At(id);
    }

    HYP_FORCE_INLINE Pair<ComponentID, Component &> AddComponent(Component &&component)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        ComponentID id = ++m_component_id_counter;

        auto insert_result = m_components.Set(id, std::move(component));

        return Pair<ComponentID, Component &> { id, insert_result.first->second };
    }

    virtual ComponentID AddComponent(AnyRef ref) override
    {
        AssertThrowMsg(ref.Is<Component>(), "Component is not of the correct type");

        return AddComponent(std::move(ref.Get<Component>())).first;
    }

    virtual bool RemoveComponent(ComponentID id) override
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        auto it = m_components.Find(id);

        if (it != m_components.End()) {
            m_components.Erase(it);

            return true;
        }

        return false;
    }

    virtual Optional<ComponentID> MoveComponent(ComponentID id, ComponentContainerBase &other) override
    {
        AssertThrowMsg(other.GetComponentTypeID() == GetComponentTypeID(), "Component container is not of the same type");

        HYP_MT_CHECK_RW(m_data_race_detector);

        auto it = m_components.Find(id);

        if (it != m_components.End()) {
            const ComponentID new_component_id = static_cast<ComponentContainer<Component> &>(other).AddComponent(std::move(it->second)).first;

            m_components.Erase(it);

            return new_component_id;
        }

        return { };
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_components.Size();
    }

private:
    ComponentID                     m_component_id_counter = 0;
    HashMap<ComponentID, Component> m_components;
};

template <class Component>
typename ComponentContainer<Component>::Factory ComponentContainer<Component>::factory {
    []() -> UniquePtr<ComponentContainerBase>
    {
        return MakeUnique<ComponentContainer<Component>>();
    }
};

} // namespace hyperion

#endif