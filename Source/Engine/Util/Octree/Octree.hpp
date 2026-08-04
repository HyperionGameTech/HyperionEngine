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
class OctreeState
{
public:
    struct DirtyState
    {
        OctantId octantId = OctantId::Invalid();
        bool needsRebuild = false;
    };

    OctreeState() = default;
    OctreeState(const OctreeState& other) = delete;

    OctreeState& operator=(const OctreeState& other) = delete;

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

protected:
    ~OctreeState() = default;
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

    template <class AllocatorType>
    void Clear(Array<Payload, AllocatorType>& outPayloads, bool undivide);

    Result Insert(const Payload& payload, const BoundingBox& aabb);

    bool GetNearestOctants(const Vec3f& position, FixedArray<Derived*, 8>& out) const;
    bool GetNearestOctant(const Vec3f& position, Derived const*& out) const;
    bool GetFittingOctant(const BoundingBox& aabb, Derived const*& out) const;

    HYP_FORCE_INLINE OctreeState<Derived, Payload>* GetState() const
    {
        return m_state;
    }

protected:
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

template <class Derived, class Payload>
const BoundingBox OctreeBase<Derived, Payload>::DefaultBounds = BoundingBox({ -250.0f }, { 250.0f });

template <class Derived, class Payload>
void OctreeState<Derived, Payload>::MarkOctantDirty(OctantId octantId, bool needsRebuild)
{
    const DirtyState prevDirtyState = dirtyState;

    if (octantId.IsInvalid())
    {
        return;
    }

    if (dirtyState.octantId.IsInvalid())
    {
        dirtyState = { octantId, needsRebuild };

        return;
    }

    while (octantId != dirtyState.octantId && !octantId.IsChildOf(dirtyState.octantId) && !dirtyState.octantId.IsInvalid())
    {
        dirtyState.octantId = dirtyState.octantId.GetParent();
    }

    // should always end up at root if it doesnt match any
    AssertDebug(dirtyState.octantId != OctantId::Invalid());
    dirtyState.needsRebuild |= needsRebuild;
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::OctreeBase()
    : OctreeBase(Derived::DefaultBounds)
{
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::OctreeBase(const BoundingBox& aabb)
    : OctreeBase(nullptr, aabb, 0)
{
    m_state = new typename Derived::DerivedOctreeState;
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::OctreeBase(Derived* parent, const BoundingBox& aabb, uint8 index)
    : m_parent(nullptr),
      m_aabb(aabb),
      m_isDivided(false),
      m_state(nullptr),
      m_octantId(index, OctantId::Invalid()),
      m_invalidationMarker(0),
      m_payload {}
{
    if (parent != nullptr)
    {
        SetParent(parent); // call explicitly to set root ptr
    }

    Assert(m_octantId.GetIndex() == index);

    InitOctants();
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::~OctreeBase()
{
    if (IsRoot())
    {
        delete static_cast<typename Derived::DerivedOctreeState*>(m_state);
    }

    for (Octant& octant : m_octants)
    {
        if (octant.octree != nullptr)
        {
            delete octant.octree;
            octant.octree = nullptr;
        }
    }
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::SetParent(Derived* parent)
{
    m_parent = parent;

    if (m_parent != nullptr)
    {
        m_state = m_parent->m_state;
    }
    else
    {
        m_state = nullptr;
    }

    m_octantId = OctantId(m_octantId.GetIndex(), parent != nullptr ? parent->m_octantId : OctantId::Invalid());

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->SetParent(static_cast<Derived*>(this));
        }
    }
}

template <class Derived, class Payload>
bool OctreeBase<Derived, Payload>::EmptyDeep(int depth, uint8 octantMask) const
{
    if (!Empty())
    {
        return false;
    }

    if (!m_isDivided)
    {
        return true;
    }

    if (depth != 0)
    {
        return Every(m_octants, [depth, octantMask](const Octant& octant)
            {
                if (octantMask & (1u << octant.octree->m_octantId.GetIndex()))
                {
                    return octant.octree->EmptyDeep(depth - 1);
                }

                return true;
            });
    }

    return true;
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::InitOctants()
{
    const Vec3f dividedAabbDimensions = m_aabb.GetExtent() / 2.0f;

    for (uint32 x = 0; x < 2; x++)
    {
        for (uint32 y = 0; y < 2; y++)
        {
            for (uint32 z = 0; z < 2; z++)
            {
                const uint32 index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    .aabb = BoundingBox(
                        m_aabb.GetMin() + dividedAabbDimensions * Vec3f(float(x), float(y), float(z)),
                        m_aabb.GetMin() + dividedAabbDimensions * (Vec3f(float(x), float(y), float(z)) + Vec3f(1.0f)))
                };
            }
        }
    }
}

template <class Derived, class Payload>
Derived* OctreeBase<Derived, Payload>::GetChildOctant(OctantId octantId)
{
    if (octantId == OctantId::Invalid())
    {
        return nullptr;
    }

    if (octantId == m_octantId)
    {
        return static_cast<Derived*>(this);
    }

    if (octantId.depth <= m_octantId.depth)
    {
        return nullptr;
    }

    Derived* current = static_cast<Derived*>(this);

    for (uint32 depth = m_octantId.depth + 1; depth <= octantId.depth; depth++)
    {
        const uint8 index = octantId.GetIndex(depth);

        if (!current || !current->IsDivided())
        {
            return nullptr;
        }

        current = current->m_octants[index].octree;
    }

    return current;
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::Divide()
{
    Assert(!IsDivided());

    for (int i = 0; i < 8; i++)
    {
        Octant& octant = m_octants[i];
        Assert(octant.octree == nullptr);

        octant.octree = static_cast<Derived*>(this)->CreateChildOctant(octant.aabb, uint8(i));
    }

    m_isDivided = true;
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::Undivide()
{
    Assert(IsDivided());
    Assert(m_payload.Empty(), "Undivide() should be called on octrees with no payload");

    for (Octant& octant : m_octants)
    {
        Assert(octant.octree != nullptr);

        if (octant.octree->IsDivided())
        {
            octant.octree->Undivide();
        }

        delete octant.octree;
        octant.octree = nullptr;
    }

    m_isDivided = false;
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::Invalidate()
{
    m_invalidationMarker++;

    if (IsDivided())
    {
        for (Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            octant.octree->Invalidate();
        }
    }
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::CollapseParents(bool allowRebuild)
{
    if (IsDivided() || !Empty())
    {
        return;
    }

    Derived* iteration = m_parent;
    Derived* highestEmpty = nullptr;

    while (iteration != nullptr && iteration->Empty())
    {
        for (const Octant& octant : iteration->m_octants)
        {
            Assert(octant.octree != nullptr);

            if (octant.octree == highestEmpty)
            {
                /* Do not perform a check on our entry, as we've already
                 * checked it by going up the chain.
                 * As `iter` becomes the parent of this entry we're currently working with,
                 * we will not have to perform duplicate EmptyDeep() checks on any octants because of this check. */
                continue;
            }

            if (!octant.octree->EmptyDeep())
            {
                goto undivide;
            }
        }

        highestEmpty = iteration;
        iteration = iteration->m_parent;
    }

undivide:
    if (highestEmpty != nullptr)
    {
        if (allowRebuild)
        {
            highestEmpty->Undivide();
        }
        else
        {
            m_state->MarkOctantDirty(highestEmpty->GetOctantID(), true);
        }
    }
}

template <class Derived, class Payload>
void OctreeBase<Derived, Payload>::Clear()
{
    Array<Payload> payloads;
    Clear(payloads, /* undivide */ true);
}

template <class Derived, class Payload>
template <class AllocatorType>
void OctreeBase<Derived, Payload>::Clear(Array<Payload, AllocatorType>& outPayloads, bool undivide)
{
    outPayloads.Reserve(outPayloads.Size() + 1);

    if (!m_payload.Empty())
    {
        outPayloads.PushBack(std::move(m_payload));
        m_payload = Payload {};
    }

    if (!m_isDivided)
    {
        return;
    }

    for (Octant& octant : m_octants)
    {
        Assert(octant.octree != nullptr);

        octant.octree->Clear(outPayloads, /* undivide */ false);
    }

    if (undivide && IsDivided())
    {
        Undivide();
    }
}

template <class Derived, class Payload>
OctreeBase<Derived, Payload>::Result OctreeBase<Derived, Payload>::Insert(const Payload& payload, const BoundingBox& aabb)
{
    if (Derived::Flags & OF_INSERT_ON_OVERLAP)
    {
        AssertDebug(aabb.IsValid() && aabb.IsFinite() && !aabb.IsZero(), "Attempting to insert invalid AABB into octree: {}", aabb);
    }

    if (aabb.IsValid() && aabb.IsFinite())
    {
        if (!m_aabb.Overlaps(aabb))
        {
            return HYP_MAKE_ERROR(Error, "Entry AABB outside of octant AABB");
        }

        // stop recursing if we are at max depth
        if (m_octantId.GetDepth() < int(static_cast<Derived*>(this)->MaxDepth()) - 1)
        {
            bool wasInserted = false;

            for (Octant& octant : m_octants)
            {
                if (!((Derived::Flags & OF_INSERT_ON_OVERLAP) ? octant.aabb.Overlaps(aabb) : octant.aabb.Contains(aabb)))
                {
                    continue;
                }

                if (!IsDivided())
                {
                    Divide();
                }

                Assert(octant.octree != nullptr);

                Result insertResult = octant.octree->Insert(payload, aabb);
                wasInserted |= bool(insertResult.HasValue());

                if (Derived::Flags & OF_INSERT_ON_OVERLAP)
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

    m_state->MarkOctantDirty(m_octantId);

    m_payload = payload;

    return m_octantId;
}

template <class Derived, class Payload>
bool OctreeBase<Derived, Payload>::GetNearestOctants(const Vec3f& position, FixedArray<Derived*, 8>& out) const
{
    if (!m_aabb.ContainsPoint(position))
    {
        return false;
    }

    if (!m_isDivided)
    {
        return false;
    }

    for (const Octant& octant : m_octants)
    {
        Assert(octant.octree != nullptr);

        if (octant.octree->GetNearestOctants(position, out))
        {
            return true;
        }
    }

    for (size_t i = 0; i < m_octants.Size(); i++)
    {
        out[i] = m_octants[i].octree;
    }

    return true;
}

template <class Derived, class Payload>
bool OctreeBase<Derived, Payload>::GetNearestOctant(const Vec3f& position, Derived const*& out) const
{
    if (!m_aabb.ContainsPoint(position))
    {
        return false;
    }

    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            if (octant.octree->GetNearestOctant(position, out))
            {
                return true;
            }
        }
    }

    out = static_cast<const Derived*>(this);

    return true;
}

template <class Derived, class Payload>
bool OctreeBase<Derived, Payload>::GetFittingOctant(const BoundingBox& aabb, Derived const*& out) const
{
    if (m_isDivided)
    {
        for (const Octant& octant : m_octants)
        {
            Assert(octant.octree != nullptr);

            if (octant.octree->GetFittingOctant(aabb, out))
            {
                return true;
            }
        }
    }

    // out should always be set to something, boolean indicates whether or not it was actually contained
    out = static_cast<const Derived*>(this);

    return ContainsAabb(aabb);
}

} // namespace Hyperion
