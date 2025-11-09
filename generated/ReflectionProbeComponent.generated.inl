#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region ReflectionProbeComponent Reflection Data

HYP_BEGIN_STRUCT(ReflectionProbeComponent, 380, 0, {}, ClassAttribute("component", true),ClassAttribute("size", 24),ClassAttribute("label", "Reflection Probe Component"),ClassAttribute("description", "Handles cubemap reflection calculations for a single EnvProbe source"),ClassAttribute("editor", true))
    Field(NAME(HYP_STR(Dimensions)), &ReflectionProbeComponent::dimensions, offsetof(ReflectionProbeComponent, dimensions), Span<const ClassAttribute> { {ClassAttribute("property", "Dimensions"), ClassAttribute("editor", true), ClassAttribute("label", "Dimensions") } }),
    Field(NAME(HYP_STR(EnvProbe)), &ReflectionProbeComponent::envProbe, offsetof(ReflectionProbeComponent, envProbe), Span<const ClassAttribute> { {ClassAttribute("property", "EnvProbe"), ClassAttribute("editor", true), ClassAttribute("label", "EnvProbe") } }),
    Field(NAME(HYP_STR(ReflectionProbeRenderer)), &ReflectionProbeComponent::reflectionProbeRenderer, offsetof(ReflectionProbeComponent, reflectionProbeRenderer), Span<const ClassAttribute> { {ClassAttribute("property", "ReflectionProbeRenderer"), ClassAttribute("noscriptbindings", true), ClassAttribute("transient", true), ClassAttribute("editor", false) } })
HYP_END_STRUCT

#pragma endregion ReflectionProbeComponent Reflection Data

HYP_REGISTER_COMPONENT(ReflectionProbeComponent);
static_assert(sizeof(ReflectionProbeComponent) == 24, "Expected sizeof(ReflectionProbeComponent) to be 24 bytes");
} // namespace hyperion

