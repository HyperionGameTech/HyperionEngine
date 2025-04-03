/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_SET_HPP
#define HYPERION_ECS_ENTITY_SET_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/ValueStorage.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <scene/Entity.hpp>
#include <scene/ecs/EntityContainer.hpp>
#include <scene/ecs/ComponentContainer.hpp>
#include <scene/ecs/EntitySetBase.hpp>

#include <Types.hpp>

namespace hyperion {

template <class... Components>
class EntitySet;

template <class... Components>
struct EntitySetIterator
{
    EntitySet<Components...>    &set;
    SizeType                    index;

    EntitySetIterator(EntitySet<Components...> &set, SizeType index)
        : set(set),
          index(index)
    {
    }

    EntitySetIterator(const EntitySetIterator &other)                   = default;
    EntitySetIterator &operator=(const EntitySetIterator &other)        = default;
    EntitySetIterator(EntitySetIterator &&other) noexcept               = default;
    EntitySetIterator &operator=(EntitySetIterator &&other) noexcept    = default;
    ~EntitySetIterator()                                                = default;

    HYP_FORCE_INLINE EntitySetIterator &operator++()
        { ++index; return *this; }

    HYP_FORCE_INLINE EntitySetIterator operator++(int)
        { EntitySetIterator it = *this; ++index; return it; }

    HYP_FORCE_INLINE bool operator==(const EntitySetIterator &other) const
        { return std::addressof(set) == std::addressof(other.set) && index == other.index; }

    HYP_FORCE_INLINE bool operator!=(const EntitySetIterator &other) const
        { return !(*this == other); }

    Tuple<ID<Entity>, Components &...> operator*()
    {
        const typename EntitySet<Components...>::Element &element = set.m_elements[index];

        return ConcatTuples(
            MakeTuple(element.first),
            GetComponents(element.second, std::make_index_sequence<sizeof...(Components)>())
        );
    }

    Tuple<ID<Entity>, const Components &...> operator*() const
        { return const_cast<EntitySetIterator *>(this)->operator*(); }

    Tuple<ID<Entity>, Components &...> operator->()
        { return **this; }

    Tuple<ID<Entity>, const Components &...> operator->() const
        { return **this; }

private:
    template <SizeType... Indices>
    Tuple<Components &...> GetComponents(const FixedArray<ComponentID, sizeof...(Components)> &component_ids, std::index_sequence<Indices...>)
    {
        return Tuple< Components &... >(
            set.m_component_containers.template GetElement< ComponentContainer<Components> & >().GetComponent(component_ids[Indices])...
        );
    }
};

template <class... Components>
struct EntitySetView;

/*! \brief A set of entities with a specific set of components.
 *
 *  \tparam Components The components that the entities in this set have.
 */
template <class... Components>
class EntitySet final : public EntitySetBase
{
public:
    friend struct EntitySetIterator<Components...>;
    friend struct EntitySetView<Components...>;

    using Element = Pair<ID<Entity>, FixedArray<ComponentID, sizeof...(Components)>>;

    using Iterator = EntitySetIterator<Components...>;
    using ConstIterator = EntitySetIterator<const Components...>;

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

    virtual SizeType Size() const override
        { return m_elements.Size(); }

    /*! \brief Gets the elements array of this set.
     *  The elements array contains the entities in this set and the corresponding component IDs.
     *
     *  \return Reference to the elements array of this set.
     */
    HYP_FORCE_INLINE const Array<Element> &GetElements() const
        { return m_elements; }

    /*! \brief Gets the entities in this set.
     *
     *  \return Reference to the entities in this set.
     */
    HYP_FORCE_INLINE const EntityContainer &GetEntityContainer() const
        { return m_entities; }

    /*! \brief Gets a component.
        \return Reference to the component. */
    template <class Component>
    HYP_FORCE_INLINE Component &GetComponent(ID<Entity> entity)
    {
        static_assert((std::is_same_v<Components, Component> || ...), "Component not in EntitySet");

        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.GetEntityData(entity).template GetComponent<Component>();
    }

    /*! \brief Gets a component.
        \return Reference to the component. */
    template <class Component>
    HYP_FORCE_INLINE const Component &GetComponent(ID<Entity> entity) const
    {
        static_assert((std::is_same_v<Components, Component> || ...), "Component not in EntitySet");
        
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.GetEntityData(entity).template GetComponent<Component>();
    }

    /*! \brief Remove an Entity from this EntitySet
     *
     *  \param entity The Entity to remove.
     */
    virtual void RemoveEntity(ID<Entity> entity) override
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

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
        HYP_MT_CHECK_RW(m_data_race_detector);

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
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.GetEntityData(entity).template HasComponents<Components...>();
    }
    
#ifdef HYP_ENABLE_MT_CHECK
    /*! \brief Get a scoped view of this EntitySet. The view will have its access determined by \ref{data_access_flags}.
     *  \return A scoped view of this EntitySet.
     */
    HYP_FORCE_INLINE EntitySetView<Components...> GetScopedView(EnumFlags<DataAccessFlags> data_access_flags, ANSIStringView current_function = "", ANSIStringView message = "")
        { return EntitySetView<Components...>(*this, data_access_flags, current_function, message); }
    
