#include <scene/ecs/components/LightComponent.hpp>

#include <rendering/backend/RendererFramebuffer.hpp>

#include <core/HypClassUtils.hpp>

namespace hyperion {

HYP_DEFINE_CLASS(
    LightComponent,
    HypClassProperty(NAME("Light"), &LightComponent::GetLight, &LightComponent::SetLight)
);

} // namespace hyperion