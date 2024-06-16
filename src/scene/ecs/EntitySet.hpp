/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_SET_HPP
#define HYPERION_ECS_ENTITY_SET_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/utilities/Tuple.hpp>
#include <core/memory/UniquePtr.hpp>

#include <scene/Entity.hpp>
#include <scene/ecs/EntityContainer.hpp>
#include <scene/ecs/ComponentContainer.hpp>
#include <scene/ecs/EntitySetBase.hpp>

#include <Types.hpp>

namespace hyperion {

template <class ... Components>
class EntitySet;

template <class ... Components>
struct EntitySetIterator
{
    EntitySet<Components...>    &set;
    uint                        index;

    EntitySetIterator(EntitySet<Components...> &set, uint index)
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

    EntitySetIterator operator++(int)
        { EntitySetIterator it = *this; ++index; return it; }

    bool operator==(const EntitySetIterator &other) const
        { return std::addressof(set) == std::addressof(other.set) && index == other.index; }

    bool operator!=(const EntitySetIterator &other) const
        { return !(*this == other); }

    Tuple< ID<Entity>, Components &... > operator*()
    {
        const typename EntitySet<Components...>::Element &element = set.m_elements[index];

        return ConcatTuples(
            MakeTuple(element.first),
            GetComponents(element.second, std::make_index_sequence<sizeof...(Components)>())
        );
    }

    Tuple< ID<Entity>, const Components &... > operator*() const
        { return const_cast<EntitySetIterator *>(this)->operator*(); }

    Tuple< ID<Entity>, Components &... > operator->()
        { return **this; }

    Tuple< ID<Entity>, const Components &... > operator->() const
        { return **this; }

private:
    template <SizeType ... Indices>
    Tuple< Components &... > GetComponents(const FixedArray<ComponentID, sizeof...(Components)> &component_ids, std::index_sequence<Indices...>)
    {
        return Tuple< Components &... >(
            set.m_component_containers.template GetElement< ComponentContainer<Components> & >().GetComponent(component_ids[Indices])...
        );
    }
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

    using Element           = Pair<ID<Entity>, FixedArray<ComponentID, sizeof...(Components)>>;

    using Iterator          = EntitySetIterator<Components...>;
    using ConstIterator     = EntitySetIterator<const Components...>;

    static const EntitySetTypeID type_id;

    EntitySet(EntityContainer &entities, ComponentContainer<Components> &... component_containers)
        : m_entities(entities),
          m_component_containers(component_containers...)
    {
        for (auto it = m_entities.Begin(); it != m_entities.End(); ++it) {
            OnEntityUpdated(it->first);
        }
    }

    EntitySet(const EntitySet &other)                   = delete;
    EntitySet &operator=(const EntitySet &other)        = delete;
    EntitySet(EntitySet &&other) noexcept               = delete;
    EntitySet &operator=(EntitySet &&other) noexcept    = delete;
    virtual ~EntitySet() override                       = default;

    /*! \brief Gets the elements array of this set.
     *  The elements array contains the entities in this set and the corresponding component IDs.
     *
     *  \return Reference to the elements array of this set.
     */
    HYP_NODISCARD HYP_FORCE_INLINE
    const Array<Element> &GetElements() const
        { return m_elements; }

    /*! \brief Gets the entities in this set.
     *
     *  \return Reference to the entities in this set.
     */
    HYP_NODISCARD HYP_FORCE_INLINE
    const EntityContainer &GetEntityContainer() const
        { return m_entities; }

    /*! \brief Gets a component.
        \return Reference to the component. */
    template <class Component>
    Component &GetComponent(ID<Entity> entity)
    {
        static_assert((std::is_same_v<Components, Component> || ...), "Component not in EntitySet");

        return m_entities.GetEntityData(entity).template GetComponent<Component>();
    }

    /*! \brief Gets a component.
        \return Reference to the component. */
    template <class Component>
    const Component &GetComponent(ID<Entity> entity) const
    {
        static_assert((std::is_same_v<Components, Component> || ...), "Component not in EntitySet");

        return m_entities.GetEntityData(entity).template GetComponent<Component>();
    }

    /*! \brief Remove an Entity from this EntitySet
     *
     *  \param entity The Entity to remove.
     */
    virtual void RemoveEntity(ID<Entity> entity) override
    {
        const auto entity_element_it = m_elements.FindIf([&entity](const Element &element)
        {
            return element.first == entity;
        });

        if (entity_element_it != m_elements.End()) {
            m_elements.Erase(entity_element_it);
        }
    }

    /*! \brief To be used by the EntityManager
        \note Do not call this function directly. */
    virtual void OnEntityUpdated(ID<Entity> entity) override
    {
        const auto entity_element_it = m_elements.FindIf([&entity](const Element &element)
        {
            return element.first == entity;
        });

        if (ValidForEntity(entity)) {
            if (entity_element_it != m_elements.End()) {
                return;
            }

            m_elements.PushBack({
                entity,
                { m_entities.GetEntityData(entity).template GetComponentID<Components>()... }
            });
        } else {
            if (entity_element_it == m_elements.End()) {
                return;
            }

            m_elements.Erase(entity_element_it);
        }
    }

    /*! \brief Checks if an Entity's components are valid for this EntitySet.
     *
     *  \param entity The Entity to check.
     *
     *  \return True if the Entity's components are valid for this EntitySet, false otherwise.
     */
    virtual bool ValidForEntity(ID<Entity> entity) const override
    {
        return m_entities.GetEntityData(entity).template HasComponents<Components...>();
    }

    /*! \brief Iterates over the entities in this EntitySet, in parallel.
     *
     *  \param task_system The task system to use for parallelization.
     *  \param lambda The lambda to call for each entity.
     */
    template <class TaskSystem, class Lambda>
    void ParallelForEach(TaskSystem &task_system, Lambda &&lambda)
    {
        task_system.ParallelForEach(m_elements.Size(), [this, &lambda](uint index, uint)
        {
            lambda(Iterator(*this, index));
        });
    }

    HYP_DEF_STL_BEGIN_END(
        Iterator(*this, 0),
        Iterator(*this, m_elements.Size())
    )

private:
    Array<Element>                                  m_elements;

    EntityContainer                                 &m_entities;
    Tuple< ComponentContainer<Components> &... >    m_component_containers;
};

template <class ... Components>
const EntitySetTypeID EntitySet<Components...>::type_id = EntitySetBase::GenerateEntitySetTypeID<Components...>();

} // namespace hyperion

#endif
