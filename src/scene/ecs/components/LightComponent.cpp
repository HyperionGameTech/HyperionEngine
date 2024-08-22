#include <scene/ecs/components/LightComponent.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_BEGIN_STRUCT(LightComponent)
    HYP_FIELD(light),
    HYP_FIELD(transform_hash_code),
    HYP_FIELD(flags),

    HYP_PROPERTY(Light, &LightComponent::GetLight, &LightComponent::SetLight)
HYP_END_STRUCT

} // namespace hyperion