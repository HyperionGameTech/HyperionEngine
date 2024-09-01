#include <scene/ecs/components/LightComponent.hpp>
#include <scene/ecs/ComponentInterface.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(LightComponent)
    HYP_FIELD(light),
    HYP_FIELD(transform_hash_code),
    HYP_FIELD(flags)
HYP_END_STRUCT

HYP_REGISTER_COMPONENT(LightComponent);

} // namespace hyperion