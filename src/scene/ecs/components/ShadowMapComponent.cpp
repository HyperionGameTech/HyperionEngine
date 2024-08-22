#include <scene/ecs/components/ShadowMapComponent.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(ShadowMapComponent)
    HYP_FIELD(mode),
    HYP_FIELD(radius),
    HYP_FIELD(resolution),
    HYP_FIELD(render_component),
    HYP_FIELD(update_counter)
HYP_END_STRUCT

} // namespace hyperion