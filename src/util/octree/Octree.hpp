/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_OCTREE_HPP
#define HYPERION_OCTREE_HPP

#include <core/containers/Array.hpp>
#include <core/containers/FixedArray.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/HashSet.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/utilities/Pair.hpp>
#include <core/utilities/Optional.hpp>

#include <core/math/Vector3.hpp>
#include <core/math/BoundingBox.hpp>
#include <core/math/BoundingSphere.hpp>

#include <core/algorithm/Every.hpp>

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
 * The maximum depth of an octree using this ID scheme is 64 bits / 3 bits for index = 21 octants.
 */
struct OctantID
{
    // This bit is reserved for invalid octants -- We use 3 bits for each index, leaving 1 bit left on a 64-bit integer
    static constexpr uint64 invalid_bits = 1ull << 63;
    static constexpr SizeType max_depth = 64 / 3;

    uint64 index_bits { 0 };
    uint8 depth { 0 };

    OctantID() = default;

    explicit OctantID(uint64 index_bits, uint8 depth)
        : index_bits(index_bits),
          depth(depth)
    {
    }

    explicit OctantID(uint8 child_index, OctantID parent_id)
        : index_bits(!parent_id.IsInvalid()
                  ? parent_id.index_bits | (uint64(child_index) << (uint64(parent_id.GetDepth() + uint8(1)) * 3ull))
                  : child_index),
          depth(parent_id.GetDepth() + uint8(1))
    {
    }

    OctantID(const OctantID& other) = default;
    OctantID& operator=(const OctantID& other) = default;
    OctantID(OctantID&& other) noexcept = default;
    OctantID& operator=(OctantID&& other) noexcept = default;
    ~OctantID() = default;

    HYP_FORCE_INLINE bool IsInvalid() const
    {
        return index_bits & invalid_bits;
    }

    HYP_FORCE_INLINE bool operator==(const OctantID& other) const
    {
        return index_bits == other.index_bits && depth == other.depth;
    }

    HYP_FORCE_INLINE bool operator!=(const OctantID& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE uint8 GetIndex(uint8 depth) const
    {
        return (index_bits >> (uint64(depth) * 3ull)) & 0x7;
    }

    HYP_FORCE_INLINE uint8 GetIndex() const
    {
        return GetIndex(depth);
    }

    HYP_FORCE_INLINE uint8 GetDepth() const
    {
        return depth;
    }

    HYP_FORCE_INLINE bool IsSiblingOf(OctantID other) const
    {
        return depth == other.depth && (index_bits & ~(~0ull << (uint64(depth) * 3ull))) == (other.index_bits & ~(~0ull << (uint64(depth) * 3ull)));
    }

    HYP_FORCE_INLINE bool IsChildOf(OctantID other) const
    {
        return depth > other.depth && (index_bits & ~(~0ull << (uint64(other.depth) * 3ull))) == other.index_bits;
    }

    HYP_FORCE_INLINE bool IsParentOf(OctantID other) const
    {
        return depth < other.depth && index_bits == (other.index_bits & ~(~0ull << (uint64(depth) * 3ull)));
    }

    HYP_FORCE_INLINE OctantID GetParent() const
    {
        if (depth == 0)
        {
            return OctantID::Invalid();
        }

        return OctantID(index_bits & ~(~0ull << (uint64(depth) * 3ull)), depth - 1);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(index_bits);
        hc.Add(depth);

        return hc;
    }

    /*! \brief Get the special invalid OctantID. */
    static OctantID Invalid()
    {
        // 0x80 For index bit because we reserve the highest bit for invalid octants
        // 0xff for depth because +1 (used for child octant id) will cause it to overflow to 0
        return OctantID(invalid_bits, 0xff);
    }
};

template <class Derived, class TEntry>
class OctreeBase;

template <class Derived, class TEntry>
struct OctreeState
{
    HashMap<TEntry, OctreeBase<Derived, TEntry>*> entry_to_octree;

