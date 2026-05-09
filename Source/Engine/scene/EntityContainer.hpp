/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/TypeMap.hpp>
#include <Core/containers/Map.hpp>
#include <Core/containers/Array.hpp>
#include <Core/containers/SparsePagedArray.hpp>

#include <Core/threading/DataRaceDetector.hpp>

#include <Core/reflection/ObjId.hpp>
#include <Core/reflection/Handle.hpp>

#include <engine/EngineMemory.hpp>

#include <scene/ComponentContainer.hpp>

namespace Hyperion {

HYP_API extern size_t GetNumDescendants(TypeId typeId);
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
        SparsePagedArray<EntityData, 256, SceneAllocator> data;
    };

public:
    HYP_API static EntityContainer& GetDefaultInstance();

    EntityContainer()
    {
        // +1 for Entity itself
        m_subtypeData.Resize(GetNumDescendants(TypeId::ForType<Entity>()) + 1);
    }

    HYP_FORCE_INLINE Span<SubtypeData> GetSubtypeData()
    {
        return m_subtypeData.ToSpan();
    }

    HYP_FORCE_INLINE Span<const SubtypeData> GetSubtypeData() const
    {
        return m_subtypeData.ToSpan();
    }

    void Add(const Handle<Entity>& entity)
    {
        AssertDebug(entity != nullptr);

        const ObjId<Entity> id = entity.Id();

        SubtypeData& subtypeData = GetSubtypeData(id.GetTypeId());
        AssertDebug(!subtypeData.data.HasIndex(id.ToIndex()), "Entity with ID {} already exists in EntityContainer!", id);

        subtypeData.data.Emplace(id.ToIndex(), EntityData { entity.ToWeak() });
    }

    bool Remove(ObjId<Entity> id)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!id.IsValid())
        {
            return false;
        }

        const TypeId typeId = id.GetTypeId();
        SubtypeData& subtypeData = GetSubtypeData(typeId);

        if (!subtypeData.data.HasIndex(id.ToIndex()))
        {
            return false;
        }

        subtypeData.data.EraseAt(id.ToIndex());

        return true;
    }

    bool HasEntity(ObjId<Entity> id) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!id.IsValid())
        {
            return false;
        }

        const TypeId typeId = id.GetTypeId();
        const SubtypeData& subtypeData = GetSubtypeData(typeId);

        return subtypeData.data.HasIndex(id.ToIndex());
    }

    EntityData* TryGetEntityData(ObjId<Entity> id)
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!id.IsValid())
        {
            return nullptr;
        }

        const TypeId typeId = id.GetTypeId();
        SubtypeData& subtypeData = GetSubtypeData(typeId);

        return subtypeData.data.TryGet(id.ToIndex());
    }

    const EntityData* TryGetEntityData(ObjId<Entity> id) const
    {
        HYP_MT_CHECK_READ(m_dataRaceDetector);

        if (!id.IsValid())
        {
            return nullptr;
        }

        const TypeId typeId = id.GetTypeId();
        const SubtypeData& subtypeData = GetSubtypeData(typeId);

        return subtypeData.data.TryGet(id.ToIndex());
    }

    HYP_FORCE_INLINE EntityData& GetEntityData(ObjId<Entity> id)
    {
        EntityData* data = TryGetEntityData(id);
        AssertDebug(data != nullptr);

        return *data;
    }

    HYP_FORCE_INLINE const EntityData& GetEntityData(ObjId<Entity> id) const
    {
        const EntityData* data = TryGetEntityData(id);
        AssertDebug(data != nullptr);

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

    Array<SubtypeData, SceneAllocator> m_subtypeData;

    HYP_DECLARE_MT_CHECK(m_dataRaceDetector);
};

} // namespace Hyperion
