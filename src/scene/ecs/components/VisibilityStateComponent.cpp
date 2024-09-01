#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(VisibilityStateComponent)
    HYP_FIELD(flags),
    HYP_FIELD(octant_id),
    HYP_FIELD(visibility_state),
    HYP_FIELD(last_aabb_hash)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(VisibilityStateComponent);

} // namespace hyperion