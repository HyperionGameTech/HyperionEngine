/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/SceneOctree.hpp>
#include <scene/Entity.hpp>
#include <scene/Node.hpp>
#include <scene/BVH.hpp>

#include <scene/EntityManager.hpp>
#include <scene/components/VisibilityStateComponent.hpp>
#include <scene/components/TransformComponent.hpp>
#include <scene/components/MeshComponent.hpp>

#include <scene/camera/Camera.hpp>

#include <rendering/Mesh.hpp>

#include <core/object/HypClass.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

SceneOctree::SceneOctree(const Handle<EntityManager>& entityManager)
    : OctreeBase(),
      m_entityManager(entityManager)
{
}

SceneOctree::SceneOctree(const Handle<EntityManager>& entityManager, const BoundingBox& aabb)
    : OctreeBase(aabb),
      m_entityManager(entityManager)
{
}

SceneOctree::SceneOctree(const Handle<EntityManager>& entityManager, SceneOctree* parent, const BoundingBox& aabb, uint8 index)
    : OctreeBase(parent, aabb, index),
      m_entityManager(entityManager)
{
}

SceneOctree::~SceneOctree()
{
    // to ensure VisibilityStateComponents are updated
    Clear();
}

void SceneOctree::SetEntityManager(const Handle<EntityManager>& entityManager)
{
    HYP_SCOPE;

    m_entityManager = entityManager;

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->SetEntityManager(m_entityManager);
        }
    }
}

void SceneOctree::Collect(Array<Entity*>& outEntities) const
{
    outEntities.Reserve(outEntities.Size() + m_payload.entries.Count());

    for (const SceneOctreePayload::Entry& entry : m_payload.entries)
    {
        outEntities.PushBack(entry.value);
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->Collect(outEntities);
        }
    }
}

void SceneOctree::Collect(const BoundingSphere& bounds, Array<Entity*>& outEntities) const
{
    if (!bounds.Overlaps(m_aabb))
    {
        return;
    }

    outEntities.Reserve(outEntities.Size() + m_payload.entries.Count());

    for (const SceneOctreePayload::Entry& entry : m_payload.entries)
    {
        if (bounds.Overlaps(entry.aabb))
        {
            outEntities.PushBack(entry.value);
        }
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->Collect(bounds, outEntities);
        }
    }
}

void SceneOctree::Collect(const BoundingBox& bounds, Array<Entity*>& outEntities) const
{
    if (!m_aabb.Overlaps(bounds))
    {
        return;
    }

    outEntities.Reserve(outEntities.Size() + m_payload.entries.Count());

    for (const SceneOctreePayload::Entry& entry : m_payload.entries)
    {
        if (bounds.Overlaps(entry.aabb))
        {
            outEntities.PushBack(entry.value);
        }
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->Collect(bounds, outEntities);
        }
    }
}

void SceneOctree::Clear()
{
    HYP_SCOPE;

    Array<SceneOctreePayload> payloads;
    OctreeBase::Clear(payloads, /* undivide */ true);

    if (m_entityManager)
    {
        Assert(Threads::IsOnThread(m_entityManager->GetOwnerThreadId()));

        for (SceneOctreePayload& payload : payloads)
        {
            for (SceneOctreePayload::Entry& entry : payload.entries)
            {
                Entity* entity = entry.value;

                if (!entity)
                {
                    continue;
                }

                if (VisibilityStateComponent* visibilityStateComponent = m_entityManager->TryGetComponent<VisibilityStateComponent>(entity))
                {
                    visibilityStateComponent->octantId = OctantId::Invalid();
                    visibilityStateComponent->visibilityState = nullptr;
                }

                m_entityManager->AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);

                if (UseEntityMap())
                {
                    SceneOctreeState* stateCasted = static_cast<SceneOctreeState*>(m_state);

                    stateCasted->entityToOctant.Erase(entity);
                }
            }
        }
    }

    if (IsRoot() && UseEntityMap())
    {
        SceneOctreeState* stateCasted = static_cast<SceneOctreeState*>(m_state);

        stateCasted->entityToOctant.Clear();
    }

    RebuildEntriesHash();
}