    // If any octants need to be rebuilt, their topmost parent that needs to be rebuilt will be stored here
    OctantID rebuild_state = OctantID::Invalid();

    HYP_FORCE_INLINE bool NeedsRebuild() const
    {
        return rebuild_state != OctantID::Invalid();
    }

    /*! \brief Mark the octant as dirty, meaning it needs to be rebuilt */
    void MarkOctantDirty(OctantID octant_id);
};

template <class Derived, class TEntry>
class OctreeBase
{
protected:
    enum
    {
        DEPTH_SEARCH_INF = -1,
        DEPTH_SEARCH_ONLY_THIS = 0
    };

    static constexpr float growth_factor = 1.5f;
    // the length value at which to stop recusively dividing
    // for a small enough object
    static constexpr float min_aabb_size = 1.0f;
    static const BoundingBox default_bounds;

public:
    struct Result
    {
        enum
        {
            OCTREE_OK = 0,
            OCTREE_ERR = 1
        } result;

        const char* message;
        int error_code = 0;

        Result()
            : Result(OCTREE_OK)
        {
        }

        Result(decltype(result) result, const char* message = "", int error_code = 0)
            : result(result),
              message(message),
              error_code(error_code)
        {
        }

        Result(const Result& other)
            : result(other.result),
              message(other.message),
              error_code(other.error_code)
        {
        }

        HYP_FORCE_INLINE bool operator!() const
        {
            return result != OCTREE_OK;
        }

        HYP_FORCE_INLINE operator bool() const
        {
            return result == OCTREE_OK;
        }
    };

    using InsertResult = Pair<Result, OctantID>;

    struct Octant
    {
        UniquePtr<OctreeBase> octree;
        BoundingBox aabb;
    };

    struct Entry
    {
        TEntry value;
        BoundingBox aabb;

        Entry() = default;

        Entry(const TEntry& value, const BoundingBox& aabb)
            : value(value),
              aabb(aabb)
        {
        }

        Entry(const Entry& other)
            : value(other.value),
              aabb(other.aabb)
        {
        }

        Entry& operator=(const Entry& other)
        {
            value = other.value;
            aabb = other.aabb;

            return *this;
        }

        Entry(Entry&& other) noexcept
            : value(std::move(other.value)),
              aabb(other.aabb)
        {
            other.aabb = BoundingBox::Empty();
        }

        Entry& operator=(Entry&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            value = std::move(other.value);
            aabb = other.aabb;

            other.aabb = BoundingBox::Empty();

            return *this;
        }

        ~Entry() = default;

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

        HashCode GetHashCode() const
        {
            HashCode hc;

            hc.Add(HashCode::GetHashCode(value));
            hc.Add(aabb.GetHashCode());

            return hc;
        }
    };

    using EntrySet = HashSet<Entry, &Entry::value>;

    OctreeBase();
    OctreeBase(const BoundingBox& aabb);
    OctreeBase(const BoundingBox& aabb, OctreeBase* parent, uint8 index);

    OctreeBase(const OctreeBase& other) = delete;
    OctreeBase& operator=(const OctreeBase& other) = delete;
    OctreeBase(OctreeBase&& other) noexcept = delete;
    OctreeBase& operator=(OctreeBase&& other) noexcept = delete;
    virtual ~OctreeBase();

    HYP_FORCE_INLINE BoundingBox& GetAABB()
    {
        return m_aabb;
    }

    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    HYP_FORCE_INLINE const EntrySet& GetEntries() const
    {
        return m_entries;
    }

    HYP_FORCE_INLINE OctantID GetOctantID() const
    {
        return m_octant_id;
    }

    HYP_FORCE_INLINE const FixedArray<Octant, 8>& GetOctants() const
    {
        return m_octants;
    }

    /*! \brief Get the child (nested) octant with the specified index
     *  \param octant_id The OctantID to use to find the octant (see OctantID struct)
     *  \return The octant with the specified index, or nullptr if it doesn't exist
     */
    OctreeBase* GetChildOctant(OctantID octant_id);

