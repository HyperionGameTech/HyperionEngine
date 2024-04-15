/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_VISIBILITY_STATE_H
#define HYPERION_V2_VISIBILITY_STATE_H

#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>

#include <atomic>
#include <cstdint>

namespace hyperion::v2 {

class Camera;

struct VisibilityStateSnapshot
{
    uint16 validity_marker { 0u };

    VisibilityStateSnapshot()                                                       = default;
    VisibilityStateSnapshot(const VisibilityStateSnapshot &other)                   = default;
    VisibilityStateSnapshot &operator=(const VisibilityStateSnapshot &other)        = default;
    VisibilityStateSnapshot(VisibilityStateSnapshot &&other) noexcept               = default;
    VisibilityStateSnapshot &operator=(VisibilityStateSnapshot &&other) noexcept    = default;
    ~VisibilityStateSnapshot()                                                      = default;

    HYP_FORCE_INLINE
    bool ValidToParent(const VisibilityStateSnapshot &parent) const
        { return validity_marker == parent.validity_marker; }
};

struct VisibilityState
{
    HashMap<ID<Camera>, VisibilityStateSnapshot>    snapshots;

    VisibilityState()                                               = default;
    VisibilityState(const VisibilityState &other)                   = default;
    VisibilityState &operator=(const VisibilityState &other)        = default;
    VisibilityState(VisibilityState &&other) noexcept               = default;
    VisibilityState &operator=(VisibilityState &&other) noexcept    = default;
    ~VisibilityState()                                              = default;

    /*! \brief Advances the validity marker of all snapshots. */
    void Next()
    {
        for (auto &snapshot : snapshots) {
            snapshot.second.validity_marker++;
        }
    }

    HYP_FORCE_INLINE
    VisibilityStateSnapshot &GetSnapshot(ID<Camera> id)
    {
        auto it = snapshots.Find(id);

        if (it == snapshots.End()) {
            it = snapshots.Insert(id, VisibilityStateSnapshot()).first;
        }

        return it->second;
    }

    HYP_FORCE_INLINE
    void SetSnapshot(ID<Camera> id, uint16 validity_marker)
    {
        auto it = snapshots.Find(id);

        if (it == snapshots.End()) {
            it = snapshots.Insert(id, VisibilityStateSnapshot()).first;
        }

        it->second.validity_marker = validity_marker;
    }
};

} // namespace hyperion::v2

#endif