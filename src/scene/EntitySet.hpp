/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>

#include <core/utilities/Tuple.hpp>
#include <core/utilities/ValueStorage.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/threading/DataRaceDetector.hpp>

#include <scene/Entity.hpp>
#include <scene/EntityContainer.hpp>
#include <scene/ComponentContainer.hpp>

#include <core/Types.hpp>

namespace hyperion {

template <class... Components>
class EntitySet;

template <class... Components>
struct EntitySetIterator
{
    EntitySet<Components...>& set;
    SizeType index;

    EntitySetIterator(EntitySet<Components...>& set, SizeType index)
        : set(set),
          index(index)
    {
    }

    EntitySetIterator(const EntitySetIterator& other) = default;
    EntitySetIterator& operator=(const EntitySetIterator& other) = default;
    EntitySetIterator(EntitySetIterator&& other) noexcept = default;
    EntitySetIterator& operator=(EntitySetIterator&& other) noexcept = default;
    ~EntitySetIterator() = default;

    HYP_FORCE_INLINE EntitySetIterator& operator++()
    {
        ++index;
        return *this;
    }

    HYP_FORCE_INLINE EntitySetIterator operator++(int)
    {
        EntitySetIterator it = *this;
        ++index;
        return it;
    }

    HYP_FORCE_INLINE bool operator==(const EntitySetIterator& other) const
    {
        return std::addressof(set) == std::addressof(other.set) && index == other.index;
    }

    HYP_FORCE_INLINE bool operator!=(const EntitySetIterator& other) const
    {
        return !(*this == other);
    }

    Tuple<Entity*, Components&...> operator*()
    {
        const typename EntitySet<Components...>::Element& element = set.m_elements[index];

        return ConcatTuples(
            MakeTuple(element.template GetElement<0>()),
            GetComponents(element.template GetElement<2>(), std::make_index_sequence<sizeof...(Components)>()));
    }

    Tuple<Entity*, const Components&...> operator*() const
    {
        return const_cast<EntitySetIterator*>(this)->operator*();
    }

    Tuple<Entity*, Components&...> operator->()
    {
        return **this;
    }

    Tuple<Entity*, const Components&...> operator->() const
    {
        return **this;
    }

private:
    template <SizeType... Indices>
    Tuple<Components&...> GetComponents(const FixedArray<ComponentId, sizeof...(Components)>& componentIds, std::index_sequence<Indices...>)
    {
        return Tuple<Components&...>(
            set.m_componentContainers.template GetElement<ComponentContainer<Components>&>().GetComponent(componentIds[Indices])...);
    }
};

template <class... Components>
struct EntitySetView;

class EntitySetBase
{
public:
    EntitySetBase() = default;
    EntitySetBase(const EntitySetBase& other) = delete;
    EntitySetBase& operator=(const EntitySetBase& other) = delete;
    EntitySetBase(EntitySetBase&& other) noexcept = delete;
    EntitySetBase& operator=(EntitySetBase&& other) noexcept = delete;
    virtual ~EntitySetBase() = default;

    virtual SizeType Size() const = 0;

    /*! \brief Checks if an Entity's components are valid for this EntitySet.
     *
     *  \param entity The Entity to check.
     *
     *  \return True if the Entity's components are valid for this EntitySet, false otherwise.
     */
    virtual bool ValidForEntity(Entity* entity) const = 0;

    /*! \brief Removes the given Entity from this EntitySet.
     *
     *  \param entity The Entity to remove.
     */
    virtual void RemoveEntity(Entity* entity) = 0;

    /*! \brief To be used by the EntityManager
        \note Do not call this function directly. */
    virtual void OnEntityUpdated(Entity* entity) = 0;
};

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

    using Element = Tuple<Entity*, TypeId, FixedArray<ComponentId, sizeof...(Components)>>;

    using Iterator = EntitySetIterator<Components...>;
    using ConstIterator = EntitySetIterator<const Components...>;

    EntitySet(EntityContainer& entities, ComponentContainer<Components>&... componentContainers)
        : m_entities(entities),
          m_componentContainers(componentContainers...)
    {
        for (auto& subtypeData : m_entities.GetSubtypeData())
        {
            for (auto it = subtypeData.data.Begin(); it != subtypeData.data.Begin(); ++it)
            {
                EntityData& entityData = *it;
                
                Entity* entity = entityData.entityWeak.GetUnsafe();
                Assert(entity != nullptr);
                
                OnEntityUpdated(entity);
            }
        }
    }

