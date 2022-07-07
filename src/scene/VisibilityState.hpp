#ifndef HYPERION_V2_VISIBILITY_STATE_H
#define HYPERION_V2_VISIBILITY_STATE_H

#include <Util.hpp>
#include <Types.hpp>

#include <atomic>
#include <cstdint>

namespace hyperion::v2 {

struct VisibilityState {
    using Bitmask = UInt64;
    using Nonce   = UInt16;

    static constexpr UInt32 max_scenes  = sizeof(Bitmask) * CHAR_BIT;
    static constexpr UInt32 cursor_size = 4;

    /* map from scene index (id - 1) -> visibility boolean
     */
    std::atomic<Bitmask> bits{0};

    /* nonce is used to validate that the VisibilityState is still valid relative to
     * some parent VisibilityState. While the parent's nonce may have been set to a new
     * value, if ours has not, the state is considered to be invalid and thus not be
     * used as if it is in a "visible" state.
     */
    std::array<std::atomic<Nonce>, cursor_size> nonces{};

    VisibilityState()
    {
        for (auto &nonce : nonces) {
            nonce.store(0);
        }
    }

    VisibilityState(const VisibilityState &other)                = delete;
    VisibilityState &operator=(const VisibilityState &other)     = delete;
    VisibilityState(VisibilityState &&other) noexcept            = delete; /* deleted because atomics have no move constructor*/
    VisibilityState &operator=(VisibilityState &&other) noexcept = delete;
    ~VisibilityState() = default;

    HYP_FORCE_INLINE bool Get(IDBase scene_id) const
    {
        AssertThrow(scene_id.value - 1ull < max_scenes);
        
        return bits & 1ull << static_cast<Bitmask>(scene_id.value - 1);
    }

    HYP_FORCE_INLINE void Set(IDBase scene_id, bool visible)
    {
        AssertThrow(scene_id.value - 1 < max_scenes);
        
        if (visible) {
            bits |= 1ull << static_cast<Bitmask>(scene_id.value - 1);
        } else {
            bits &= ~(1ull << (scene_id.value - 1));
        }
    }

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityState &parent, UInt32 cursor) const
        { return nonces[cursor] == parent.nonces[cursor]; }
};

} // namespace hyperion::v2

#endif