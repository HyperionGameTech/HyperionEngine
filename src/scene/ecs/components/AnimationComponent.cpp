#include <scene/ecs/components/AnimationComponent.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(AnimationComponent)
    HYP_FIELD(playback_state)
HYP_END_STRUCT

} // namespace hyperion