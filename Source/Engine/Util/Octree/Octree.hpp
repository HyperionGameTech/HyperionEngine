/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/Set.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Utilities/Pair.hpp>
#include <Core/Utilities/Optional.hpp>
#include <Core/Utilities/Result.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/BoundingSphere.hpp>

#include <Core/Types.hpp>

#include <Util/Octree/OctantId.hpp>

// #define HYP_OCTREE_DEBUG

namespace Hyperion {

/*! \brief Base class for an octree
 *  \tparam Derived The derived class type (used for CRTP)
 *  \tparam Payload The payload to be stored in each octant node. Must have an Empty() method, used to determine Octant occupancy
 *  This class provides the basic functionality for an octree, including insertion, removal, and querying of entries.
 */
template <class Derived, class Payload>
class OctreeBase;

/*! \brief State of the octree, used to track which octants need to be rebuilt and maps entries to their respective octants.
 *  \tparam Derived The derived class type (used for CRTP)
 *  \tparam Payload The payload to be stored in each octant node. Must have an Empty() method, used to determine Octant occupancy
 *  \internal Used by OctreeBase to manage the state of the octree. */
template <class Derived, class Payload>
struct OctreeState
{
    struct DirtyState
    {
        OctantId octantId = OctantId::Invalid();
        bool needsRebuild = false;
    };

    OctreeState() = default;
    OctreeState(const OctreeState& other) = delete;
    OctreeState& operator=(const OctreeState& other) = delete;

    virtual ~OctreeState() = default;

    // If any octants need to be rebuilt, their topmost parent that needs to be rebuilt will be stored here
    DirtyState dirtyState;

    HYP_FORCE_INLINE bool NeedsRebuild() const
    {
        return dirtyState.needsRebuild && dirtyState.octantId != OctantId::Invalid();
    }

    HYP_FORCE_INLINE bool IsDirty() const
    {
        return dirtyState.octantId != OctantId::Invalid();
    }

    void MarkOctantDirty(OctantId octantId, bool needsRebuild = false);
};

enum OctreeFlags : uint8
{
    OF_NONE = 0x0,
    OF_INSERT_ON_OVERLAP = 0x2, //!< Insert into child octant on overlap rather than contains check of the Entry's AABB. Will cause entries to be contained in multiple octants rather than one.
    OF_ALLOW_GROW_ROOT = 0x4,
    OF_DEFAULT = OF_ALLOW_GROW_ROOT
};

HYP_MAKE_ENUM_FLAGS(OctreeFlags);

template <class Derived, class Payload>
class OctreeBase
{
protected:
    enum
    {
        DEPTH_SEARCH_INF = -1,
        DEPTH_SEARCH_ONLY_THIS = 0
    };

    static constexpr float GrowthFactor = 1.5f;
    static const BoundingBox DefaultBounds;

    static constexpr EnumFlags<OctreeFlags> Flags = OctreeFlags::OF_DEFAULT;

    OctreeBase();
    OctreeBase(const BoundingBox& aabb);
    OctreeBase(Derived* parent, const BoundingBox& aabb, uint8 index);
    virtual ~OctreeBase();

public:
    using Result = utilities::TResult<OctantId>;

    struct Octant
    {
        Derived* octree = nullptr;
        BoundingBox aabb;
    };

    OctreeBase(const OctreeBase& other) = delete;
    OctreeBase& operator=(const OctreeBase& other) = delete;
    OctreeBase(OctreeBase&& other) noexcept = delete;
    OctreeBase& operator=(OctreeBase&& other) noexcept = delete;

    HYP_FORCE_INLINE const Payload& GetPayload() const
    {
        return m_payload;
    }

    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    HYP_FORCE_INLINE OctantId GetOctantID() const
    {
        return m_octantId;
    }

    HYP_FORCE_INLINE const FixedArray<Octant, 8>& GetOctants() const
    {
        return m_octants;
    }

    /*! \brief Get the child (nested) octant with the specified index
     *  \param octantId The OctantId to use to find the octant (see OctantId struct)
     *  \return The octant with the specified index, or nullptr if it doesn't exist
     */
    Derived* GetChildOctant(OctantId octantId);

    HYP_FORCE_INLINE bool IsDivided() const
    {
        return m_isDivided;
    }

    void Clear();
    void Clear(Array<Payload>& outPayloads, bool undivide);

    Result Insert(const Payload& payload, const BoundingBox& aabb);

    bool GetNearestOctants(const Vec3f& position, FixedArray<Derived*, 8>& out) const;
    bool GetNearestOctant(const Vec3f& position, Derived const*& out) const;
    bool GetFittingOctant(const BoundingBox& aabb, Derived const*& out) const;

    HYP_FORCE_INLINE OctreeState<Derived, Payload>* GetState() const
    {
        return m_state;
    }

protected:
    static OctreeState<Derived, Payload>* CreateOctreeState()
    {
        return new OctreeState<Derived, Payload>();
    }

    HYP_FORCE_INLINE uint8 MaxDepth() const
    {
        return OctantId::MaxDepth;
    }

    //! derived classes must implement this
    // Derived* CreateChildOctant(Derived* parent, const BoundingBox& aabb, uint8 index);

    HYP_FORCE_INLINE bool ContainsAabb(const BoundingBox& aabb) const
    {
        return (Derived::Flags & OF_INSERT_ON_OVERLAP) ? m_aabb.Overlaps(aabb) : m_aabb.Contains(aabb);
    }

    HYP_FORCE_INLINE bool IsRoot() const
    {
        return m_parent == nullptr;
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_payload.Empty();
    }

    void SetParent(Derived* parent);
    bool EmptyDeep(int depth = DEPTH_SEARCH_INF, uint8 octantMask = 0xff) const;

    void InitOctants();
    void Divide();
    void Undivide();

    void Invalidate();

    /*! \brief If \p allowRebuild is true, removes any potentially empty octants above the entry.
        If \p allowRebuild is false, marks them as dirty so they get removed on the next call to PerformUpdates()
    */
    void CollapseParents(bool allowRebuild);

    Payload m_payload;

    Derived* m_parent;
    BoundingBox m_aabb;
    FixedArray<Octant, 8> m_octants;
    OctreeState<Derived, Payload>* m_state;
    OctantId m_octantId;

    uint32 m_invalidationMarker : 16;
    bool m_isDivided : 1;
};

#include <Util/Octree/Octree.inc>

} // namespace Hyperion
