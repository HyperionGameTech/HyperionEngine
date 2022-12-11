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

    Bitmask bits { 0u };
    Nonce nonce { 0u };

    VisibilityStateSnapshot()
    {
    }

    VisibilityStateSnapshot(const VisibilityStateSnapshot &other)
    {
        bits = other.bits;
        nonce = other.nonce;
    }

    VisibilityStateSnapshot &operator=(const VisibilityStateSnapshot &other)
    {
        bits = other.bits;
        nonce = other.nonce;

        return *this;
    }

    VisibilityStateSnapshot(VisibilityStateSnapshot &&other) noexcept = delete; /* deleted because atomics have no move constructor*/
    VisibilityStateSnapshot &operator=(VisibilityStateSnapshot &&other) noexcept = delete;
    ~VisibilityStateSnapshot() = default;

    HYP_FORCE_INLINE Bitmask Get(IDBase scene_id) const
        { return bits & (1ull << static_cast<Bitmask>(scene_id.value - 1)); }

    HYP_FORCE_INLINE void Set(IDBase scene_id, bool visible)
    {
        if (visible) {
            bits |= (1ull << static_cast<Bitmask>(scene_id.value - 1));
        } else {
            bits &= (~(1ull << (scene_id.value - 1)));
        }
    }

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityStateSnapshot &parent) const
        { return nonce == parent.nonce; }
};

struct VisibilityState
{
    using Bitmask = VisibilityStateSnapshot::Bitmask;
    using Nonce = VisibilityStateSnapshot::Nonce;

    static constexpr UInt max_scenes = sizeof(Bitmask) * CHAR_BIT;
    static constexpr UInt cursor_size = 3u;

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
            snapshot.bits = MathUtil::MaxSafeValue<Bitmask>();
            snapshot.nonce = MathUtil::MaxSafeValue<Nonce>();
        }
    }
};

} // namespace hyperion::v2

#endif