    EntitySet(const EntitySet& other) = delete;
    EntitySet& operator=(const EntitySet& other) = delete;
    EntitySet(EntitySet&& other) noexcept = delete;
    EntitySet& operator=(EntitySet&& other) noexcept = delete;
    virtual ~EntitySet() override = default;

    virtual SizeType Size() const override
    {
        return m_elements.Size();
    }

    /*! \brief Gets the elements array of this set.
     *  The elements array contains the entities in this set and the corresponding component IDs.
     *
     *  \return Reference to the elements array of this set.
     */
    HYP_FORCE_INLINE const Array<Element>& GetElements() const
    {
        return m_elements;
    }

    /*! \brief Gets the entities in this set.
     *
     *  \return Reference to the entities in this set.
     */
    HYP_FORCE_INLINE const EntityContainer& GetEntityContainer() const
    {
        return m_entities;
    }

    /*! \brief Remove an Entity from this EntitySet
     *
     *  \param entity The Entity to remove.
     */
    virtual void RemoveEntity(Entity* entity) override
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        const auto entityElementIt = m_elements.FindIf([entity](const Element& element)
            {
                return element.template GetElement<0>() == entity;
            });

        if (entityElementIt != m_elements.End())
        {
            m_elements.Erase(entityElementIt);
        }
    }

    /*! \brief To be used by the EntityManager
        \note Do not call this function directly. */
    virtual void OnEntityUpdated(Entity* entity) override
    {
        HYP_MT_CHECK_RW(m_dataRaceDetector);

        const auto entityElementIt = m_elements.FindIf([entity](const Element& element)
            {
                return element.template GetElement<0>() == entity;
            });

        if (ValidForEntity(entity))
        {
            if (entityElementIt != m_elements.End())
            {
                return;
            }

            EntityData& entityData = m_entities.GetEntityData(entity);

            m_elements.EmplaceBack(entity, entityData.entityWeak.GetTypeId(), FixedArray<ComponentId, sizeof...(Components)> { entityData.template GetComponentId<Components>()... });
        }
        else
        {
            if (entityElementIt == m_elements.End())
            {
                return;
            }

            m_elements.Erase(entityElementIt);
        }
    }

    /*! \brief Checks if an Entity's components are valid for this EntitySet.
     *
     *  \param entity The Entity to check.
     *
     *  \return True if the Entity's components are valid for this EntitySet, false otherwise.
     */
    virtual bool ValidForEntity(Entity* entity) const override
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        return m_entities.GetEntityData(entity).template HasComponents<Components...>();
    }

#ifdef HYP_ENABLE_MT_CHECK
    /*! \brief Get a scoped view of this EntitySet. The view will have its access determined by \ref{dataAccessFlags}.
     *  \return A scoped view of this EntitySet.
     */
    HYP_FORCE_INLINE EntitySetView<Components...> GetScopedView(EnumFlags<DataAccessFlags> dataAccessFlags, ANSIStringView currentFunction = "", ANSIStringView message = "")
    {
        return EntitySetView<Components...>(*this, dataAccessFlags, currentFunction, message);
    }

    /*! \brief Get a scoped view of this EntitySet. The view will have its access determined by \ref{componentInfos}.
     *  \param componentInfos The ComponentInfo objects to use for the view.
     *  \return A scoped view of this EntitySet.
     */
    HYP_FORCE_INLINE EntitySetView<Components...> GetScopedView(const Array<ComponentInfo>& componentInfos, ANSIStringView currentFunction = "", ANSIStringView message = "")
    {
        return EntitySetView<Components...>(*this, componentInfos);
    }
#else
    /*! \brief Get a scoped view of this EntitySet. The view will have its access determined by \ref{dataAccessFlags}.
     *  \return A scoped view of this EntitySet.
     */
    template <class... Args>
    HYP_FORCE_INLINE EntitySetView<Components...> GetScopedView(Args&&... args)
    {
        // don't use the passed args
        ((void)args, ...);

        return EntitySetView<Components...>(*this);
    }
#endif

    HYP_DEF_STL_BEGIN_END(Iterator(*this, 0), Iterator(*this, m_elements.Size()))

private:
    Array<Element> m_elements;

    EntityContainer& m_entities;
    Tuple<ComponentContainer<Components>&...> m_componentContainers;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

