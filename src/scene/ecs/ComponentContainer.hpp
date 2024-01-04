#ifndef HYPERION_V2_ECS_COMPONENT_CONTAINER_HPP
#define HYPERION_V2_ECS_COMPONENT_CONTAINER_HPP

#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Optional.hpp>
#include <core/ID.hpp>

namespace hyperion::v2 {

class Entity;

class ComponentContainerBase
{
public:
    virtual ~ComponentContainerBase() = default;

    /*! \brief Remove a component from an entity.
     *
     *  \param entity The entity to remove the component from.
     *
     *  \return True if the component was removed, false otherwise.
     */
    virtual Bool RemoveComponent(ID<Entity> entity) = 0;
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

    HYP_FORCE_INLINE
    Bool HasComponent(ID<Entity> entity) const
        { return m_components.Contains(entity); }

    HYP_FORCE_INLINE
    Component &GetComponent(ID<Entity> entity)
    {
        AssertThrowMsg(HasComponent(entity), "Entity does not have component");
        
        return m_components.At(entity);
    }

    HYP_FORCE_INLINE
    const Component &GetComponent(ID<Entity> entity) const
    {
        AssertThrowMsg(HasComponent(entity), "Entity does not have component");
        
        return m_components.At(entity);
    }

    HYP_FORCE_INLINE
    void AddComponent(ID<Entity> entity, Component &&component)
    {
        AssertThrowMsg(!HasComponent(entity), "Entity already has component");

        m_components.Set(entity, std::move(component));
    }

    virtual Bool RemoveComponent(ID<Entity> entity) override
    {
        auto it = m_components.Find(entity);

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
    FlatMap<ID<Entity>, Component> m_components;
};

} // namespace hyperion::v2

#endif