SceneOctree::Result SceneOctree::Rebuild()
{
    if (IsRoot())
    {
        return Rebuild(BoundingBox::Empty(), /* allowGrow */ g_flags[OF_ALLOW_GROW_ROOT]);
    }
    else
    {
        // if we are not root, we can't grow this octant as it would invalidate the rules of an octree!
        return Rebuild(m_aabb, /* allowGrow */ false);
    }
}

SceneOctree::Result SceneOctree::RebuildExtend_Internal(const BoundingBox& extendIncludeAabb)
{
    if (!extendIncludeAabb.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "AABB is in invalid state");
    }

    if (!extendIncludeAabb.IsFinite())
    {
        return HYP_MAKE_ERROR(Error, "AABB is not finite");
    }

    // have to grow the aabb by rebuilding the octree
    BoundingBox newAabb(m_aabb.Union(extendIncludeAabb));
    // grow our new aabb by a predetermined growth factor,
    // to keep it from constantly resizing
    newAabb *= g_growthFactor;

    return Rebuild(newAabb, /* allowGrow */ false);
}

SceneOctree::Result SceneOctree::Rebuild(const BoundingBox& newAabb, bool allowGrow)
{
    Array<SceneOctreePayload> payloads;
    OctreeBase::Clear(payloads, /* undivide */ true);

    m_aabb = newAabb;

    AssertDebug(!allowGrow || IsRoot(), "allowGrow=true is only allowed on the root node");

    for (SceneOctreePayload& payload : payloads)
    {
        for (SceneOctreePayload::Entry& entry : payload.entries)
        {
            if (allowGrow)
            {
                if (entry.aabb.IsValid() && entry.aabb.IsFinite())
                {
                    m_aabb = m_aabb.Union(entry.aabb);
                }
            }

            if (UseEntityMap() && entry.value != nullptr)
            {
                SceneOctreeState* stateCasted = static_cast<SceneOctreeState*>(m_state);

                stateCasted->entityToOctant.Erase(entry.value);
            }
        }
    }

    InitOctants();

    for (SceneOctreePayload& payload : payloads)
    {
        for (SceneOctreePayload::Entry& entry : payload.entries)
        {
            Entity* entity = entry.value;
            Assert(entity != nullptr);

            if (entry.aabb.IsValid() && entry.aabb.IsFinite())
            {
                AssertDebug(m_aabb.Contains(entry.aabb));
            }

            OctreeBase::Result insertResult = Insert(entity, entry.aabb, true /* allow rebuild */);

            if (insertResult.HasError())
            {
                return insertResult;
            }

            VisibilityStateComponent* visibilityStateComponent = m_entityManager->TryGetComponent<VisibilityStateComponent>(entity);

            if (visibilityStateComponent)
            {
                visibilityStateComponent->octantId = insertResult.GetValue();
                visibilityStateComponent->visibilityState = nullptr;
            }
            else
            {
                m_entityManager->AddComponent<VisibilityStateComponent>(entity, VisibilityStateComponent { .octantId = insertResult.GetValue(), .visibilityState = nullptr });
            }

            m_entityManager->AddTag<EntityTag::UPDATE_VISIBILITY_STATE>(entity);
        }
    }

    return m_octantId;
}

void SceneOctree::PerformUpdates()
{
    HYP_SCOPE;

    Assert(m_state != nullptr);

    if (!m_state->IsDirty())
    {
        // No octant to rebuild, skipping
        return;
    }

    Assert(m_state != nullptr);

    if (m_state->NeedsRebuild())
    {
        SceneOctree* octant = GetChildOctant(m_state->dirtyState.octantId);
        Assert(octant != nullptr);

        const Result rebuildResult = octant->Rebuild();
        AssertDebug(!rebuildResult.HasError(), "Failed to rebuild Octree: {}", rebuildResult.GetError().GetMessage());

        if (!rebuildResult.HasError())
        {
            // set rebuild state back to invalid if rebuild was successful
            m_state->dirtyState = {};
        }
    }

    RebuildEntriesHash();
}

