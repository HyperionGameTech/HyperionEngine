#ifndef HYPERION_V2_VISIBILITY_STATE_H
#define HYPERION_V2_VISIBILITY_STATE_H

#include <Util.hpp>
#include <Types.hpp>

#include <atomic>
#include <cstdint>

namespace hyperion::v2 {

struct VisibilityState
{
    using Bitmask = UInt64;
    using Nonce = UInt16;

    static constexpr UInt max_scenes = sizeof(Bitmask) * CHAR_BIT;
    static constexpr UInt cursor_size = 3u;

    /* map from scene index (id - 1) -> visibility boolean */
    std::atomic<Bitmask> bits { 0u };

    /* nonce is used to validate that the VisibilityState is still valid relative to
     * some parent VisibilityState. While the parent's nonce may have been set to a new
     * value, if ours has not, the state is considered to be invalid and thus not be
     * used as if it is in a "visible" state.
     */
    std::array<std::atomic<Nonce>, cursor_size> nonces { };

    VisibilityState()
    {
        for (auto &nonce : nonces) {
            nonce.store(0u, std::memory_order_relaxed);
        }
    }

    VisibilityState(const VisibilityState &other) = delete;
    VisibilityState &operator=(const VisibilityState &other) = delete;
    VisibilityState(VisibilityState &&other) noexcept = delete; /* deleted because atomics have no move constructor*/
    VisibilityState &operator=(VisibilityState &&other) noexcept = delete;
    ~VisibilityState() = default;

    HYP_FORCE_INLINE bool Get(IDBase scene_id) const
    {
        AssertThrow(scene_id.value - 1ull < max_scenes);
        
        return bits.load(std::memory_order_relaxed) & 1ull << static_cast<Bitmask>(scene_id.value - 1);
    }

    HYP_FORCE_INLINE void Set(IDBase scene_id, bool visible)
    {
        AssertThrow(scene_id.value - 1 < max_scenes);
        
        if (visible) {
            bits.fetch_or(1ull << static_cast<Bitmask>(scene_id.value - 1), std::memory_order_relaxed);
        } else {
            bits.fetch_and(~(1ull << (scene_id.value - 1)), std::memory_order_relaxed);
        }
    }

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityState &parent, UInt32 cursor) const
        { return nonces[cursor].load(std::memory_order_relaxed) == parent.nonces[cursor].load(std::memory_order_relaxed); }
};

} // namespace hyperion::v2

#endif