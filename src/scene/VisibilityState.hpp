/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/ObjId.hpp>

#include <core/containers/Array.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Camera;

struct VisibilityStateSnapshot
{
    uint16 validityMarker { 0u };

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityStateSnapshot& parent) const
    {
        return validityMarker == parent.validityMarker;
    }
};

struct VisibilityState
{
    //static_assert(std::is_final_v<Camera>, "ObjId<Camera> must be final (to prevent ID index issues with derived types)");
    
    Array<VisibilityStateSnapshot, InlineAllocator<16>> snapshots;
    uint16 validityMarker { 0u };

    VisibilityState() = default;
    VisibilityState(const VisibilityState& other) = delete;
    VisibilityState& operator=(const VisibilityState& other) = delete;
    VisibilityState(VisibilityState&& other) noexcept = default;
    VisibilityState& operator=(VisibilityState&& other) noexcept = default;
    ~VisibilityState() = default;

    HYP_FORCE_INLINE void Next()
    {
        ++validityMarker;
    }

    HYP_FORCE_INLINE VisibilityStateSnapshot GetSnapshot(ObjId<Camera> id) const
    {
        if (id.ToIndex() >= snapshots.Size())
        {
            return {};
        }

        return snapshots[id.ToIndex()];
    }

    HYP_FORCE_INLINE void MarkAsValid(ObjId<Camera> id)
    {
        if (id.ToIndex() >= snapshots.Size())
        {
            snapshots.Resize(id.ToIndex() + 1);
        }

        VisibilityStateSnapshot& snapshot = snapshots[id.ToIndex()];
        snapshot.validityMarker = validityMarker;
    }
};

} // namespace hyperion