SceneOctree::Result SceneOctree::Insert(Entity* entity, const BoundingBox& aabb, bool allowRebuild)
{
    HYP_SCOPE;

    AssertDebug(entity != nullptr);

    if (!entity)
    {
        return HYP_MAKE_ERROR(Error, "Cannot insert null entity into octree");
    }

    if (g_flags & OF_INSERT_ON_OVERLAP)
    {
        if (!aabb.IsValid() || !aabb.IsFinite() || aabb.IsZero())
        {
            return HYP_MAKE_ERROR(Error, "Attempting to insert invalid AABB into Octree: {}", aabb);
        }
    }

    if (aabb.IsValid() && aabb.IsFinite())
    {
        if (IsRoot())
        {
            if (!m_aabb.Contains(aabb) && (g_flags & OF_ALLOW_GROW_ROOT))
            {
                if (allowRebuild)
                {
                    Result rebuildResult = RebuildExtend_Internal(aabb);

                    if (rebuildResult.HasError())
                    {
                        return rebuildResult;
                    }
                }
                else
                {
                    // mark octree to be rebuilt
                    m_state->MarkOctantDirty(m_octantId, true);
                }
            }
        }
        else
        {
            if (!m_aabb.Overlaps(aabb))
            {
                return HYP_MAKE_ERROR(Error, "Entry AABB outside of octant AABB");
            }
        }

        // stop recursing if we are at max depth
        if (m_octantId.GetDepth() < g_maxDepth - 1)
        {
            bool wasInserted = false;

            for (Octant& octant : m_octants)
            {
                if (!((g_flags & OF_INSERT_ON_OVERLAP) ? octant.aabb.Overlaps(aabb) : octant.aabb.Contains(aabb)))
                {
                    continue;
                }

                if (!IsDivided())
                {
                    if (!allowRebuild)
                    {
                        // do not use this octant if it has not been divided yet.
                        // instead, we'll insert into the THIS octant, marking it as dirty,
                        // so it will get added to the correct octant on Rebuild().

                        m_state->MarkOctantDirty(m_octantId, true);

                        // insert into parent for now (will be rebuilt)
                        return Insert_Internal(entity, aabb);
                    }

                    Divide();
                }

                Assert(octant.octree != nullptr);

                Result insertResult = octant.octree->Insert(entity, aabb, allowRebuild);
                wasInserted |= bool(insertResult.HasValue());

                if (g_flags & OF_INSERT_ON_OVERLAP)
                {
                    AssertDebug(insertResult.HasValue(), "Failed to insert into overlapping octant! Message: {}", insertResult.GetError().GetMessage());
                }
                else
                {
                    // return on first call to Insert() on child octant - child fully contains the aabb
                    return insertResult;
                }
            }

            if (wasInserted)
            {
                return m_octantId;
            }
        }
    }

    return Insert_Internal(entity, aabb);
}

SceneOctree::Result SceneOctree::Insert_Internal(Entity* entity, const BoundingBox& aabb)
{
    if (UseEntityMap())
    {
        SceneOctreeState* stateCasted = static_cast<SceneOctreeState*>(m_state);

        if (stateCasted->entityToOctant.Find(entity) != stateCasted->entityToOctant.End())
        {
            return HYP_MAKE_ERROR(Error, "Entity {} already exists in entity to octant map", entity->Id());
        }

        stateCasted->entityToOctant[entity] = this;
    }

#ifdef HYP_DEBUG_MODE
    AssertDebug(entity->InstanceClass() == Entity::Class(),
        "Cannot use insert subclass of Entity into SceneOctree: {}",
        entity->InstanceClass()->GetName());
#endif

    m_payload.entries.Set(entity->Id().ToIndex(), SceneOctreePayload::Entry { entity, aabb });

    // mark dirty (not for rebuild), but ontant id has changed
    m_state->MarkOctantDirty(m_octantId);

    return m_octantId;
}

SceneOctree::Result SceneOctree::Remove(Entity* entity, bool allowRebuild)
{
    HYP_SCOPE;

    if (!entity)
    {
        return m_octantId;
    }

    if (UseEntityMap())
    {
        SceneOctreeState* stateCasted = static_cast<SceneOctreeState*>(m_state);

        const auto it = stateCasted->entityToOctant.Find(entity);

        if (it != stateCasted->entityToOctant.End())
        {
            if (SceneOctree* octant = it->second)
            {
                return octant->Remove_Internal(entity, allowRebuild);
            }
        }
    }

    return Remove_Internal(entity, allowRebuild);
}

