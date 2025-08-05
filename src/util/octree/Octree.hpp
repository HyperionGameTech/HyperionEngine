/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/Optional.hpp>
#include <core/utilities/Result.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <Types.hpp>

// #define HYP_OCTREE_DEBUG

namespace hyperion {

/*! \brief Represents an octant in an octree
 *  \details The bits are ordered as follows:
 *  - 0-2: index of topmost parent octant
 *  - 3-5: index of second parent octant
 *  - 6-8: index of third parent octant
 * ... and so on until the index of the octant itself.
 *
 * The maximum depth of an octree using this scheme is 64 bits / 3 bits for index = 21 octants.
 */
struct OctantId
{
    //! This bit is reserved for invalid octants -- We use 3 bits for each index, leaving 1 bit left on a 64-bit integer
    static constexpr uint64 invalidBits = 1ull << 63;
    static constexpr SizeType maxDepth = 64 / 3;

    uint64 indexBits { 0 };
    uint8 depth { 0 };

    OctantId() = default;

    explicit OctantId(uint64 indexBits, uint8 depth)
        : indexBits(indexBits),
          depth(depth)
    {
    }

    explicit OctantId(uint8 childIndex, OctantId parentId)
        : indexBits(!parentId.IsInvalid()
                  ? parentId.indexBits | (uint64(childIndex) << (uint64(parentId.GetDepth() + uint8(1)) * 3ull))
                  : childIndex),
          depth(parentId.GetDepth() + uint8(1))
    {
    }

    OctantId(const OctantId& other) = default;
    OctantId& operator=(const OctantId& other) = default;
    OctantId(OctantId&& other) noexcept = default;
    OctantId& operator=(OctantId&& other) noexcept = default;
    ~OctantId() = default;

    HYP_FORCE_INLINE bool IsInvalid() const
    {
        return indexBits & invalidBits;
    }

    HYP_FORCE_INLINE bool operator==(const OctantId& other) const
    {
        return indexBits == other.indexBits && depth == other.depth;
    }

    HYP_FORCE_INLINE bool operator!=(const OctantId& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE uint8 GetIndex(uint8 depth) const
    {
        return (indexBits >> (uint64(depth) * 3ull)) & 0x7;
    }

    HYP_FORCE_INLINE uint8 GetIndex() const
    {
        return GetIndex(depth);
    }

    HYP_FORCE_INLINE uint8 GetDepth() const
    {
        return depth;
    }

    HYP_FORCE_INLINE bool IsSiblingOf(OctantId other) const
    {
        return depth == other.depth && (indexBits & ~(~0ull << (uint64(depth) * 3ull))) == (other.indexBits & ~(~0ull << (uint64(depth) * 3ull)));
    }

    HYP_FORCE_INLINE bool IsChildOf(OctantId other) const
    {
        return depth > other.depth && (indexBits & ~(~0ull << (uint64(other.depth) * 3ull))) == other.indexBits;
    }

    HYP_FORCE_INLINE bool IsParentOf(OctantId other) const
    {
        return depth < other.depth && indexBits == (other.indexBits & ~(~0ull << (uint64(depth) * 3ull)));
    }

    HYP_FORCE_INLINE OctantId GetParent() const
    {
        if (depth == 0)
        {
            return OctantId::Invalid();
        }

        return OctantId(indexBits & ~(~0ull << (uint64(depth) * 3ull)), depth - 1);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(indexBits);
        hc.Add(depth);

        return hc;
    }

    /*! \brief Get the special invalid OctantId. */
    static OctantId Invalid()
    {
        // 0x80 For index bit because we reserve the highest bit for invalid octants
        // 0xff for depth because +1 (used for child octant id) will cause it to overflow to 0
        return OctantId(invalidBits, 0xff);
    }
};

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

    static constexpr float g_growthFactor = 1.5f;
    static const BoundingBox g_defaultBounds;

    static constexpr uint8 g_maxDepth = OctantId::maxDepth;
    static constexpr EnumFlags<OctreeFlags> g_flags = OctreeFlags::OF_DEFAULT;

    OctreeBase();
    OctreeBase(const BoundingBox& aabb);
    OctreeBase(Derived* parent, const BoundingBox& aabb, uint8 index);
    ~OctreeBase();

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

    //! derived classes must implement this
    // static Derived* CreateChildOctant(Derived* parent, const BoundingBox& aabb, uint8 index);

    HYP_FORCE_INLINE bool ContainsAabb(const BoundingBox& aabb) const
    {
        return (Derived::g_flags & OF_INSERT_ON_OVERLAP) ? m_aabb.Overlaps(aabb) : m_aabb.Contains(aabb);
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

    /*! \brief If \ref{allowRebuild} is true, removes any potentially empty octants above the entry.
        If \ref{allowRebuild} is false, marks them as dirty so they get removed on the next call to PerformUpdates()
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

#include <util/octree/Octree.inl>

} // namespace hyperion
