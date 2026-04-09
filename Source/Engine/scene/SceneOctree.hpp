/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <util/octree/Octree.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/FixedArray.hpp>
#include <Core/containers/HashMap.hpp>
#include <Core/containers/HashSet.hpp>
#include <Core/containers/SparsePagedArray.hpp>

#include <Core/utilities/Pair.hpp>

#include <Core/reflection/Handle.hpp>

#include <scene/Entity.hpp>
#include <scene/VisibilityState.hpp>
#include <scene/EntityTag.hpp>

#include <Core/math/Vector3.hpp>
#include <Core/math/BoundingBox.hpp>
#include <Core/math/Ray.hpp>

#include <Core/Types.hpp>

// #define HYP_OCTREE_DEBUG

namespace Hyperion {

class Entity;
class EntityManager;
class View;

class SceneOctree;

struct SceneOctreePayload
{
    struct Entry
    {
        Entity* value;
        BoundingBox aabb;

        HYP_FORCE_INLINE Entry() = default;

        HYP_FORCE_INLINE Entry(Entity* value, const BoundingBox& aabb)
            : value(value),
              aabb(aabb)
        {
        }

        HYP_FORCE_INLINE bool operator==(const Entry& other) const
        {
            return value == other.value
                && aabb == other.aabb;
        }

        HYP_FORCE_INLINE bool operator!=(const Entry& other) const
        {
            return value != other.value
                || aabb != other.aabb;
        }

        HYP_FORCE_INLINE ObjId<Entity> GetId() const
        {
            return value ? value->Id() : ObjId<Entity>::invalid;
        }

        HYP_FORCE_INLINE HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(value ? value->Id() : ObjId<Entity>::invalid);
            hc.Add(aabb.GetHashCode());

            return hc;
        }
    };

    using EntrySet = IntrusiveMap<Entry, &Entry::GetId, DynamicAllocator, HashTablePolicy::NotPooled>;

    EntrySet entries;

    HYP_FORCE_INLINE bool Empty() const
    {
        return entries.Empty();
    }
};

struct SceneOctreeState : public OctreeState<SceneOctree, SceneOctreePayload>
{
    HashMap<Entity*, SceneOctree*> entityToOctant;

    virtual ~SceneOctreeState() override = default;
};

class HYP_API SceneOctree final : public OctreeBase<SceneOctree, SceneOctreePayload>
{
    friend class OctreeBase<SceneOctree, SceneOctreePayload>;

    SceneOctree(const Handle<EntityManager>& entityManager, SceneOctree* parent, const BoundingBox& aabb, uint8 index);

public:
    static OctreeState<SceneOctree, SceneOctreePayload>* CreateOctreeState()
    {
        return new SceneOctreeState();
    }

    SceneOctree(const Handle<EntityManager>& entityManager);
    SceneOctree(const Handle<EntityManager>& entityManager, const BoundingBox& aabb);

    ~SceneOctree();

    HYP_FORCE_INLINE const SceneOctreePayload::EntrySet& GetEntries() const
    {
        return m_payload.entries;
    }

    VisibilityState& GetVisibilityState()
    {
        return m_visibilityState;
    }

    const VisibilityState& GetVisibilityState() const
    {
        return m_visibilityState;
    }

    /*! \brief Get the EntityManager the Octree is using to manage entities.
     *  \returns The EntityManager the Octree is set to use */
    const Handle<EntityManager>& GetEntityManager() const
    {
        return m_entityManager;
    }

    /*! \brief Set the EntityManager for the Octree to use. For internal use from \ref Scene only
     *  \internal */
    void SetEntityManager(const Handle<EntityManager>& entityManager);

    /*! \brief Get a hashcode of all entities currently in this Octant that have the given tags (child octants affect this too)
     */
    template <EntityTag Tag>
    HYP_FORCE_INLINE HashCode GetEntryListHash() const
    {
        static_assert((uint32(Tag) < uint32(EntityTag::MaxPersistent)), "All tags must have a value < EntityTag::SAVABLE_MAX");

        return HashCode(m_entryHashes[uint32(Tag)])
            .Add(m_invalidationMarker);
    }

