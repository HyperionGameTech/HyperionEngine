#include <scene/ecs/components/SkyComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(SkyComponent)
    HYP_FIELD(render_component)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(SkyComponent);

} // namespace hyperion