SceneOctree::Result SceneOctree::Remove_Internal(Entity* entity, bool allowRebuild)
{
#ifdef HYP_DEBUG_MODE
    AssertDebug(entity->InstanceClass() == Entity::Class(),
        "Cannot use remove subclass of Entity from SceneOctree: {}",
        entity->InstanceClass()->GetName());
#endif

    const SizeType entryIndex = entity->Id().ToIndex();
    SceneOctreePayload::Entry* entry = m_payload.entries.TryGet(entryIndex);

    if (!entry)
    {
        if (m_isDivided)
        {
            bool wasRemoved = false;

            for (Octant& octant : m_octants)
            {
                Assert(octant.octree != nullptr);

                if (g_flags & OF_INSERT_ON_OVERLAP)
                {
                    wasRemoved |= bool(octant.octree->Remove_Internal(entity, allowRebuild));
                }
                else
                {
                    if (Result octantResult = octant.octree->Remove_Internal(entity, allowRebuild))
                    {
                        return octantResult;
                    }
                }
            }

            if (wasRemoved)
            {
                return m_octantId;
            }
        }

        return HYP_MAKE_ERROR(Error, "Could not be removed from any sub octants and not found in this octant");
    }

    if (UseEntityMap())
    {
        SceneOctreeState* stateCasted = static_cast<SceneOctreeState*>(m_state);

        auto entryToOctantIt = stateCasted->entityToOctant.Find(entity);

        if (entryToOctantIt != stateCasted->entityToOctant.End())
        {
            stateCasted->entityToOctant.Erase(entryToOctantIt);
        }
    }

    m_payload.entries.EraseAt(entryIndex);

    m_state->MarkOctantDirty(m_octantId);

    if (!m_isDivided && m_payload.entries.Empty())
    {
        SceneOctree* lastEmptyParent = nullptr;

        if (SceneOctree* parent = m_parent)
        {
            const SceneOctree* child = this;

            while (parent->EmptyDeep(DEPTH_SEARCH_INF, 0xff & ~(1 << child->m_octantId.GetIndex())))
            { // do not search this branch of the tree again
                lastEmptyParent = parent;

                if (parent->m_parent == nullptr)
                {
                    break;
                }

                child = parent;
                parent = child->m_parent;
            }
        }

        if (lastEmptyParent != nullptr)
        {
            Assert(lastEmptyParent->EmptyDeep(DEPTH_SEARCH_INF));

            /* At highest empty parent octant, call Undivide() to collapse entries */
            if (allowRebuild)
            {
                lastEmptyParent->Undivide();
            }
            else
            {
                m_state->MarkOctantDirty(lastEmptyParent->GetOctantID(), true);
            }
        }
    }

    return m_octantId;
}

