#ifndef HYPERION_V2_ECS_COMPONENT_CONTAINER_HPP
#define HYPERION_V2_ECS_COMPONENT_CONTAINER_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Optional.hpp>
#include <core/ID.hpp>

namespace hyperion::v2 {

class Entity;

using ComponentID = UInt;
using ComponentRWFlags = UInt;

enum ComponentRWFlagBits : ComponentRWFlags
{
    COMPONENT_RW_FLAGS_NONE         = 0,
    COMPONENT_RW_FLAGS_READ         = 0x1,
    COMPONENT_RW_FLAGS_WRITE        = 0x2,
    COMPONENT_RW_FLAGS_READ_WRITE   = COMPONENT_RW_FLAGS_READ | COMPONENT_RW_FLAGS_WRITE
};

template <class T, ComponentRWFlags RWFlags = COMPONENT_RW_FLAGS_READ_WRITE>
struct ComponentDescriptor
{
    using Type = T;

    constexpr static ComponentRWFlags rw_flags = RWFlags;
};

class ComponentContainerBase
{
public:
    virtual ~ComponentContainerBase() = default;

    /*! \brief Checks if the component container has a component with the given ID.
     *
     *  \param id The ID of the component to check.
     *
     *  \return True if the component container has a component with the given ID, false otherwise.
     */
    virtual Bool HasComponent(ComponentID id) const = 0;

    /*! \brief Removes the component with the given ID from the component container.
     *
     *  \param id The ID of the component to remove.
     *
     *  \return True if the component was removed, false otherwise.
     */
    virtual Bool RemoveComponent(ComponentID id) = 0;
};

template <class Component>
class ComponentContainer : public ComponentContainerBase
{
public:
    ComponentContainer()                                            = default;
    ComponentContainer(const ComponentContainer &)                  = delete;
    ComponentContainer &operator=(const ComponentContainer &)       = delete;
    ComponentContainer(ComponentContainer &&) noexcept              = delete;
    ComponentContainer &operator=(ComponentContainer &&) noexcept   = delete;
    virtual ~ComponentContainer() override                          = default;

    virtual Bool HasComponent(ComponentID id) const override
        { return m_components.Contains(id); }

    HYP_FORCE_INLINE
    Component &GetComponent(ComponentID id)
    {
        AssertThrowMsg(HasComponent(id), "Component does not exist");
        
        return m_components.At(id);
    }

    HYP_FORCE_INLINE
    const Component &GetComponent(ComponentID id) const
    {
        AssertThrowMsg(HasComponent(id), "Component does not exist");
        
        return m_components.At(id);
    }

    HYP_FORCE_INLINE
    ComponentID AddComponent(Component &&component)
    {
        ComponentID id = m_component_id_counter++;

        m_components.Set(id, std::move(component));

        return id;
    }

    virtual Bool RemoveComponent(ComponentID id) override
    {
        auto it = m_components.Find(id);

        if (it != m_components.End()) {
            m_components.Erase(it);

            return true;
        }

        return false;
    }

    HYP_FORCE_INLINE
    SizeType Size() const
        { return m_components.Size(); }

private:
    ComponentID                     m_component_id_counter = 0;
    FlatMap<ComponentID, Component> m_components;
};

} // namespace hyperion::v2

#endif