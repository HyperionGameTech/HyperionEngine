/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCENE_OCTREE_HPP
#define HYPERION_SCENE_OCTREE_HPP

#include <util/octree/Octree.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/Pair.hpp>

#include <core/Handle.hpp>

#include <scene/Entity.hpp>
#include <scene/VisibilityState.hpp>

#include <scene/ecs/EntityTag.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/Ray.hpp>

#include <Types.hpp>

// #define HYP_OCTREE_DEBUG

namespace hyperion {

class Entity;
class EntityManager;
class Camera;

class Octree;

class HYP_API Octree final : public OctreeBase<Octree, Entity*>
{
public:
    Octree(const Handle<EntityManager>& entity_manager);
    Octree(const Handle<EntityManager>& entity_manager, const BoundingBox& aabb);
    Octree(const Handle<EntityManager>& entity_manager, const BoundingBox& aabb, Octree* parent, uint8 index);

    virtual ~Octree() override;

    VisibilityState& GetVisibilityState()
    {
        return m_visibility_state;
    }

    const VisibilityState& GetVisibilityState() const
    {
        return m_visibility_state;
    }

    /*! \brief Get the EntityManager the Octree is using to manage entities.
     *  \returns The EntityManager the Octree is set to use */
    const Handle<EntityManager>& GetEntityManager() const
    {
        return m_entity_manager;
    }

    /*! \brief Set the EntityManager for the Octree to use. For internal use from \ref{Scene} only
     *  \internal */
    void SetEntityManager(const Handle<EntityManager>& entity_manager);

    /*! \brief Get a hashcode of all entities currently in this Octant that have the given tags (child octants affect this too)
     */
    template <EntityTag... Tags>
    HYP_FORCE_INLINE HashCode GetEntryListHash() const
    {
        static_assert(((uint32(Tags) < uint32(EntityTag::DESCRIPTOR_MAX)) && ...), "All tags must have a value < EntityTag::DESCRIPTOR_MAX");

        const uint32 mask = ((Tags == EntityTag::NONE ? 0 : (1u << (uint32(Tags) - 1))) | ...);

        return HashCode(m_entry_hashes[mask])
            .Add(m_invalidation_marker);
    }

    /*! \brief Get a hashcode of all entities currently in this Octant that match the mask tag (child octants affect this too)
     */
    HYP_FORCE_INLINE HashCode GetEntryListHash(uint32 entity_tag_mask) const
    {
        AssertThrow(entity_tag_mask < m_entry_hashes.Size());

        return HashCode(m_entry_hashes[entity_tag_mask])
            .Add(m_invalidation_marker);
    }

    HYP_FORCE_INLINE Octree* GetChildOctant(OctantId octant_id)
    {
        return static_cast<Octree*>(OctreeBase::GetChildOctant(octant_id));
    }

    void NextVisibilityState();
    void CalculateVisibility(const Handle<Camera>& camera);

    bool TestRay(const Ray& ray, RayTestResults& out_results, bool use_bvh = true) const;

    virtual void Clear() override;
    virtual InsertResult Rebuild(const BoundingBox& new_aabb, bool allow_grow) override;
    virtual void PerformUpdates() override;

private:
    static constexpr uint32 num_entry_hashes = 1u << uint32(EntityTag::DESCRIPTOR_MAX);

    virtual UniquePtr<Octree> CreateChildOctant(const BoundingBox& aabb, Octree* parent, uint8 index) override
    {
        return MakeUnique<Octree>(m_entity_manager, aabb, this, index);
    }

    void ResetEntriesHash();
    void RebuildEntriesHash(uint32 level = 0);

    void UpdateVisibilityState(const Handle<Camera>& camera, uint16 validity_marker);

    Handle<EntityManager> m_entity_manager;

    FixedArray<HashCode, num_entry_hashes> m_entry_hashes;

    VisibilityState m_visibility_state;
};

} // namespace hyperion

#endif
