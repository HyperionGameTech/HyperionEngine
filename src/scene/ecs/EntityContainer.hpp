/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_CONTAINER_HPP
#define HYPERION_ECS_ENTITY_CONTAINER_HPP

#include <core/containers/FlatMap.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/containers/HashMap.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Spinlock.hpp>

#include <core/object/ObjId.hpp>
#include <core/Handle.hpp>

#include <scene/ecs/ComponentContainer.hpp>

namespace hyperion {

class Entity;

struct EntityData
{
    TypeId type_id;
    TypeMap<ComponentId> components;

    template <class Component>
    HYP_FORCE_INLINE bool HasComponent() const
    {
        return components.Contains<Component>();
    }

    HYP_FORCE_INLINE bool HasComponent(TypeId component_type_id) const
    {
        return components.Contains(component_type_id);
    }

    template <class... Components>
    HYP_FORCE_INLINE bool HasComponents() const
    {
        return (HasComponent<Components>() && ...);
    }

    HYP_FORCE_INLINE bool HasComponents(Span<const TypeId> component_type_ids) const
    {
        for (const TypeId& type_id : component_type_ids)
        {
            if (!components.Contains(type_id))
            {
                return false;
            }
        }

        return true;
    }

    template <class Component>
    HYP_FORCE_INLINE ComponentId GetComponentId() const
    {
        return components.At<Component>();
    }

    HYP_FORCE_INLINE ComponentId GetComponentId(TypeId component_type_id) const
    {
        return components.At(component_type_id);
    }

    template <class Component>
    HYP_FORCE_INLINE Optional<ComponentId> TryGetComponentId() const
    {
        auto it = components.Find<Component>();

        if (it == components.End())
        {
            return {};
        }

        return it->second;
    }

    HYP_FORCE_INLINE Optional<ComponentId> TryGetComponentId(TypeId component_type_id) const
    {
        auto it = components.Find(component_type_id);

        if (it == components.End())
        {
            return {};
        }

        return it->second;
    }

    template <class Component>
    HYP_FORCE_INLINE typename TypeMap<ComponentId>::Iterator FindComponent()
    {
        return components.Find<Component>();
    }

    HYP_FORCE_INLINE typename TypeMap<ComponentId>::Iterator FindComponent(TypeId component_type_id)
    {
        return components.Find(component_type_id);
    }

    template <class Component>
    HYP_FORCE_INLINE typename TypeMap<ComponentId>::ConstIterator FindComponent() const
    {
        return components.Find<Component>();
    }

    HYP_FORCE_INLINE typename TypeMap<ComponentId>::ConstIterator FindComponent(TypeId component_type_id) const
    {
        return components.Find(component_type_id);
    }
};

class EntityContainer
{
public:
    using Iterator = HashMap<Entity*, EntityData>::Iterator;
    using ConstIterator = HashMap<Entity*, EntityData>::ConstIterator;

    HYP_FORCE_INLINE void Add(TypeId type_id, Entity* entity)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        auto it = m_entities.Insert(entity, { type_id });
        AssertThrow(it.second);
    }

    HYP_FORCE_INLINE EntityData* TryGetEntityData(const Entity* entity)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_entities.FindAs(entity);

        return it != m_entities.End() ? &it->second : nullptr;
    }

    HYP_FORCE_INLINE const EntityData* TryGetEntityData(const Entity* entity) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        auto it = m_entities.FindAs(entity);

        return it != m_entities.End() ? &it->second : nullptr;
    }

    HYP_FORCE_INLINE EntityData& GetEntityData(const Entity* entity)
    {
        EntityData* data = TryGetEntityData(entity);
        AssertThrow(data != nullptr);

        return *data;
    }

    HYP_FORCE_INLINE const EntityData& GetEntityData(const Entity* entity) const
    {
        const EntityData* data = TryGetEntityData(entity);
        AssertThrow(data != nullptr);

        return *data;
    }

    HYP_FORCE_INLINE Iterator Find(const Entity* entity)
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.FindAs(entity);
    }

    HYP_FORCE_INLINE ConstIterator Find(const Entity* entity) const
    {
        HYP_MT_CHECK_READ(m_data_race_detector);

        return m_entities.FindAs(entity);
    }

    HYP_FORCE_INLINE void Erase(ConstIterator it)
    {
        HYP_MT_CHECK_RW(m_data_race_detector);

        m_entities.Erase(it);
    }

    HYP_DEF_STL_BEGIN_END(m_entities.Begin(), m_entities.End())

private:
    HashMap<Entity*, EntityData> m_entities;

    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

} // namespace hyperion

#endif