    HYP_FORCE_INLINE bool IsDivided() const
    {
        return m_is_divided;
    }

    virtual void Clear();
    void Clear(Array<Entry>& out_entries, bool undivide);
    void Clear(Array<TEntry>& out_entries, bool undivide);

    InsertResult Insert(const TEntry& value, const BoundingBox& aabb, bool allow_rebuild = false);
    Result Remove(const TEntry& value, bool allow_rebuild = false);

    /*! \brief Update the entry in the octree.
     * \param id value The ID of the entry to update
     * \param aabb The new AABB of the entry
     * \param allow_rebuild If true, the octree will be rebuilt if the entry doesn't fit in the new octant. Otherwise, the octree will be marked as dirty and rebuilt on the next call to PerformUpdates()
     * \param force_invalidation If true, the entry will have its invalidation marker incremented, causing the octant's hash to be updated
     */
    InsertResult Update(const TEntry& value, const BoundingBox& aabb, bool force_invalidation = false, bool allow_rebuild = false);
    InsertResult Rebuild();
    virtual InsertResult Rebuild(const BoundingBox& new_aabb, bool allow_grow);

    void CollectEntries(Array<const TEntry*>& out_entries) const;
    void CollectEntries(const BoundingSphere& bounds, Array<const TEntry*>& out_entries) const;
    void CollectEntries(const BoundingBox& bounds, Array<const TEntry*>& out_entries) const;

    bool GetNearestOctants(const Vec3f& position, FixedArray<Derived*, 8>& out) const;
    bool GetNearestOctant(const Vec3f& position, Derived const*& out) const;
    bool GetFittingOctant(const BoundingBox& aabb, Derived const*& out) const;

    virtual void PerformUpdates();

    HYP_FORCE_INLINE OctreeState<Derived, TEntry>* GetState() const
    {
        return m_state;
    }

protected:
    virtual UniquePtr<Derived> CreateChildOctant(const BoundingBox& aabb, Derived* parent, uint8 index) = 0;

    /*! \brief Move the entry to a new octant. If allow_rebuild is true, the octree will be rebuilt if the entry doesn't fit in the new octant,
        and subdivided octants will be collapsed if they are empty + new octants will be created if they are needed.
     */
    InsertResult Move(const TEntry& value, const BoundingBox& aabb, bool allow_rebuild, EntrySet::Iterator it);

    HYP_FORCE_INLINE bool IsRoot() const
    {
        return m_parent == nullptr;
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_entries.Empty();
    }

    void SetParent(OctreeBase* parent);
    bool EmptyDeep(int depth = DEPTH_SEARCH_INF, uint8 octant_mask = 0xff) const;

    void InitOctants();
    void Divide();
    void Undivide();

    void Invalidate();

    /*! \brief If \ref{allow_rebuild} is true, removes any potentially empty octants above the entry.
        If \ref{allow_rebuild} is false, marks them as dirty so they get removed on the next call to PerformUpdates()
    */
    void CollapseParents(bool allow_rebuild);

    EntrySet m_entries;
    OctreeBase* m_parent;
    BoundingBox m_aabb;
    FixedArray<Octant, 8> m_octants;
    bool m_is_divided;
    OctreeState<Derived, TEntry>* m_state;
    OctantID m_octant_id;
    uint32 m_invalidation_marker;

private:
    InsertResult Insert_Internal(const TEntry& value, const BoundingBox& aabb);
    InsertResult Update_Internal(const TEntry& value, const BoundingBox& aabb, bool force_invalidation, bool allow_rebuild);
    Result Remove_Internal(const TEntry& value, bool allow_rebuild);
    InsertResult RebuildExtend_Internal(const BoundingBox& extend_include_aabb);
};

#include <util/octree/Octree.inl>

} // namespace hyperion

#endif
