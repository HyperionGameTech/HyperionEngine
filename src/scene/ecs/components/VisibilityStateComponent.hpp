#ifndef HYPERION_V2_ECS_VISIBILITY_STATE_COMPONENT_HPP
#define HYPERION_V2_ECS_VISIBILITY_STATE_COMPONENT_HPP

#include <scene/VisibilityState.hpp>
#include <scene/Octree.hpp>

namespace hyperion::v2 {

using VisibilityStateFlags = UInt32;

enum VisibilityStateFlagBits : VisibilityStateFlags
{
    VISIBILITY_STATE_FLAG_NONE              = 0x0,
    VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE    = 0x1
};

struct VisibilityStateComponent
{
    VisibilityStateFlags flags = VISIBILITY_STATE_FLAG_NONE;

    VisibilityState visibility_state;
    OctantID        octant_id = OctantID::invalid;

    HashCode        last_aabb_hash;
};

} // namespace hyperion::v2

#endif