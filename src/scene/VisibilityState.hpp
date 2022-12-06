#ifndef HYPERION_V2_VISIBILITY_STATE_H
#define HYPERION_V2_VISIBILITY_STATE_H

#include <core/Containers.hpp>
#include <math/MathUtil.hpp>
#include <Util.hpp>
#include <Types.hpp>

#include <atomic>
#include <cstdint>

namespace hyperion::v2 {

struct VisibilityStateSnapshot
{
    using Bitmask = UInt64;
    using Nonce = UInt16;

    std::atomic<Bitmask> bits { 0u };
    std::atomic<Nonce> nonce { 0u };

    VisibilityStateSnapshot()
    {
    }

    VisibilityStateSnapshot(const VisibilityStateSnapshot &other)
    {
        bits.store(other.bits.load(std::memory_order_relaxed), std::memory_order_relaxed);
        nonce.store(other.nonce.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    VisibilityStateSnapshot &operator=(const VisibilityStateSnapshot &other)
    {
        bits.store(other.bits.load(std::memory_order_relaxed), std::memory_order_relaxed);
        nonce.store(other.nonce.load(std::memory_order_relaxed), std::memory_order_relaxed);

        return *this;
    }

    VisibilityStateSnapshot(VisibilityStateSnapshot &&other) noexcept = delete; /* deleted because atomics have no move constructor*/
    VisibilityStateSnapshot &operator=(VisibilityStateSnapshot &&other) noexcept = delete;
    ~VisibilityStateSnapshot() = default;

    HYP_FORCE_INLINE Bitmask Get(IDBase scene_id) const
        { return bits.load(std::memory_order_relaxed) & (1ull << static_cast<Bitmask>(scene_id.value - 1)); }

    HYP_FORCE_INLINE void Set(IDBase scene_id, bool visible)
    {
        if (visible) {
            bits.fetch_or(1ull << static_cast<Bitmask>(scene_id.value - 1), std::memory_order_relaxed);
        } else {
            bits.fetch_and(~(1ull << (scene_id.value - 1)), std::memory_order_relaxed);
        }
    }

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityStateSnapshot &parent) const
        { return nonce.load(std::memory_order_relaxed) == parent.nonce.load(std::memory_order_relaxed); }
};

struct VisibilityState
{
    using Bitmask = VisibilityStateSnapshot::Bitmask;
    using Nonce = VisibilityStateSnapshot::Nonce;

    static constexpr UInt max_scenes = sizeof(Bitmask) * CHAR_BIT;
    static constexpr UInt cursor_size = 3u;

    // /* map from scene index (id - 1) -> visibility boolean */
    // std::atomic<Bitmask> bits { 0u };

    // /* nonce is used to validate that the VisibilityState is still valid relative to
    //  * some parent VisibilityState. While the parent's nonce may have been set to a new
    //  * value, if ours has not, the state is considered to be invalid and thus not be
    //  * used as if it is in a "visible" state.
    //  */
    // std::array<std::atomic<Nonce>, cursor_size> nonces { };

    FixedArray<VisibilityStateSnapshot, cursor_size> snapshots { };

    VisibilityState() = default;
    VisibilityState(const VisibilityState &other) = delete;
    VisibilityState &operator=(const VisibilityState &other) = delete;
    VisibilityState(VisibilityState &&other) noexcept = delete; /* deleted because atomics have no move constructor*/
    VisibilityState &operator=(VisibilityState &&other) noexcept = delete;
    ~VisibilityState() = default;

    HYP_FORCE_INLINE bool Get(IDBase scene_id, UInt8 cursor) const
        { return snapshots[cursor].Get(scene_id); }

    HYP_FORCE_INLINE void SetVisible(IDBase scene_id, UInt8 cursor)
        { snapshots[cursor].Set(scene_id, true); }

    HYP_FORCE_INLINE void SetHidden(IDBase scene_id, UInt8 cursor)
        { snapshots[cursor].Set(scene_id, false); }

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityState &parent, UInt8 cursor) const
        { return snapshots[cursor].ValidToParent(parent.snapshots[cursor]); }

    void ForceAllVisible()
    {
        for (auto &snapshot : snapshots) {
            snapshot.bits.store(MathUtil::MaxSafeValue<Bitmask>(), std::memory_order_relaxed);
            snapshot.nonce.store(~0u, std::memory_order_relaxed);
        }
    }
};

} // namespace hyperion::v2

#endif