SceneOctree::Result SceneOctree::Move(Entity* entity, const BoundingBox& aabb, bool allowRebuild, SceneOctreePayload::Entry* entry)
{
    HYP_SCOPE;

    AssertDebug(entity != nullptr);

#ifdef HYP_DEBUG_MODE
    AssertDebug(entity->InstanceClass() == Entity::Class(),
        "Cannot use move subclass of Entity in SceneOctree: {}",
        entity->InstanceClass()->GetName());
#endif

    const BoundingBox& newAabb = aabb;

    const bool isRoot = IsRoot();
    const bool contains = ContainsAabb(aabb);

    if (!contains)
    {
        // NO LONGER CONTAINS AABB

        if (isRoot)
        {
            // have to rebuild, invalidating child octants.
            // which we have a ContainsAabb() check for child entries walking upwards

            if (allowRebuild)
            {
                return RebuildExtend_Internal(newAabb);
            }
            else
            {
                m_state->MarkOctantDirty(m_octantId, true);

                // Moved outside of the root octree, but we keep it here for now.
                // Next call of PerformUpdates(), we will extend the octree.
                return m_octantId;
            }
        }

        // not root

        Optional<Result> parentInsertResult;

        /* Contains is false at this point */
        SceneOctree* parent = m_parent;
        SceneOctree* lastParent = parent;

        while (parent != nullptr)
        {
            lastParent = parent;

            if (parent->ContainsAabb(newAabb))
            {
                if (entry)
                {
                    if (UseEntityMap())
                    {
                        SceneOctreeState* stateCasted = static_cast<SceneOctreeState*>(m_state);

                        auto jt = stateCasted->entityToOctant.Find(entity);

                        if (jt != stateCasted->entityToOctant.End())
                        {
                            stateCasted->entityToOctant.Erase(jt);
                        }
                    }

                    m_payload.entries.EraseAt(entry->value->Id().ToIndex());
                }

                parentInsertResult = parent->Move(entity, aabb, allowRebuild, nullptr);

                break;
            }

            parent = parent->m_parent;
        }

        if (parentInsertResult.HasValue())
        { // succesfully inserted, safe to call CollapseParents()
            // Entry has now been added to it's appropriate octant which is a parent of this -
            // collapse this up to the top
            CollapseParents(allowRebuild);

            return parentInsertResult.Get();
        }

        // not inserted because no Move() was called on parents (because they don't contain AABB),
        // have to _manually_ call Move() which will go to the above branch for the parent octant,
        // this invalidating `this`

        Assert(lastParent != nullptr);

        return lastParent->Move(entity, aabb, allowRebuild, nullptr);
    }

    // CONTAINS AABB HERE

    if (allowRebuild)
    {
        bool wasMoved = false;

        // Check if we can go deeper.
        for (Octant& octant : m_octants)
        {
            if ((g_flags & OF_INSERT_ON_OVERLAP) ? octant.aabb.Overlaps(newAabb) : octant.aabb.Contains(newAabb))
            {
                if (entry)
                {
                    if (UseEntityMap())
                    {
                        SceneOctreeState* stateCasted = static_cast<SceneOctreeState*>(m_state);

                        auto jt = stateCasted->entityToOctant.Find(entity);

                        if (jt != stateCasted->entityToOctant.End())
                        {
                            stateCasted->entityToOctant.Erase(jt);
                        }
                    }

                    m_payload.entries.EraseAt(entry->value->Id().ToIndex());
                }

                if (!IsDivided())
                {
                    if (allowRebuild && m_octantId.GetDepth() < int(g_maxDepth) - 1)
                    {
                        Divide();
                    }
                    else
                    {
                        // no point checking other octants
                        break;
                    }
                }

                AssertDebug(octant.octree != nullptr);

                if (g_flags & OF_INSERT_ON_OVERLAP)
                {
                    wasMoved |= bool(octant.octree->Move(entity, aabb, allowRebuild, nullptr));
                }
                else
                {
                    return octant.octree->Move(entity, aabb, allowRebuild, nullptr);
                }
            }
        }

        if (wasMoved)
        {
            return m_octantId;
        }
    }
    else
    {
        m_state->MarkOctantDirty(m_octantId, true);
    }

    // needs hash recalculation due to the entry changing, at minimum
    m_state->MarkOctantDirty(m_octantId);

    if (entry != nullptr)
    {
        /* Not moved out of this octant (for now) */
        entry->aabb = newAabb;
    }
    else
    {
        /* Moved into this octant */
        m_payload.entries.Set(entity->Id().ToIndex(), SceneOctreePayload::Entry { entity, newAabb });

        if (UseEntityMap())
        {
            static_cast<SceneOctreeState*>(m_state)->entityToOctant[entity] = this;
        }
    }

    return m_octantId;
}

SceneOctree::Result SceneOctree::Update(Entity* entity, const BoundingBox& aabb, bool forceInvalidation, bool allowRebuild)
{
    HYP_SCOPE;

    AssertDebug(entity != nullptr);

    if (!entity)
    {
        return HYP_MAKE_ERROR(Error, "Cannot update null entity in octree");
    }

    if (UseEntityMap())
    {
        SceneOctreeState* stateCasted = static_cast<SceneOctreeState*>(m_state);

        const auto it = stateCasted->entityToOctant.Find(entity);

        if (it == stateCasted->entityToOctant.End())
        {
            return HYP_MAKE_ERROR(Error, "Object not found in entry map!");
        }

        if (it->second)
        {
            return it->second->Update_Internal(entity, aabb, forceInvalidation, allowRebuild);
        }

        return HYP_MAKE_ERROR(Error, "Object has no octree in entry map!");
    }

    return Update_Internal(entity, aabb, forceInvalidation, allowRebuild);
}

