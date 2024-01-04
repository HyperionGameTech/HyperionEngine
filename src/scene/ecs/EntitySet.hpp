#ifndef HYPERION_V2_ECS_ENTITY_SET_HPP
#define HYPERION_V2_ECS_ENTITY_SET_HPP

#include <core/lib/FlatMap.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/IDCreator.hpp>
#include <core/Handle.hpp>
#include <scene/ecs/EntityContainer.hpp>
#include <scene/ecs/ComponentContainer.hpp>
#include <scene/ecs/EntitySetBase.hpp>
#include <Types.hpp>

#include <tuple>

namespace hyperion::v2 {

class Entity;

template <class ... Components>
class EntitySet;

template <class ... Components>
struct EntitySetIterator
{
    EntitySet<Components...>    &set;
    UInt                        index;

    EntitySetIterator(EntitySet<Components...> &set, UInt index)
        : set(set),
          index(index)
    {
    }

    EntitySetIterator(const EntitySetIterator &other)                   = default;
    EntitySetIterator &operator=(const EntitySetIterator &other)        = default;
    EntitySetIterator(EntitySetIterator &&other) noexcept               = default;
    EntitySetIterator &operator=(EntitySetIterator &&other) noexcept    = default;
    ~EntitySetIterator()                                                = default;

    EntitySetIterator &operator++()
        { ++index; return *this; }

    EntitySetIterator operator++(Int)
        { EntitySetIterator it = *this; ++index; return it; }

    Bool operator==(const EntitySetIterator &other) const
        { return std::addressof(set) == std::addressof(other.set) && index == other.index; }

    Bool operator!=(const EntitySetIterator &other) const
        { return !(*this == other); }

    std::tuple<ID<Entity>, Components &...> operator*()
    {
        return std::tuple<ID<Entity>, Components &...> {
            set.m_entities.AtIndex(index).first,
            std::get<ComponentContainer<Components> &>(set.m_components).GetComponent(set.m_entities.AtIndex(index).first)...
        };
    }

    std::tuple<ID<Entity>, const Components &...> operator*() const
    {
        return std::tuple<ID<Entity>, const Components &...> {
            set.m_entities.AtIndex(index).first,
            std::get<ComponentContainer<Components> &>(set.m_components).GetComponent(set.m_entities.AtIndex(index).first)...
        };
    }

    std::tuple<ID<Entity>, Components &...> operator->()
        { return **this; }

    std::tuple<ID<Entity>, const Components &...> operator->() const
        { return **this; }
};

/*! \brief A set of entities with a specific set of components.
 *
 *  \tparam Components The components that the entities in this set have.
 */
template <class ... Components>
class EntitySet : public EntitySetBase
{
public:
    friend struct EntitySetIterator<Components...>;

    using Iterator          = EntitySetIterator<Components...>;
    using ConstIterator     = EntitySetIterator<const Components...>;

    static const EntitySetTypeID type_id;

    EntitySet(EntityContainer &entities, ComponentContainer<Components> &... components)
        : m_entities(entities),
          m_components(components...)
    {
    }

    EntitySet(const EntitySet &other)                   = delete;
    EntitySet &operator=(const EntitySet &other)        = delete;
    EntitySet(EntitySet &&other) noexcept               = delete;
    EntitySet &operator=(EntitySet &&other) noexcept    = delete;
    ~EntitySet()                                        = default;

    /*! \brief Gets the entities in this set.
     *
     *  \return Reference to the entities in this set.
     */
    const EntityContainer &GetEntities() const
        { return m_entities; }

    /*! \brief Gets a component. */
    template <class Component>
    Component &GetComponent(ID<Entity> entity)
    {
        static_assert((std::is_same_v<Components, Component> || ...), "Component not in EntitySet");

        return std::get<ComponentContainer<Component> &>(m_components).GetComponent(m_entities);
    }

    /*! \brief Gets a component. */
    template <class Component>
    const Component &GetComponent(ID<Entity> entity) const
    {
        static_assert((std::is_same_v<Components, Component> || ...), "Component not in EntitySet");

        return std::get<ComponentContainer<Component> &>(m_components).GetComponent(m_entities);
    }

    /*! \brief Adds a component to an entity. */
    template <class Component>
    void AddComponent(ID<Entity> entity, Component &&component)
    {
        static_assert((std::is_same_v<Components, Component> || ...), "Component not in EntitySet");

        std::get<ComponentContainer<Component> &>(m_components).AddComponent(entity, std::move(component));
    }

    /*! \brief Removes a component from an entity. */
    template <class Component>
    void RemoveComponent(ID<Entity> entity)
    {
        static_assert((std::is_same_v<Components, Component> || ...), "Component not in EntitySet");

        std::get<ComponentContainer<Component> &>(m_components).RemoveComponent(entity);
    }

    // All component containers should have the same size, so we can just use the first one.

    HYP_DEF_STL_BEGIN_END(
        Iterator(*this, 0),
        Iterator(*this, std::get<0>(m_components).Size())
    )

private:
    EntityContainer                                 &m_entities;
    std::tuple<ComponentContainer<Components> &...> m_components;
};

template <class ... Components>
const EntitySetTypeID EntitySet<Components...>::type_id = EntitySetBase::GenerateEntitySetTypeID<Components...>();

} // namespace hyperion::v2

#endif