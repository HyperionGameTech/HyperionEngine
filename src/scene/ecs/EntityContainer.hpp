/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_CONTAINER_HPP
#define HYPERION_ECS_ENTITY_CONTAINER_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Spinlock.hpp>

#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <scene/ecs/ComponentContainer.hpp>

namespace hyperion {

class Entity;

struct EntityData
{
    TypeMap<ComponentID> components;

    template <class Component>
    HYP_FORCE_INLINE bool HasComponent() const
    {
        return components.Contains<Component>();
    }

    HYP_FORCE_INLINE bool HasComponent(TypeID component_type_id) const
    {
        return components.Contains(component_type_id);
    }

    template <class... Components>
    HYP_FORCE_INLINE bool HasComponents() const
    {
        return (HasComponent<Components>() && ...);
    }

    template <class Component>
    HYP_FORCE_INLINE ComponentID GetComponentID() const
    {
        return components.At<Component>();
    }

    HYP_FORCE_INLINE ComponentID GetComponentID(TypeID component_type_id) const
    {
        return components.At(component_type_id);
    }

    template <class Component>
    HYP_FORCE_INLINE Optional<ComponentID> TryGetComponentID() const
    {
        auto it = components.Find<Component>();

        if (it == components.End())
        {
            return {};
        }

        return it->second;
    }

    HYP_FORCE_INLINE Optional<ComponentID> TryGetComponentID(TypeID component_type_id) const
    {
        auto it = components.Find(component_type_id);

        if (it == components.End())
        {
            return {};
        }

        return it->second;
    }

    template <class Component>
    HYP_FORCE_INLINE typename TypeMap<ComponentID>::Iterator FindComponent()
    {
        return components.Find<Component>();
    }

    HYP_FORCE_INLINE typename TypeMap<ComponentID>::Iterator FindComponent(TypeID component_type_id)
    {
        return components.Find(component_type_id);
    }

    template <class Component>
    HYP_FORCE_INLINE typename TypeMap<ComponentID>::ConstIterator FindComponent() const
    {
        return components.Find<Component>();
    }

    HYP_FORCE_INLINE typename TypeMap<ComponentID>::ConstIterator FindComponent(TypeID component_type_id) const
    {
        return components.Find(component_type_id);
    }
};

class EntityContainer
{
public:
    using Iterator = HashMap<WeakHandle<Entity>, EntityData>::Iterator;
    using ConstIterator = HashMap<WeakHandle<Entity>, EntityData>::ConstIterator;

    HYP_FORCE_INLINE void AddEntity(const Handle<Entity>& handle)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        auto it = m_entities.Insert(handle.ToWeak(), {});
        AssertThrow(it.second);
    }

    HYP_FORCE_INLINE void AddEntity(const Handle<Entity>& handle, const EntityData& entity_data)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        auto it = m_entities.Insert(handle.ToWeak(), entity_data);
        AssertThrow(it.second);
    }

    HYP_FORCE_INLINE void AddEntity(const Handle<Entity>& handle, EntityData&& entity_data)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        auto it = m_entities.Insert(handle.ToWeak(), std::move(entity_data));
        AssertThrow(it.second);
    }

    HYP_FORCE_INLINE EntityData* TryGetEntityData(ID<Entity> id)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_entities.FindAs(id);

        return it != m_entities.End() ? &it->second : nullptr;
    }

    HYP_FORCE_INLINE const EntityData* TryGetEntityData(ID<Entity> id) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_entities.FindAs(id);

        return it != m_entities.End() ? &it->second : nullptr;
    }

    HYP_FORCE_INLINE EntityData& GetEntityData(ID<Entity> id)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_entities.FindAs(id);
        AssertThrow(it != m_entities.End());

        return it->second;
    }

    HYP_FORCE_INLINE const EntityData& GetEntityData(ID<Entity> id) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_entities.FindAs(id);
        AssertThrow(it != m_entities.End());

        return it->second;
    }

    HYP_FORCE_INLINE Iterator Find(const Handle<Entity>& entity)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.FindAs(entity);
    }

    HYP_FORCE_INLINE ConstIterator Find(const Handle<Entity>& entity) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.FindAs(entity);
    }

    HYP_FORCE_INLINE Iterator Find(const WeakHandle<Entity>& entity)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.Find(entity);
    }

    HYP_FORCE_INLINE ConstIterator Find(const WeakHandle<Entity>& entity) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.Find(entity);
    }

    HYP_FORCE_INLINE Iterator Find(ID<Entity> id)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.FindAs(id);
    }

    HYP_FORCE_INLINE ConstIterator Find(ID<Entity> id) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.FindAs(id);
    }

    HYP_FORCE_INLINE void Erase(ConstIterator it)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        m_entities.Erase(it);
    }

    HYP_DEF_STL_BEGIN_END(
        m_entities.Begin(),
        m_entities.End())

private:
    HashMap<WeakHandle<Entity>, EntityData> m_entities;

    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

} // namespace hyperion

#endif