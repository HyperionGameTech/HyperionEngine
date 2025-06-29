/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_VISIBILITY_STATE_HPP
#define HYPERION_VISIBILITY_STATE_HPP

#include <core/Id.hpp>

#include <Types.hpp>

namespace hyperion {

class Camera;

struct VisibilityStateSnapshot
{
    uint16 validity_marker { 0u };

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityStateSnapshot& parent) const
    {
        return validity_marker == parent.validity_marker;
    }
};

struct VisibilityState
{
    uint16 validity_marker { 0u };
    HashMap<Id<Camera>, VisibilityStateSnapshot> snapshots;

    VisibilityState() = default;
    VisibilityState(const VisibilityState& other) = default;
    VisibilityState& operator=(const VisibilityState& other) = default;
    VisibilityState(VisibilityState&& other) noexcept = default;
    VisibilityState& operator=(VisibilityState&& other) noexcept = default;
    ~VisibilityState() = default;

    HYP_FORCE_INLINE void Next()
    {
        ++validity_marker;
    }

    HYP_FORCE_INLINE VisibilityStateSnapshot GetSnapshot(Id<Camera> id) const
    {
        auto it = snapshots.Find(id);

        if (it == snapshots.End())
        {
            return {};
        }

        return it->second;
    }

    HYP_FORCE_INLINE void MarkAsValid(Id<Camera> id)
    {
        auto it = snapshots.Find(id);

        if (it == snapshots.End())
        {
            snapshots.Insert(id, VisibilityStateSnapshot { validity_marker });
        }
        else
        {
            it->second.validity_marker = validity_marker;
        }
    }
};

} // namespace hyperion

#endif