    /*! \brief Get a hashcode of all entities currently in this Octant that match the mask tag (child octants affect this too)
     */
    HYP_FORCE_INLINE HashCode GetEntryListHash(EntityTag entityTag) const
    {
        Assert(uint32(entityTag) < m_entryHashes.Size());

        return HashCode(m_entryHashes[uint32(entityTag)])
            .Add(m_invalidationMarker);
    }

    void NextVisibilityState();
    void CalculateVisibility(const View* view);

    bool TestRay(const Ray& ray, RayTestResults& outResults, EnumFlags<RayTestFlags> flags = RayTestFlags::TestBVH) const;

    void Collect(Array<Entity*>& outEntities) const;
    void Collect(const BoundingSphere& bounds, Array<Entity*>& outEntities) const;
    void Collect(const BoundingBox& bounds, Array<Entity*>& outEntities) const;

    HYP_FORCE_INLINE OctreeBase::Result Insert(const SceneOctreePayload& payload, const BoundingBox& aabb)
    {
        return OctreeBase::Insert(payload, aabb);
    }

    Result Insert(Entity* entity, const BoundingBox& aabb, bool allowRebuild = false);
    Result Remove(Entity* entity, bool allowRebuild = false);

    void Clear();

    HYP_FORCE_INLINE void Clear(Array<SceneOctreePayload>& outPayloads, bool undivide)
    {
        OctreeBase::Clear(outPayloads, undivide);
    }

    Result Rebuild();
    Result Rebuild(const BoundingBox& newAabb, bool allowGrow);

    void PerformUpdates();

    /*! \brief Update a given entity's bounds and assigned octant in the octree.
     * \param entity The Entity to update in the octree
     * \param aabb The new AABB of the entry
     * \param allowRebuild If true, the octree will be rebuilt if the entry doesn't fit in the new octant. Otherwise, the octree will be marked as dirty and rebuilt on the next call to PerformUpdates()
     * \param forceInvalidation If true, the entry will have its invalidation marker incremented, causing the octant's hash to be updated
     */
    Result Update(Entity* entity, const BoundingBox& aabb, bool forceInvalidation = false, bool allowRebuild = false);

private:
    static constexpr uint32 numEntryHashes = uint32(EntityTag::MaxPersistent);

    HYP_FORCE_INLINE bool UseEntityMap() const
    {
        return m_state != nullptr && !Flags[OF_INSERT_ON_OVERLAP];
    }

    SceneOctree* CreateChildOctant(const BoundingBox& aabb, uint8 index)
    {
        return new SceneOctree(m_entityManager, this, aabb, index);
    }

    void ResetEntriesHash();
    void RebuildEntriesHash(uint32 level = 0);

    void UpdateVisibilityState(const View* view, uint16 validityMarker);

    /*! \brief Move the entity to a new octant. If allowRebuild is true, the octree will be rebuilt if the entry doesn't fit in the new octant,
        and subdivided octants will be collapsed if they are empty + new octants will be created if they are needed.
     */
    Result Move(Entity* entity, const BoundingBox& aabb, bool allowRebuild, SceneOctreePayload::Entry* entry);

    Result Insert_Internal(Entity* entity, const BoundingBox& aabb);

    Result Remove_Internal(Entity* entity, bool allowRebuild);

    Result Update_Internal(Entity* entity, const BoundingBox& aabb, bool forceInvalidation, bool allowRebuild);

    Result RebuildExtend_Internal(const BoundingBox& extendIncludeAabb);

    Handle<EntityManager> m_entityManager;

    FixedArray<HashCode, numEntryHashes> m_entryHashes;

    VisibilityState m_visibilityState;
};

} // namespace Hyperion