    /*! \brief Get a scoped view of this EntitySet. The view will have its access determined by \ref{component_infos}.
     *  \param component_infos The ComponentInfo objects to use for the view.
     *  \return A scoped view of this EntitySet.
     */
    HYP_FORCE_INLINE EntitySetView<Components...> GetScopedView(const Array<ComponentInfo> &component_infos, ANSIStringView current_function = "", ANSIStringView message = "")
        { return EntitySetView<Components...>(*this, component_infos); }
#else
    /*! \brief Get a scoped view of this EntitySet. The view will have its access determined by \ref{data_access_flags}.
    *  \return A scoped view of this EntitySet.
    */
    template <class... Args>
    HYP_FORCE_INLINE EntitySetView<Components...> GetScopedView(Args &&... args)
    {
        // don't use the passed args
        ((void)args, ...);
        
        return EntitySetView<Components...>(*this);
    }
#endif

    HYP_DEF_STL_BEGIN_END(
        Iterator(*this, 0),
        Iterator(*this, m_elements.Size())
    )

private:
    Array<Element>                                  m_elements;

    EntityContainer                                 &m_entities;
    Tuple< ComponentContainer<Components> &... >    m_component_containers;

    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

template <class... Components>
struct EntitySetView
{
    using Iterator = EntitySetIterator<Components...>;
    using ConstIterator = EntitySetIterator<const Components...>;

    EntitySet<Components...>                                                    &entity_set;

#ifdef HYP_ENABLE_MT_CHECK
    FixedArray<DataRaceDetector *, sizeof...(Components)>                       m_component_data_race_detectors;
    ValueStorageArray<DataRaceDetector::DataAccessScope, sizeof...(Components)> m_component_data_access_scopes;
#endif

#ifdef HYP_ENABLE_MT_CHECK
    EntitySetView(EntitySet<Components...> &entity_set, EnumFlags<DataAccessFlags> data_access_flags, ANSIStringView current_function = "", ANSIStringView message = "")
        : entity_set(entity_set),
          m_component_data_race_detectors { &entity_set.m_component_containers.template GetElement< ComponentContainer<Components> & >().GetDataRaceDetector()... }
    {
        if constexpr (sizeof...(Components) != 0) {
            for (SizeType i = 0; i < m_component_data_race_detectors.Size(); i++) {
                m_component_data_access_scopes[i].Construct(data_access_flags, *m_component_data_race_detectors[i], DataRaceDetector::DataAccessState { current_function, message });
            }
        }
    }

    EntitySetView(EntitySet<Components...> &entity_set, const Array<ComponentInfo> &component_infos, ANSIStringView current_function = "", ANSIStringView message = "")
        : entity_set(entity_set),
          m_component_data_race_detectors { &entity_set.m_component_containers.template GetElement< ComponentContainer<Components> & >().GetDataRaceDetector()... }
    {
        static const FixedArray<TypeID, sizeof...(Components)> component_type_ids = { TypeID::ForType<Components>()... };

        if constexpr (sizeof...(Components) != 0) {
            for (SizeType i = 0; i < m_component_data_race_detectors.Size(); i++) {
                auto component_infos_it = component_infos.FindIf([type_id = component_type_ids[i]](const ComponentInfo &info)
                {
                    return info.type_id == type_id;
                });

                AssertThrowMsg(component_infos_it != component_infos.End(), "Component info not found for component with type ID %u", component_type_ids[i].Value());

                EnumFlags<DataAccessFlags> access_flags = DataAccessFlags::ACCESS_NONE;

                if (component_infos_it->rw_flags & COMPONENT_RW_FLAGS_READ) {
                    access_flags |= DataAccessFlags::ACCESS_READ;
                }

                if (component_infos_it->rw_flags & COMPONENT_RW_FLAGS_WRITE) {
                    access_flags |= DataAccessFlags::ACCESS_WRITE;
                }

                m_component_data_access_scopes[i].Construct(access_flags, *m_component_data_race_detectors[i], DataRaceDetector::DataAccessState { current_function, message });
            }
        }
    }
#else
    EntitySetView(EntitySet<Components...> &entity_set, ...)
        : entity_set(entity_set)
    {
    }
#endif

    EntitySetView(const EntitySetView &other)                   = delete;
    EntitySetView &operator=(const EntitySetView &other)        = delete;
    EntitySetView(EntitySetView &&other) noexcept               = delete;
    EntitySetView &operator=(EntitySetView &&other) noexcept    = delete;

    ~EntitySetView()
    {
#ifdef HYP_ENABLE_MT_CHECK
        if constexpr (sizeof...(Components) != 0) {
            for (SizeType i = 0; i < m_component_data_access_scopes.Size(); i++) {
                m_component_data_access_scopes[i].Destruct();
            }
        }
#endif
    }

    HYP_DEF_STL_BEGIN_END(
        entity_set.Begin(),
        entity_set.End()
    )
};

} // namespace hyperion

#endif
