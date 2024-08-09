#include <scene/ecs/components/LightComponent.hpp>

#include <core/object/HypClassUtils.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(
    LightComponent,
    HypProperty(NAME("Light"), &LightComponent::GetLight, &LightComponent::SetLight)
);

} // namespace hyperion