SceneOctree::Result SceneOctree::Update_Internal(Entity* entity, const BoundingBox& aabb, bool forceInvalidation, bool allowRebuild)
{
#ifdef HYP_DEBUG_MODE
    AssertDebug(entity->InstanceClass() == Entity::Class(),
        "Cannot use insert subclass of Entity into SceneOctree: {}",
        entity->InstanceClass()->GetName());
#endif

    const SizeType entryIndex = entity->Id().ToIndex();
    SceneOctreePayload::Entry* entry = m_payload.entries.TryGet(entryIndex);

    if (!entry)
    {
        if (m_isDivided)
        {
            bool wasUpdated = false;

            for (Octant& octant : m_octants)
            {
                Assert(octant.octree != nullptr);

                if (g_flags & OF_INSERT_ON_OVERLAP)
                {
                    wasUpdated |= bool(octant.octree->Update_Internal(entity, aabb, forceInvalidation, allowRebuild));
                }
                else
                {
                    Result updateInternalResult = octant.octree->Update_Internal(entity, aabb, forceInvalidation, allowRebuild);

                    if (updateInternalResult.HasValue())
                    {
                        return updateInternalResult;
                    }
                }
            }

            if (wasUpdated)
            {
                return m_octantId;
            }
        }

        return HYP_MAKE_ERROR(Error, "Could not update in any sub octants");
    }

    if (forceInvalidation)
    {
        // force invalidation of this entry so the octant's hash will be updated
        Invalidate();
    }

    const BoundingBox& newAabb = aabb;
    const BoundingBox& oldAabb = entry->aabb;

    if (newAabb == oldAabb)
    {
        /* AABB has not changed - no need to update */
        return m_octantId;
    }

    /* AABB has changed to we remove it from this octree and either:
     * If we don't contain it anymore - insert it from the highest level octree that still contains the aabb and then walking down from there
     * If we do still contain it - we will remove it from this octree and re-insert it to find the deepest child octant
     */

    return Move(entity, newAabb, allowRebuild, entry);
}

void SceneOctree::NextVisibilityState()
{
    HYP_SCOPE;

    Assert(IsRoot());

    m_visibilityState.Next();
}

void SceneOctree::CalculateVisibility(const Handle<Camera>& camera)
{
    HYP_SCOPE;

    Assert(IsRoot());

    UpdateVisibilityState(camera, m_visibilityState.validityMarker);
}

void SceneOctree::UpdateVisibilityState(const Handle<Camera>& camera, uint16 validityMarker)
{
    if (!camera.IsValid())
    {
        return;
    }

    const Frustum& frustum = camera->GetFrustum();

    if (!frustum.ContainsAABB(m_aabb))
    {
        return;
    }

    SceneOctree* current = this;
    uint8 childIndex = uint8(-1);

    while (true)
    {
        // Process current node.
        current->m_visibilityState.validityMarker = validityMarker;
        current->m_visibilityState.MarkAsValid(camera.Id());

        if (current->m_isDivided)
        {
            bool descended = false;

            for (uint8 i = childIndex + 1; i < 8; i++)
            {
                if (!frustum.ContainsAABB(current->m_octants[i].aabb))
                {
                    continue;
                }

                current = current->m_octants[i].octree;
                childIndex = uint8(-1);

                descended = true;

                break;
            }
            if (descended)
            {
                continue;
            }
        }

        if (current->m_parent == nullptr)
        {
            break;
        }

        childIndex = current->m_octantId.GetIndex();
        current = current->m_parent;
    }
}

void SceneOctree::ResetEntriesHash()
{
    HYP_SCOPE;

    m_entryHashes = {};
}

