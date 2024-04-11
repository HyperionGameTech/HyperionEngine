#ifndef HYPERION_V2_ECS_VISIBILITY_STATE_COMPONENT_HPP
#define HYPERION_V2_ECS_VISIBILITY_STATE_COMPONENT_HPP

#include <core/lib/RefCountedPtr.hpp>

#include <scene/VisibilityState.hpp>
#include <scene/Octree.hpp>

namespace hyperion::v2 {

using VisibilityStateFlags = uint32;

enum VisibilityStateFlagBits : VisibilityStateFlags
{
    VISIBILITY_STATE_FLAG_NONE              = 0x0,
    VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE    = 0x1,
    VISIBILITY_STATE_FLAG_INVALIDATED       = 0x2
};

struct VisibilityStateComponent
{
    VisibilityStateFlags    flags = VISIBILITY_STATE_FLAG_NONE;

    OctantID                octant_id = OctantID::Invalid();
    VisibilityState         *visibility_state = nullptr;

    HashCode                last_aabb_hash;
};
static_assert(sizeof(VisibilityStateComponent) == 40, "VisibilityStateComponent must match C# struct size");

} // namespace hyperion::v2

#endif
