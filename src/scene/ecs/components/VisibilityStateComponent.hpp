#ifndef HYPERION_V2_ECS_VISIBILITY_STATE_COMPONENT_HPP
#define HYPERION_V2_ECS_VISIBILITY_STATE_COMPONENT_HPP

#include <scene/VisibilityState.hpp>
#include <scene/Octree.hpp>

namespace hyperion::v2 {

struct VisibilityStateComponent
{
    VisibilityState visibility_state;
    OctantID        octant_id = OctantID::invalid;

    HashCode        last_aabb_hash;
};

} // namespace hyperion::v2

#endif