void SceneOctree::RebuildEntriesHash(uint32 level)
{
    HYP_SCOPE;

    ResetEntriesHash();

    for (SceneOctreePayload::Entry& entry : m_payload.entries)
    {
        const HashCode entryHashCode = entry.GetHashCode();
        m_entryHashes[0].Add(entryHashCode);

        uint32 tagsMask = 0;

        if (m_entityManager)
        {
            tagsMask = m_entityManager->GetSavableTagsMask(entry.value);
        }

        FOR_EACH_BIT(tagsMask, i)
        {
            EntityTag tag = EntityTag(i + 1);
            AssertDebug(uint32(tag) < m_entryHashes.Size());

            // sanity check, temp.
            AssertDebug(m_entityManager->HasTag(entry.value, tag));

            m_entryHashes[uint32(tag)].Add(entryHashCode);
        }
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->RebuildEntriesHash(level + 1);
        }
    }

    // Update parent's hash to include this octant's hash
    if (m_parent)
    {
        for (uint32 i = 0; i < uint32(m_entryHashes.Size()); i++)
        {
            m_parent->m_entryHashes[i].Add(m_entryHashes[i]);
        }
    }
}

bool SceneOctree::TestRay(const Ray& ray, RayTestResults& outResults, bool useBvh) const
{
    HYP_SCOPE;

    bool hasHit = false;

    if (ray.TestAABB(m_aabb))
    {
        for (const SceneOctreePayload::Entry& entry : m_payload.entries)
        {
            if (!entry.value || (entry.value->InstanceClass() != Entity::Class()))
            {
                // we only want Entity instances, as we store their ID as a raw value (and they can have a mesh)
                continue;
            }

            RayTestResults aabbResult;

            if (useBvh)
            {
                // If the entity has a BVH associated with it, use that instead of the AABB for more accuracy
                if (MeshComponent* meshComponent = m_entityManager->TryGetComponent<MeshComponent>(entry.value);
                    meshComponent && meshComponent->mesh && meshComponent->mesh->GetBVH().IsValid())
                {
                    Matrix4 modelMatrix = Matrix4::Identity();
                    Matrix4 normalMatrix = Matrix4::Identity();

                    Ray localSpaceRay = ray;

                    if (TransformComponent* transformComponent = m_entityManager->TryGetComponent<TransformComponent>(entry.value))
                    {
                        modelMatrix = transformComponent->transform.GetMatrix();

                        Matrix4 invModelMatrix = modelMatrix.Inverted();
                        normalMatrix = invModelMatrix.Transposed();

                        localSpaceRay = invModelMatrix * ray;
                    }

                    RayTestResults localBvhResults = meshComponent->mesh->GetBVH().TestRay(localSpaceRay);

                    if (localBvhResults.Any())
                    {
                        RayTestResults bvhResults;

                        for (RayHit hit : localBvhResults)
                        {
                            hit.id = entry.value->Id().Value();
                            hit.userData = nullptr;

                            Vec4f transformedNormal = normalMatrix * Vec4f(hit.normal, 0.0f);
                            hit.normal = transformedNormal.GetXYZ().Normalized();

                            Vec4f transformedPosition = modelMatrix * Vec4f(hit.hitpoint, 1.0f);
                            transformedPosition /= transformedPosition.w;

                            hit.hitpoint = transformedPosition.GetXYZ();

                            hit.distance = (hit.hitpoint - ray.position).Length();

                            bvhResults.AddHit(hit);
                        }

                        outResults.Merge(std::move(bvhResults));

                        hasHit = true;
                    }

                    continue;
                }
                else
                {
                    HYP_LOG(Scene, Warning, "Entity #{} (node: {}) does not have a BVH component, using AABB instead", entry.value->Id(), entry.value->GetName());
                }
            }

            if (ray.TestAABB(entry.aabb, entry.value->Id().Value(), nullptr, aabbResult))
            {
                outResults.Merge(std::move(aabbResult));

                hasHit = true;
            }
        }

        if (m_isDivided)
        {
            for (const Octant& octant : m_octants)
            {
                Assert(octant.octree != nullptr);

                if (octant.octree->TestRay(ray, outResults))
                {
                    hasHit = true;
                }
            }
        }
    }

    return hasHit;
}

} // namespace hyperion
