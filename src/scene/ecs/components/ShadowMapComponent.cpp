#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(ShadowMapComponent)
    HYP_FIELD(mode),
    HYP_FIELD(radius),
    HYP_FIELD(resolution),
    HYP_FIELD(render_component),
    HYP_FIELD(update_counter)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(ShadowMapComponent);

} // namespace hyperion