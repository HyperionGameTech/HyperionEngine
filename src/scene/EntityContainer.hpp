/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/FlatMap.hpp>
#include <core/containers/TypeMap.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/Array.hpp>
#include <core/containers/SparsePagedArray.hpp>

#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/Spinlock.hpp>

#include <core/utilities/TypeId.hpp>

#include <core/object/ObjId.hpp>
#include <core/object/Handle.hpp>

#include <scene/ComponentContainer.hpp>

namespace hyperion {

HYP_API extern SizeType GetNumDescendants(TypeId typeId);
HYP_API extern int GetSubclassIndex(TypeId baseTypeId, TypeId subclassTypeId);

class Entity;

struct EntityData
{
    // Keep a weak handle around so the Entity pointer doesn't get invalidated and reused
    WeakHandle<Entity> entityWeak;
    TypeMap<ComponentId> components;

    template <class Component>
    HYP_FORCE_INLINE bool HasComponent() const
    {
        return components.Contains<Component>();
    }

    HYP_FORCE_INLINE bool HasComponent(TypeId componentTypeId) const
    {
        return components.Contains(componentTypeId);
    }

    template <class... Components>
    HYP_FORCE_INLINE bool HasComponents() const
    {
        return (HasComponent<Components>() && ...);
    }

    HYP_FORCE_INLINE bool HasComponents(Span<const TypeId> componentTypeIds) const
    {
        for (const TypeId& typeId : componentTypeIds)
        {
            if (!components.Contains(typeId))
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

    HYP_FORCE_INLINE ComponentId GetComponentId(TypeId componentTypeId) const
    {
        return components.At(componentTypeId);
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

    HYP_FORCE_INLINE Optional<ComponentId> TryGetComponentId(TypeId componentTypeId) const
    {
        auto it = components.Find(componentTypeId);

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

    HYP_FORCE_INLINE typename TypeMap<ComponentId>::Iterator FindComponent(TypeId componentTypeId)
    {
        return components.Find(componentTypeId);
    }

    template <class Component>
    HYP_FORCE_INLINE typename TypeMap<ComponentId>::ConstIterator FindComponent() const
    {
        return components.Find<Component>();
    }

    HYP_FORCE_INLINE typename TypeMap<ComponentId>::ConstIterator FindComponent(TypeId componentTypeId) const
    {
        return components.Find(componentTypeId);
    }
};

class EntityContainer
{
    struct SubtypeData
    {
        SparsePagedArray<EntityData, 256> data;
    };

public:
    EntityContainer()
    {
        // +1 for Entity itself
        m_subtypeData.Resize(GetNumDescendants(TypeId::ForType<Entity>()) + 1);
    }

    HYP_FORCE_INLINE Array<SubtypeData>& GetSubtypeData()
    {
        return m_subtypeData;
    }

    HYP_FORCE_INLINE const Array<SubtypeData>& GetSubtypeData() const
    {
        return m_subtypeData;
    }

    HYP_FORCE_INLINE void Add(const Handle<Entity>& entity)
    {
        AssertDebug(entity != nullptr);

        const ObjId<Entity> id = entity.Id();

        SubtypeData& subtypeData = GetSubtypeData(id.GetTypeId());
        AssertDebug(!subtypeData.data.HasIndex(id.ToIndex()), "Entity with ID {} already exists in EntityContainer!", id);

        subtypeData.data.Emplace(id.ToIndex(), EntityData { entity.ToWeak() });
    }

    HYP_FORCE_INLINE bool Remove(const Entity* entity)
    {
        if (!entity)
        {
            return false;
        }

        const TypeId typeId = entity->Id().GetTypeId();
        SubtypeData& subtypeData = GetSubtypeData(typeId);

        if (!subtypeData.data.HasIndex(entity->Id().ToIndex()))
        {
            return false;
        }

        subtypeData.data.EraseAt(entity->Id().ToIndex());

        return true;
    }

    HYP_FORCE_INLINE bool HasEntity(const Entity* entity) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!entity)
        {
            return false;
        }

        const TypeId typeId = entity->Id().GetTypeId();
        const SubtypeData& subtypeData = GetSubtypeData(typeId);

        return subtypeData.data.HasIndex(entity->Id().ToIndex());
    }

    HYP_FORCE_INLINE EntityData* TryGetEntityData(const Entity* entity)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!entity)
        {
            return nullptr;
        }

        const TypeId typeId = entity->Id().GetTypeId();
        SubtypeData& subtypeData = GetSubtypeData(typeId);

        return subtypeData.data.TryGet(entity->Id().ToIndex());
    }

    HYP_FORCE_INLINE const EntityData* TryGetEntityData(const Entity* entity) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!entity)
        {
            return nullptr;
        }

        const TypeId typeId = entity->Id().GetTypeId();
        const SubtypeData& subtypeData = GetSubtypeData(typeId);

        return subtypeData.data.TryGet(entity->Id().ToIndex());
    }

    HYP_FORCE_INLINE EntityData& GetEntityData(const Entity* entity)
    {
        EntityData* data = TryGetEntityData(entity);
        Assert(data != nullptr);

        return *data;
    }

    HYP_FORCE_INLINE const EntityData& GetEntityData(const Entity* entity) const
    {
        const EntityData* data = TryGetEntityData(entity);
        Assert(data != nullptr);

        return *data;
    }

private:
    SubtypeData& GetSubtypeData(TypeId typeId)
    {
        const int classIndex = GetSubclassIndex(TypeId::ForType<Entity>(), typeId) + 1;
        AssertDebug(classIndex >= 0, "Invalid class index {}", classIndex);
        AssertDebug(classIndex < m_subtypeData.Size(), "Invalid class index {}", classIndex);

        return m_subtypeData[classIndex];
    }

    const SubtypeData& GetSubtypeData(TypeId typeId) const
    {
        return const_cast<EntityContainer*>(this)->GetSubtypeData(typeId);
    }

    Array<SubtypeData> m_subtypeData;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace hyperion