template <class... Components>
struct EntitySetView
{
    using Iterator = EntitySetIterator<Components...>;
    using ConstIterator = EntitySetIterator<const Components...>;

    EntitySet<Components...>& entitySet;

#ifdef HYP_ENABLE_MT_CHECK
    FixedArray<DataRaceDetector*, sizeof...(Components)> m_componentDataRaceDetectors;
    ValueStorage<DataRaceDetector::DataAccessScope, sizeof...(Components)> m_componentDataAccessScopes;
#endif

#ifdef HYP_ENABLE_MT_CHECK
    EntitySetView(EntitySet<Components...>& entitySet, EnumFlags<DataAccessFlags> dataAccessFlags, ANSIStringView currentFunction = "", ANSIStringView message = "")
        : entitySet(entitySet),
          m_componentDataRaceDetectors { &entitySet.m_componentContainers.template GetElement<ComponentContainer<Components>&>().GetDataRaceDetector()... }
    {
        if constexpr (sizeof...(Components) != 0)
        {
            static const FixedArray<ANSIString, sizeof...(Components)> componentNames = { TypeNameWithoutNamespace<Components>().Data()... };

            for (SizeType i = 0; i < m_componentDataRaceDetectors.Size(); i++)
            {
                m_componentDataAccessScopes.ConstructElement(i, dataAccessFlags, *m_componentDataRaceDetectors[i], DataRaceDetector::DataAccessState { currentFunction, message.Length() != 0 ? message : ANSIStringView(componentNames[i]) });
            }
        }
    }

    EntitySetView(EntitySet<Components...>& entitySet, const Array<ComponentInfo>& componentInfos, ANSIStringView currentFunction = "", ANSIStringView message = "")
        : entitySet(entitySet),
          m_componentDataRaceDetectors { &entitySet.m_componentContainers.template GetElement<ComponentContainer<Components>&>().GetDataRaceDetector()... }
    {

        if constexpr (sizeof...(Components) != 0)
        {
            static const FixedArray<TypeId, sizeof...(Components)> componentTypeIds = { TypeId::ForType<Components>()... };
            static const FixedArray<ANSIString, sizeof...(Components)> componentNames = { TypeNameWithoutNamespace<Components>().Data()... };

            for (SizeType i = 0; i < m_componentDataRaceDetectors.Size(); i++)
            {
                auto componentInfosIt = componentInfos.FindIf([typeId = componentTypeIds[i]](const ComponentInfo& info)
                    {
                        return info.typeId == typeId;
                    });

                Assert(componentInfosIt != componentInfos.End(), "Component info not found for component with TypeId {} ({})", componentTypeIds[i].Value(), componentNames[i]);

                EnumFlags<DataAccessFlags> accessFlags = DataAccessFlags::ACCESS_NONE;

                if (componentInfosIt->rwFlags & COMPONENT_RW_FLAGS_READ)
                {
                    accessFlags |= DataAccessFlags::ACCESS_READ;
                }

                if (componentInfosIt->rwFlags & COMPONENT_RW_FLAGS_WRITE)
                {
                    accessFlags |= DataAccessFlags::ACCESS_WRITE;
                }

                m_componentDataAccessScopes.ConstructElement(i, accessFlags, *m_componentDataRaceDetectors[i], DataRaceDetector::DataAccessState { currentFunction, message.Length() != 0 ? message : ANSIStringView(componentNames[i]) });
            }
        }
    }
#else
    EntitySetView(EntitySet<Components...>& entitySet, ...)
        : entitySet(entitySet)
    {
    }
#endif

    EntitySetView(const EntitySetView& other) = delete;
    EntitySetView& operator=(const EntitySetView& other) = delete;
    EntitySetView(EntitySetView&& other) noexcept = delete;
    EntitySetView& operator=(EntitySetView&& other) noexcept = delete;

    ~EntitySetView()
    {
#ifdef HYP_ENABLE_MT_CHECK
        if constexpr (sizeof...(Components) != 0)
        {
            for (SizeType i = 0; i < m_componentDataAccessScopes.Size(); i++)
            {
                m_componentDataAccessScopes.DestructElement(i);
            }
        }
#endif
    }

    HYP_DEF_STL_BEGIN_END(entitySet.Begin(), entitySet.End())
};

} // namespace hyperion
