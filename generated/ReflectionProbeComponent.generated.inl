#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>
#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region ReflectionProbeComponent Reflection Data

HYP_BEGIN_STRUCT(ReflectionProbeComponent, 413, 0, {}, HypClassAttribute("component", true),HypClassAttribute("size", 24),HypClassAttribute("label", "Reflection Probe Component"),HypClassAttribute("description", "Handles cubemap reflection calculations for a single EnvProbe source"),HypClassAttribute("editor", true))
    HypField(NAME(HYP_STR(Dimensions)), &ReflectionProbeComponent::dimensions, offsetof(ReflectionProbeComponent, dimensions), Span<const HypClassAttribute> { {HypClassAttribute("property", "Dimensions"), HypClassAttribute("editor", true), HypClassAttribute("label", "Dimensions") } }),
    HypField(NAME(HYP_STR(EnvProbe)), &ReflectionProbeComponent::envProbe, offsetof(ReflectionProbeComponent, envProbe), Span<const HypClassAttribute> { {HypClassAttribute("property", "EnvProbe"), HypClassAttribute("editor", true), HypClassAttribute("label", "EnvProbe") } }),
    HypField(NAME(HYP_STR(ReflectionProbeRenderer)), &ReflectionProbeComponent::reflectionProbeRenderer, offsetof(ReflectionProbeComponent, reflectionProbeRenderer), Span<const HypClassAttribute> { {HypClassAttribute("property", "ReflectionProbeRenderer"), HypClassAttribute("noscriptbindings", true), HypClassAttribute("transient", true), HypClassAttribute("editor", false) } })
HYP_END_STRUCT

#pragma endregion ReflectionProbeComponent Reflection Data

HYP_REGISTER_COMPONENT(ReflectionProbeComponent);
static_assert(sizeof(ReflectionProbeComponent) == 24, "Expected sizeof(ReflectionProbeComponent) to be 24 bytes");
} // namespace hyperion

