/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/ObjId.hpp>

#include <Core/Containers/Array.hpp>

namespace Hyperion {

class View;

HYP_STRUCT()
struct VisibilityStateSnapshot
{
    HYP_STRUCT_BODY(VisibilityStateSnapshot);

    uint16 validityMarker { 0u };

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityStateSnapshot& parent) const
    {
        return validityMarker == parent.validityMarker;
    }
};

HYP_STRUCT()
struct VisibilityState
{
    HYP_STRUCT_BODY(VisibilityState);

    Array<VisibilityStateSnapshot, InlineAllocator<8>> snapshots;
    uint16 validityMarker { 0u };

    VisibilityState() = default;

    VisibilityState(const VisibilityState& other) = default;
    VisibilityState& operator=(const VisibilityState& other) = default;

    VisibilityState(VisibilityState&& other) noexcept = default;
    VisibilityState& operator=(VisibilityState&& other) noexcept = default;

    ~VisibilityState() = default;

    HYP_FORCE_INLINE void Next()
    {
        ++validityMarker;
    }

    HYP_FORCE_INLINE VisibilityStateSnapshot GetSnapshot(ObjId<View> id) const
    {
        if (id.ToIndex() >= snapshots.Size())
        {
            return {};
        }

        return snapshots[id.ToIndex()];
    }

    HYP_FORCE_INLINE void MarkAsValid(ObjId<View> id)
    {
        if (id.ToIndex() >= snapshots.Size())
        {
            snapshots.Resize(id.ToIndex() + 1);
        }

        VisibilityStateSnapshot& snapshot = snapshots[id.ToIndex()];
        snapshot.validityMarker = validityMarker;
    }
};

} // namespace Hyperion
