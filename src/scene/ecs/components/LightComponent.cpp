#include <scene/ecs/components/LightComponent.hpp>

#include <core/HypClassUtils.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(
    LightComponent,
    HypClassProperty(NAME("Light"), &LightComponent::GetLight, &LightComponent::SetLight)
);

} // namespace hyperion